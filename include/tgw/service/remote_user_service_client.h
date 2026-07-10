#pragma once

#include "tgw/common/config.h"
#include "tgw/service/user_service.h"

#include <cstdint>
#include <string>

namespace tgw {

class RemoteUserServiceClient : public IUserService {
public:
    explicit RemoteUserServiceClient(UserServiceRpcConfig config);

    tgw::rpc::LoginResponse Login(
        const RpcContext& ctx,
        const tgw::rpc::LoginRequest& request
    ) override;

    tgw::rpc::GetProfileResponse GetProfile(
        const RpcContext& ctx,
        const tgw::rpc::GetProfileRequest& request
    ) override;

private:
    std::string Call(const std::string& payload, const RpcContext& ctx) const;

private:
    UserServiceRpcConfig config_;
};

} // namespace tgw
