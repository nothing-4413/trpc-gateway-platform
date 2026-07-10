#include "tgw/gateway/http_server.h"

#include "tgw/common/logger.h"
#include "tgw/gateway/http_types.h"

#include <chrono>
#include <sstream>
#include <string>

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

} // namespace

HttpServer::HttpServer(ServerConfig server_config,
    RuntimeConfig runtime_config,
    std::shared_ptr<Router> router)
    : server_config_(std::move(server_config)),
      runtime_config_(std::move(runtime_config)),
      router_(std::move(router)),
      worker_pool_(runtime_config_.worker_threads) {}

void HttpServer::Start() {
    asio::io_context ioc;
    auto address = asio::ip::make_address(server_config_.host);
    tcp::endpoint endpoint(address, server_config_.port);

    tcp::acceptor acceptor(ioc);

    acceptor.open(endpoint.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(asio::socket_base::max_listen_connections);

    TGW_INFO("http server listening on {}:{}", server_config_.host, server_config_.port);

    while (true) {
        auto socket = std::make_shared<tcp::socket>(ioc);

        beast::error_code ec;
        acceptor.accept(*socket, ec);

        if (ec) {
            TGW_ERROR("accept connection failed: {}", ec.message());
            continue;
        }

        asio::post(worker_pool_, [this, socket]() {
            HandleSession(std::static_pointer_cast<void>(socket));
        });
    }
}

void HttpServer::HandleSession(std::shared_ptr<void> socket_holder) {
    auto socket = std::static_pointer_cast<tcp::socket>(socket_holder);

    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(*socket, buffer, req);

        std::string client_ip = "unknown";
        beast::error_code remote_ec;
        auto remote_endpoint = socket->remote_endpoint(remote_ec);
        if (!remote_ec) {
            client_ip = remote_endpoint.address().to_string();
        }

        std::string request_id = GenerateRequestId();

        auto req_id_header = req.find("X-Request-Id");
        if (req_id_header != req.end()) {
            request_id = ToString(req_id_header->value());
        }

        HttpRequest tgw_req = ConvertBeastRequest(req, client_ip, request_id);
        HttpResponse tgw_rsp = router_->Dispatch(tgw_req);

        http::response<http::string_body> res{
            static_cast<http::status>(tgw_rsp.status),
            req.version()
        };

        res.set(http::field::server, "tgw-gateway");
        res.set(http::field::content_type, tgw_rsp.content_type);
        res.set("X-Request-Id", request_id);

        for (const auto& header : tgw_rsp.headers) {
            res.set(header.first, header.second);
        }

        res.keep_alive(false);
        res.body() = tgw_rsp.body;
        res.prepare_payload();

        http::write(*socket, res);

        beast::error_code ec;
        socket->shutdown(tcp::socket::shutdown_send, ec);

        TGW_INFO(
            "request finished, request_id={}, status={}, response_bytes={}",
            request_id,
            tgw_rsp.status,
            tgw_rsp.body.size()
        );
    } catch (const std::exception& e) {
        TGW_ERROR("handle session exception: {}", e.what());
    }
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
