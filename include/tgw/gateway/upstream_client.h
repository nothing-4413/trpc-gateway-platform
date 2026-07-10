#pragma once

#include "tgw/gateway/forward_context.h"
#include "tgw/gateway/http_types.h"

#include <memory>
#include <string>

namespace tgw {

// 后端调用接口。
// 当前模块用 MockUpstreamClient。
// 后续模块会替换成：
// 1. RpcUpstreamClient
// 2. GrpcUpstreamClient
// 3. TrpcUpstreamClient
// 4. HttpUpstreamClient
class IUpstreamClient {
public:
    virtual ~IUpstreamClient() = default;

    virtual HttpResponse Forward(const ForwardContext& ctx) = 0;
};

// Mock 后端客户端。
// 它不真正访问网络，而是模拟一次成功的后端调用。
// 这样我们可以先验证网关路由、转发上下文、日志链路是否正确。
class MockUpstreamClient : public IUpstreamClient {
public:
    HttpResponse Forward(const ForwardContext& ctx) override;
};

using UpstreamClientPtr = std::shared_ptr<IUpstreamClient>;

} // namespace tgw