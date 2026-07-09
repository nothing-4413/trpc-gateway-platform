
#include "tgw/common/config.h"
#include "tgw/common/logger.h"

#include <exception>
#include <iostream>
#include <string>

namespace
{
    std::string ParseConfigPath(int argc,char* argv[])
    {
        std::string config_path = "configs/gateway.yaml";

        const std::string prefix = "--config=";

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg.rfind(prefix, 0) == 0)
            {
                config_path = arg.substr(prefix.size());
            }
        }
        return config_path;
    }
}

int main(int argc,char* argv[])
{
    try
    {
        std::string config_path = ParseConfigPath(argc,argv);
        tgw::AppConfig config = tgw::ConfigLoader::LoadFromFile(config_path);
        tgw::Logger::Init(config.log);

        TGW_INFO("==================================================");
        TGW_INFO("tRPC Gateway Platform starting...");
        TGW_INFO("service name: {}", config.server.name);
        TGW_INFO("listen address: {}:{}", config.server.host, config.server.port);
        TGW_INFO("io threads: {}", config.runtime.io_threads);
        TGW_INFO("worker threads: {}", config.runtime.worker_threads);
        TGW_INFO("auth enabled: {}", config.gateway.enable_auth);
        TGW_INFO("rate limit enabled: {}", config.gateway.enable_rate_limit);
        TGW_INFO("tracing enabled: {}", config.gateway.enable_tracing);
        TGW_INFO("request timeout: {} ms", config.gateway.request_timeout_ms);
        TGW_INFO("==================================================");

        TGW_INFO("module 1 initialized successfully");

        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr <<"fatal error: " << e.what() << std::endl;
        return 1;
    }
}