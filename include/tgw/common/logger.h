#pragma once

#include "tgw/common/config.h"

#include <memory>

#include <spdlog/logger.h>

namespace tgw
{
    class Logger
    {
    public:
        static void Init(const LogConfig& config);
        static std::shared_ptr<spdlog::logger> Get();
    private:
        static std::shared_ptr<spdlog::logger> logger_;
    };
}

#define TGW_TRACE(...) ::tgw::Logger::Get()->trace(__VA_ARGS__)
#define TGW_DEBUG(...) ::tgw::Logger::Get()->debug(__VA_ARGS__)
#define TGW_INFO(...)  ::tgw::Logger::Get()->info(__VA_ARGS__)
#define TGW_WARN(...)  ::tgw::Logger::Get()->warn(__VA_ARGS__)
#define TGW_ERROR(...) ::tgw::Logger::Get()->error(__VA_ARGS__)