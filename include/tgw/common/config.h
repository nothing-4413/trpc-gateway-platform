#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tgw {

struct ServerConfig {
    std::string name = "tgw-gateway";
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    bool keep_alive = true;
    int read_timeout_ms = 3000;
    int write_timeout_ms = 3000;
    std::size_t body_limit_bytes = 1048576;
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

struct UserServiceRpcConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    uint16_t port = 9001;
    int timeout_ms = 1000;
};

struct AuthConfig {
    bool enabled = true;
    std::string jwt_secret = "tgw-dev-secret";
    int token_ttl_seconds = 7200;
    bool rbac_enabled = true;
    std::vector<std::string> public_paths = {
        "/api/user/login"
    };
};

struct RbacRuleConfig {
    std::string path;
    std::vector<std::string> roles;
};

struct MysqlConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    uint16_t port = 3306;
    std::string user = "tgw";
    std::string password = "tgw";
    std::string database = "tgw_gateway";
};

struct RateLimitRuleConfig {
    std::string path;
    int max_requests = 120;
};

struct RedisRateLimitConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    std::string key_prefix = "tgw:rate_limit";
    bool fail_open = true;
};

struct RateLimitConfig {
    bool enabled = true;
    int window_seconds = 60;
    int default_max_requests = 120;
    RedisRateLimitConfig redis;
    std::vector<RateLimitRuleConfig> rules;
};

struct RetryConfig {
    bool enabled = true;
    int max_attempts = 2;
    int backoff_ms = 20;
    bool retry_non_idempotent = false;
};

struct CircuitBreakerConfig {
    bool enabled = true;
    int failure_threshold = 5;
    int open_seconds = 10;
    int half_open_success_threshold = 2;
};

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

// 链路追踪配置。
// 当前模块实现内存 Span 收集和 TraceId 贯穿。
// 后续可以在此基础上接 Jaeger / OpenTelemetry exporter。
struct TracingConfig {
    bool enabled = true;
    std::string service_name = "tgw-gateway";

    // 采样率，0.0 表示不采样，1.0 表示全采样。
    double sample_ratio = 1.0;

    // 内存中最多保留多少条已完成 span。
    std::size_t max_finished_spans = 1024;

    // 是否解析 W3C traceparent header。
    bool accept_traceparent = true;
    bool expose_debug_headers = false;
};

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
    UserServiceRpcConfig user_service_rpc;
    MysqlConfig mysql;
    AuthConfig auth;
    std::vector<RbacRuleConfig> rbac_rules;
    RateLimitConfig rate_limit;
    GovernanceConfig governance;
    TracingConfig tracing;

    std::vector<RouteConfig> routes;
};

class ConfigLoader {
public:
    static AppConfig LoadFromFile(const std::string& path);
};

} // namespace tgw
