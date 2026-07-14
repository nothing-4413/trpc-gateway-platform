#include "tgw/service/user_service.h"

#include "tgw/common/logger.h"

#include <memory>
#include <utility>

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

bool FillTimeoutIfCancelled(tgw::rpc::RpcMeta* meta, const RpcContext& ctx) {
    if (!ctx.DeadlineExceeded()) {
        return false;
    }

    ctx.CancelDeadline("deadline exceeded");
    FillMeta(meta, tgw::rpc::RPC_TIMEOUT, "deadline exceeded", ctx);
    return true;
}

} // namespace

UserServiceImpl::UserServiceImpl()
    : repository_(std::make_shared<InMemoryUserRepository>()) {}

UserServiceImpl::UserServiceImpl(UserRepositoryPtr repository)
    : repository_(std::move(repository)) {
    if (!repository_) {
        repository_ = std::make_shared<InMemoryUserRepository>();
    }
}

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

    if (FillTimeoutIfCancelled(response.mutable_meta(), ctx)) {
        return response;
    }

    if (request.username().empty() || request.password().empty()) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_INVALID_ARGUMENT,
            "username or password is empty",
            ctx
        );
        return response;
    }

    auto user = repository_->FindByUsername(request.username());
    if (!user.has_value() || user->password != request.password()) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_UNAUTHORIZED,
            "invalid username or password",
            ctx
        );
        return response;
    }

    response.set_user_id(user->user_id);
    response.set_username(user->username);
    response.set_token("mock-token-" + ctx.request_id);
    response.set_role(user->role);

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

    if (FillTimeoutIfCancelled(response.mutable_meta(), ctx)) {
        return response;
    }

    auto user = repository_->FindById(request.user_id());
    if (!user.has_value()) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_NOT_FOUND,
            "user not found",
            ctx
        );
        return response;
    }

    auto* profile = response.mutable_profile();
    profile->set_user_id(user->user_id);
    profile->set_username(user->username);
    profile->set_email(user->email);
    profile->set_role(user->role);

    FillMeta(
        response.mutable_meta(),
        tgw::rpc::RPC_OK,
        "ok",
        ctx
    );

    return response;
}

} // namespace tgw
