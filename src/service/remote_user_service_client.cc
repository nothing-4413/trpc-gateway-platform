#include "tgw/service/remote_user_service_client.h"

#include "tgw/common/logger.h"

#include <algorithm>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

namespace tgw {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

namespace {

void FillMeta(
    tgw::rpc::RpcMeta* meta,
    tgw::rpc::RpcCode code,
    const std::string& message,
    const RpcContext& ctx
) {
    meta->set_code(code);
    meta->set_message(message);
    meta->set_request_id(ctx.request_id);
    meta->set_cost_ms(ctx.ElapsedMs());
}

void FillMetaFromJson(tgw::rpc::RpcMeta* meta, const json& value, const RpcContext& ctx) {
    const auto& meta_json = value.at("meta");
    meta->set_code(static_cast<tgw::rpc::RpcCode>(meta_json.value("code", 500)));
    meta->set_message(meta_json.value("message", "remote user service error"));
    meta->set_request_id(meta_json.value("request_id", ctx.request_id));
    meta->set_cost_ms(meta_json.value("cost_ms", ctx.ElapsedMs()));
}

} // namespace

RemoteUserServiceClient::RemoteUserServiceClient(UserServiceRpcConfig config)
    : config_(std::move(config)) {}

std::string RemoteUserServiceClient::Call(const std::string& payload, const RpcContext& ctx) const {
    if (ctx.DeadlineExceeded()) {
        ctx.CancelDeadline("deadline exceeded");
        throw std::runtime_error("deadline exceeded before remote call");
    }

    asio::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::socket socket(io_context);

    auto endpoints = resolver.resolve(config_.host, std::to_string(config_.port));
    asio::connect(socket, endpoints);

    std::string wire_payload = payload;
    wire_payload.push_back('\n');
    asio::write(socket, asio::buffer(wire_payload));

    asio::streambuf buffer;
    asio::read_until(socket, buffer, '\n');

    std::istream input(&buffer);
    std::string line;
    std::getline(input, line);

    if (line.empty()) {
        throw std::runtime_error("remote user service returned empty response");
    }

    return line;
}

tgw::rpc::LoginResponse RemoteUserServiceClient::Login(
    const RpcContext& ctx,
    const tgw::rpc::LoginRequest& request
) {
    tgw::rpc::LoginResponse response;

    try {
        json payload = {
            {"method", "Login"},
            {"request_id", ctx.request_id},
            {"timeout_ms", std::min(ctx.timeout_ms, config_.timeout_ms)},
            {"username", request.username()},
            {"password", request.password()}
        };

        auto raw_response = Call(payload.dump(), ctx);
        auto value = json::parse(raw_response);

        FillMetaFromJson(response.mutable_meta(), value, ctx);
        response.set_user_id(value.value("user_id", 0));
        response.set_username(value.value("username", ""));
        response.set_token(value.value("token", ""));
        response.set_role(value.value("role", "user"));

        TGW_INFO(
            "remote UserService.Login finished, request_id={}, code={}",
            ctx.request_id,
            response.meta().code()
        );
    } catch (const std::exception& e) {
        TGW_ERROR(
            "remote UserService.Login failed, request_id={}, error={}",
            ctx.request_id,
            e.what()
        );
        FillMeta(
            response.mutable_meta(),
            ctx.DeadlineExceeded() ? tgw::rpc::RPC_TIMEOUT : tgw::rpc::RPC_INTERNAL_ERROR,
            e.what(),
            ctx
        );
    }

    return response;
}

tgw::rpc::GetProfileResponse RemoteUserServiceClient::GetProfile(
    const RpcContext& ctx,
    const tgw::rpc::GetProfileRequest& request
) {
    tgw::rpc::GetProfileResponse response;

    try {
        json payload = {
            {"method", "GetProfile"},
            {"request_id", ctx.request_id},
            {"timeout_ms", std::min(ctx.timeout_ms, config_.timeout_ms)},
            {"user_id", request.user_id()}
        };

        auto raw_response = Call(payload.dump(), ctx);
        auto value = json::parse(raw_response);

        FillMetaFromJson(response.mutable_meta(), value, ctx);
        if (value.contains("profile")) {
            const auto& profile = value.at("profile");
            auto* out = response.mutable_profile();
            out->set_user_id(profile.value("user_id", 0));
            out->set_username(profile.value("username", ""));
            out->set_email(profile.value("email", ""));
            out->set_role(profile.value("role", ""));
        }

        TGW_INFO(
            "remote UserService.GetProfile finished, request_id={}, code={}",
            ctx.request_id,
            response.meta().code()
        );
    } catch (const std::exception& e) {
        TGW_ERROR(
            "remote UserService.GetProfile failed, request_id={}, error={}",
            ctx.request_id,
            e.what()
        );
        FillMeta(
            response.mutable_meta(),
            ctx.DeadlineExceeded() ? tgw::rpc::RPC_TIMEOUT : tgw::rpc::RPC_INTERNAL_ERROR,
            e.what(),
            ctx
        );
    }

    return response;
}

} // namespace tgw
