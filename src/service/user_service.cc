#include "tgw/service/user_service.h"

#include "tgw/common/logger.h"

namespace tgw {

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

} // namespace

tgw::rpc::LoginResponse UserServiceImpl::Login(
    const RpcContext& ctx,
    const tgw::rpc::LoginRequest& request
) {
    TGW_INFO(
        "UserService.Login called, request_id={}, username={}",
        ctx.request_id,
        request.username()
    );

    tgw::rpc::LoginResponse response;

    if (request.username().empty() || request.password().empty()) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_INVALID_ARGUMENT,
            "username or password is empty",
            ctx
        );
        return response;
    }

    // 当前阶段先写死一个用户。
    // 后续模块会替换成 MySQL 用户表查询 + 密码哈希校验。
    if (request.username() != "admin" || request.password() != "123456") {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_UNAUTHORIZED,
            "invalid username or password",
            ctx
        );
        return response;
    }

    response.set_user_id(10001);
    response.set_username("admin");
    response.set_token("mock-token-" + ctx.request_id);

    FillMeta(
        response.mutable_meta(),
        tgw::rpc::RPC_OK,
        "ok",
        ctx
    );

    return response;
}

tgw::rpc::GetProfileResponse UserServiceImpl::GetProfile(
    const RpcContext& ctx,
    const tgw::rpc::GetProfileRequest& request
) {
    TGW_INFO(
        "UserService.GetProfile called, request_id={}, user_id={}",
        ctx.request_id,
        request.user_id()
    );

    tgw::rpc::GetProfileResponse response;

    if (request.user_id() != 10001) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_NOT_FOUND,
            "user not found",
            ctx
        );
        return response;
    }

    auto* profile = response.mutable_profile();
    profile->set_user_id(10001);
    profile->set_username("admin");
    profile->set_email("admin@example.com");
    profile->set_role("admin");

    FillMeta(
        response.mutable_meta(),
        tgw::rpc::RPC_OK,
        "ok",
        ctx
    );

    return response;
}

} // namespace tgw