#pragma once
#include<cstdint>
#include<string>

namespace tgw
{
    struct ServerConfig
    {
        std::string name = "tgw_gateway";
        std::string host = "0.0.0.0";
        uint16_t port = 8080;
    };

    struct RuntimeConfig
    {
        int io_threads = 2;
        int worker_threads = 4;
    };

    struct LogConfig
    {
        std::string level = "info";
        std::string path = "logs/gateway.log";
        bool async = false;
    };

    struct GatewayConfig
    {
        bool enable_auth = true;
        bool enable_rate_limit = true;
        bool enable_tracing = true;
        int required_timeout_ms = 3000;
    };

    struct AppConfig
    {
        ServerConfig server;
        RuntimeConfig runtime;
        LogConfig log;
        GatewayConfig gateway;
    };

    struct ConfigLoader
    {
    public:
        static AppConfig LoadFromFile(const std::string& path);
    };

}