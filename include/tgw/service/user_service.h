#pragma once

#include "tgw/rpc/rpc_context.h"

#include "user.pb.h"

#include <memory>

namespace tgw {

// UserService 抽象接口。
// 后续可以有多种实现：
// 1. 本地 Mock 实现
// 2. MySQL 实现
// 3. 远程 RPC Proxy 实现
class IUserService {
public:
    virtual ~IUserService() = default;

    virtual tgw::rpc::LoginResponse Login(
        const RpcContext& ctx,
        const tgw::rpc::LoginRequest& request
    ) = 0;

    virtual tgw::rpc::GetProfileResponse GetProfile(
        const RpcContext& ctx,
        const tgw::rpc::GetProfileRequest& request
    ) = 0;
};

class UserServiceImpl : public IUserService {
public:
    tgw::rpc::LoginResponse Login(
        const RpcContext& ctx,
        const tgw::rpc::LoginRequest& request
    ) override;

    tgw::rpc::GetProfileResponse GetProfile(
        const RpcContext& ctx,
        const tgw::rpc::GetProfileRequest& request
    ) override;
};

using UserServicePtr = std::shared_ptr<IUserService>;

} // namespace tgw