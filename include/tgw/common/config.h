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

// 单条路由配置。
// 这里先支持 prefix 匹配，后续可以扩展 exact、regex、header、method 等匹配方式。
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

    // 网关路由规则列表
    std::vector<RouteConfig> routes;
};

class ConfigLoader {
public:
    static AppConfig LoadFromFile(const std::string& path);
};

} // namespace tgw