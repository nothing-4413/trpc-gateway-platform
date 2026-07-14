#include "tgw/common/config.h"
#include "tgw/common/logger.h"
#include "tgw/common/deadline.h"
#include "tgw/service/user_service.h"

#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

namespace {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

struct ServerOptions {
    std::string host = "0.0.0.0";
    uint16_t port = 9001;
};

ServerOptions ParseOptions(int argc, char* argv[]) {
    ServerOptions options;

    const std::string host_prefix = "--host=";
    const std::string port_prefix = "--port=";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(host_prefix, 0) == 0) {
            options.host = arg.substr(host_prefix.size());
            continue;
        }
        if (arg.rfind(port_prefix, 0) == 0) {
            options.port = static_cast<uint16_t>(std::stoi(arg.substr(port_prefix.size())));
            continue;
        }
    }

    return options;
}

json MetaToJson(const tgw::rpc::RpcMeta& meta) {
    return {
        {"code", static_cast<int>(meta.code())},
        {"message", meta.message()},
        {"request_id", meta.request_id()},
        {"cost_ms", meta.cost_ms()}
    };
}

json ErrorResponse(
    tgw::rpc::RpcCode code,
    const std::string& message,
    const std::string& request_id
) {
    return {
        {"meta", {
            {"code", static_cast<int>(code)},
            {"message", message},
            {"request_id", request_id},
            {"cost_ms", 0}
        }}
    };
}

tgw::RpcContext BuildRpcContext(const json& request) {
    tgw::RpcContext ctx;
    ctx.request_id = request.value("request_id", "");
    ctx.caller = "tgw-gateway-remote-client";
    ctx.timeout_ms = request.value("timeout_ms", 1000);
    ctx.deadline = tgw::DeadlineContext::Start(ctx.timeout_ms);
    return ctx;
}

json HandleRequest(const json& request, const std::shared_ptr<tgw::IUserService>& service) {
    std::string method = request.value("method", "");
    tgw::RpcContext ctx = BuildRpcContext(request);

    if (method == "Login") {
        tgw::rpc::LoginRequest rpc_request;
        rpc_request.set_username(request.value("username", ""));
        rpc_request.set_password(request.value("password", ""));

        auto response = service->Login(ctx, rpc_request);
        return {
            {"meta", MetaToJson(response.meta())},
            {"user_id", response.user_id()},
            {"username", response.username()},
            {"token", response.token()},
            {"role", response.role()}
        };
    }

    if (method == "GetProfile") {
        tgw::rpc::GetProfileRequest rpc_request;
        rpc_request.set_user_id(request.value("user_id", 0));

        auto response = service->GetProfile(ctx, rpc_request);
        return {
            {"meta", MetaToJson(response.meta())},
            {"profile", {
                {"user_id", response.profile().user_id()},
                {"username", response.profile().username()},
                {"email", response.profile().email()},
                {"role", response.profile().role()}
            }}
        };
    }

    return ErrorResponse(
        tgw::rpc::RPC_NOT_FOUND,
        "unknown UserService method: " + method,
        ctx.request_id
    );
}

void HandleSession(tcp::socket socket, std::shared_ptr<tgw::IUserService> service) {
    try {
        asio::streambuf buffer;
        asio::read_until(socket, buffer, '\n');

        std::istream input(&buffer);
        std::string line;
        std::getline(input, line);

        auto request = json::parse(line);
        auto response = HandleRequest(request, service);

        std::string payload = response.dump();
        payload.push_back('\n');
        asio::write(socket, asio::buffer(payload));
    } catch (const std::exception& e) {
        TGW_ERROR("user service session failed: {}", e.what());

        std::string payload = ErrorResponse(
            tgw::rpc::RPC_INTERNAL_ERROR,
            e.what(),
            ""
        ).dump();
        payload.push_back('\n');

        boost::system::error_code ec;
        asio::write(socket, asio::buffer(payload), ec);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        auto options = ParseOptions(argc, argv);

        tgw::LogConfig log_config;
        log_config.path = "logs/user-service.log";
        tgw::Logger::Init(log_config);

        auto service = std::make_shared<tgw::UserServiceImpl>();

        asio::io_context io_context;
        tcp::endpoint endpoint(asio::ip::make_address(options.host), options.port);
        tcp::acceptor acceptor(io_context, endpoint);

        TGW_INFO(
            "tgw_user_service listening on {}:{}",
            options.host,
            options.port
        );

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            std::thread(
                HandleSession,
                std::move(socket),
                service
            ).detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "fatal error: " << e.what() << std::endl;
        return 1;
    }
}
