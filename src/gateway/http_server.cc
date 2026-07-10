#include "tgw/gateway/http_server.h"

#include "tgw/common/logger.h"
#include "tgw/gateway/http_types.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace tgw {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;

using tcp = asio::ip::tcp;

namespace {

std::string ToString(beast::string_view value) {
    return std::string(value.data(), value.size());
}

void ParseTarget(const std::string& target, std::string* path, std::string* query) {
    auto pos = target.find('?');
    if (pos == std::string::npos) {
        *path = target;
        *query = "";
        return;
    }

    *path = target.substr(0, pos);
    *query = target.substr(pos + 1);
}

HttpRequest ConvertBeastRequest(
    const http::request<http::string_body>& req,
    const std::string& client_ip,
    const std::string& request_id
) {
    HttpRequest tgw_req;
    tgw_req.method = ToString(req.method_string());

    std::string target = ToString(req.target());
    ParseTarget(target, &tgw_req.path, &tgw_req.query);

    tgw_req.body = req.body();
    tgw_req.client_ip = client_ip;
    tgw_req.request_id = request_id;

    for (const auto& field : req.base()) {
        tgw_req.headers[ToString(field.name_string())] = ToString(field.value());
    }

    return tgw_req;
}

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(
        tcp::socket socket,
        ServerConfig server_config,
        std::shared_ptr<Router> router,
        asio::thread_pool& worker_pool,
        std::function<std::string()> request_id_generator
    )
        : socket_(std::move(socket)),
          read_timer_(socket_.get_executor()),
          write_timer_(socket_.get_executor()),
          server_config_(std::move(server_config)),
          router_(std::move(router)),
          worker_pool_(worker_pool),
          request_id_generator_(std::move(request_id_generator)) {}

    void Start() {
        ReadRequest();
    }

private:
    void ReadRequest() {
        parser_ = std::make_unique<http::request_parser<http::string_body>>();
        parser_->body_limit(server_config_.body_limit_bytes);

        read_timer_.expires_after(std::chrono::milliseconds(server_config_.read_timeout_ms));
        auto self = shared_from_this();
        read_timer_.async_wait([self](beast::error_code ec) {
            if (!ec) {
                TGW_WARN("read timeout, closing connection");
                self->Close();
            }
        });

        http::async_read(
            socket_,
            buffer_,
            *parser_,
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                (void)bytes_transferred;
                self->read_timer_.cancel();

                if (ec) {
                    if (ec != http::error::end_of_stream) {
                        TGW_WARN("async read request failed: {}", ec.message());
                    }
                    self->Close();
                    return;
                }

                self->request_ = self->parser_->release();
                self->parser_.reset();
                self->DispatchRequest();
            }
        );
    }

    void DispatchRequest() {
        auto self = shared_from_this();
        asio::post(worker_pool_, [self]() {
            self->request_id_ = self->request_id_generator_();

            auto req_id_header = self->request_.find("X-Request-Id");
            if (req_id_header != self->request_.end()) {
                self->request_id_ = ToString(req_id_header->value());
            }

            std::string client_ip = "unknown";
            beast::error_code remote_ec;
            auto remote_endpoint = self->socket_.remote_endpoint(remote_ec);
            if (!remote_ec) {
                client_ip = remote_endpoint.address().to_string();
            }

            HttpRequest tgw_req = ConvertBeastRequest(
                self->request_,
                client_ip,
                self->request_id_
            );
            HttpResponse tgw_rsp = self->router_->Dispatch(tgw_req);

            asio::post(
                self->socket_.get_executor(),
                [self, tgw_rsp = std::move(tgw_rsp)]() mutable {
                    self->WriteResponse(std::move(tgw_rsp));
                }
            );
        });
    }

    void WriteResponse(HttpResponse tgw_rsp) {
        response_ = std::make_shared<http::response<http::string_body>>(
            static_cast<http::status>(tgw_rsp.status),
            request_.version()
        );

        response_->set(http::field::server, "tgw-gateway");
        response_->set(http::field::content_type, tgw_rsp.content_type);
        response_->set("X-Request-Id", request_id_);

        for (const auto& header : tgw_rsp.headers) {
            response_->set(header.first, header.second);
        }

        response_->keep_alive(server_config_.keep_alive && request_.keep_alive());
        response_->body() = std::move(tgw_rsp.body);
        response_->prepare_payload();

        write_timer_.expires_after(std::chrono::milliseconds(server_config_.write_timeout_ms));
        auto self = shared_from_this();
        write_timer_.async_wait([self](beast::error_code ec) {
            if (!ec) {
                TGW_WARN("write timeout, closing connection");
                self->Close();
            }
        });

        http::async_write(
            socket_,
            *response_,
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                self->write_timer_.cancel();

                if (ec) {
                    TGW_WARN("async write response failed: {}", ec.message());
                    self->Close();
                    return;
                } else {
                    TGW_INFO(
                        "request finished, request_id={}, status={}, response_bytes={}",
                        self->request_id_,
                        static_cast<unsigned>(self->response_->result_int()),
                        bytes_transferred
                    );
                }

                bool keep_alive = self->response_->keep_alive();
                self->response_.reset();
                self->request_ = {};
                self->request_id_.clear();

                if (keep_alive) {
                    self->ReadRequest();
                    return;
                }

                self->Close();
            }
        );
    }

    void Close() {
        beast::error_code ec;
        read_timer_.cancel(ec);
        write_timer_.cancel(ec);
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        socket_.close(ec);
    }

private:
    tcp::socket socket_;
    asio::steady_timer read_timer_;
    asio::steady_timer write_timer_;
    ServerConfig server_config_;
    std::shared_ptr<Router> router_;
    asio::thread_pool& worker_pool_;
    std::function<std::string()> request_id_generator_;

    beast::flat_buffer buffer_;
    std::unique_ptr<http::request_parser<http::string_body>> parser_;
    http::request<http::string_body> request_;
    std::shared_ptr<http::response<http::string_body>> response_;
    std::string request_id_;
};

} // namespace

HttpServer::HttpServer(
    ServerConfig server_config,
    RuntimeConfig runtime_config,
    std::shared_ptr<Router> router
)
    : server_config_(std::move(server_config)),
      runtime_config_(std::move(runtime_config)),
      router_(std::move(router)),
      io_context_(std::max(runtime_config.io_threads, 1)),
      acceptor_(io_context_),
      worker_pool_(std::max(runtime_config.worker_threads, 1)) {}

void HttpServer::Start() {
    auto address = asio::ip::make_address(server_config_.host);
    tcp::endpoint endpoint(address, server_config_.port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);

    TGW_INFO(
        "async http server listening on {}:{}, io_threads={}, worker_threads={}",
        server_config_.host,
        server_config_.port,
        std::max(runtime_config_.io_threads, 1),
        std::max(runtime_config_.worker_threads, 1)
    );

    DoAccept();

    asio::signal_set signals(io_context_, SIGINT, SIGTERM);
    signals.async_wait([this](beast::error_code ec, int signal_number) {
        if (ec) {
            return;
        }

        TGW_INFO("received signal {}, shutting down http server", signal_number);

        beast::error_code close_ec;
        acceptor_.close(close_ec);
        io_context_.stop();
    });

    auto work_guard = asio::make_work_guard(io_context_);
    std::vector<std::thread> io_threads;
    int thread_count = std::max(runtime_config_.io_threads, 1);
    io_threads.reserve(static_cast<size_t>(thread_count));

    for (int i = 0; i < thread_count; ++i) {
        io_threads.emplace_back([this]() {
            io_context_.run();
        });
    }

    for (auto& thread : io_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void HttpServer::DoAccept() {
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (ec) {
            if (acceptor_.is_open()) {
                TGW_ERROR("async accept connection failed: {}", ec.message());
            }
        } else {
            auto session = std::make_shared<HttpSession>(
                std::move(socket),
                server_config_,
                router_,
                worker_pool_,
                [this]() {
                    return GenerateRequestId();
                }
            );
            session->Start();
        }

        if (acceptor_.is_open()) {
            DoAccept();
        }
    });
}

std::string HttpServer::GenerateRequestId() {
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    auto seq = request_seq_.fetch_add(1);

    std::ostringstream oss;
    oss << "tgw-" << now << "-" << seq;

    return oss.str();
}

} // namespace tgw
