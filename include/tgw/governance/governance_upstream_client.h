#pragma once

#include "tgw/common/config.h"
#include "tgw/gateway/upstream_client.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace tgw {

enum class CircuitState {
    CLOSED = 0,
    OPEN = 1,
    HALF_OPEN = 2
};

struct CircuitBreakerRuntimeState {
    CircuitState state = CircuitState::CLOSED;
    int consecutive_failures = 0;
    int consecutive_successes = 0;
    std::chrono::steady_clock::time_point open_until;
};

// GovernanceUpstreamClient 是 UpstreamClient 的治理装饰器。
// 它不关心具体后端是 Local RPC、gRPC 还是 tRPC，只负责：
// 1. 超时判断
// 2. 重试
// 3. 熔断
// 4. 降级
class GovernanceUpstreamClient : public IUpstreamClient {
public:
    GovernanceUpstreamClient(
        UpstreamClientPtr inner,
        GovernanceConfig config
    );

    HttpResponse Forward(const ForwardContext& ctx) override;

private:
    bool AllowRequest(const std::string& upstream);
    void RecordResult(const std::string& upstream, bool success);

    bool CanRetryMethod(const ForwardContext& ctx) const;
    bool IsFailureResponse(const HttpResponse& response) const;
    bool IsIdempotentMethod(const std::string& method) const;

    HttpResponse CallOnce(const ForwardContext& ctx);
    HttpResponse TimeoutResponse(const ForwardContext& ctx, int attempt) const;
    HttpResponse CircuitOpenResponse(const ForwardContext& ctx) const;
    HttpResponse FallbackResponse(const ForwardContext& ctx, const std::string& reason) const;

private:
    UpstreamClientPtr inner_;
    GovernanceConfig config_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, CircuitBreakerRuntimeState> circuit_states_;
};

using GovernanceUpstreamClientPtr = std::shared_ptr<GovernanceUpstreamClient>;

} // namespace tgw