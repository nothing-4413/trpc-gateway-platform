#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tgw {

struct ServerConfig {
    std::string name = "tgw-gateway";
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
};

struct RuntimeConfig {
    int io_threads = 2;
    int worker_threads = 4;
};

struct LogConfig {
    std::string level = "info";
    std::string path = "logs/gateway.log";
    bool async = false;
};

struct GatewayConfig {
    bool enable_auth = true;
    bool enable_rate_limit = true;
    bool enable_tracing = true;
    int request_timeout_ms = 3000;
};

struct AuthConfig {
    bool enabled = true;
    std::string jwt_secret = "tgw-dev-secret";
    int token_ttl_seconds = 7200;
    std::vector<std::string> public_paths = {
        "/api/user/login"
    };
};

struct RateLimitRuleConfig {
    std::string path;
    int max_requests = 120;
};

struct RateLimitConfig {
    bool enabled = true;
    int window_seconds = 60;
    int default_max_requests = 120;
    std::vector<RateLimitRuleConfig> rules;
};

// 重试配置。
// max_attempts 表示总调用次数，包括第一次正常调用。
// 例如 max_attempts = 2 表示：首次调用 + 1 次重试。
struct RetryConfig {
    bool enabled = true;
    int max_attempts = 2;
    int backoff_ms = 20;

    // 默认不重试 POST/PUT/DELETE，避免非幂等请求被重复执行。
    bool retry_non_idempotent = false;
};

// 熔断配置。
// 连续失败达到 failure_threshold 后，熔断器进入 OPEN 状态。
// open_seconds 后进入 HALF_OPEN 半开探测状态。
struct CircuitBreakerConfig {
    bool enabled = true;
    int failure_threshold = 5;
    int open_seconds = 10;
    int half_open_success_threshold = 2;
};

// 降级配置。
// 熔断打开或重试失败后，可以返回可控 fallback 响应。
struct FallbackConfig {
    bool enabled = true;
    std::string message = "service degraded";
};

struct GovernanceConfig {
    bool enabled = true;
    RetryConfig retry;
    CircuitBreakerConfig circuit_breaker;
    FallbackConfig fallback;
};

// 单条路由配置。
struct RouteConfig {
    std::string name;
    std::string match_type = "prefix";
    std::string path;
    std::string upstream;
    bool strip_prefix = true;
    int timeout_ms = 1000;
};

struct AppConfig {
    ServerConfig server;
    RuntimeConfig runtime;
    LogConfig log;
    GatewayConfig gateway;
    AuthConfig auth;
    RateLimitConfig rate_limit;
    GovernanceConfig governance;

    std::vector<RouteConfig> routes;
};

class ConfigLoader {
public:
    static AppConfig LoadFromFile(const std::string& path);
};

} // namespace tgw