#include "tgw/common/logger.h"

#include <filesystem>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace tgw
{
    std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

    namespace
    {
        spdlog::level::level_enum ParseLogLevel(const std::string& level) 
        {
            if (level == "trace") {
                return spdlog::level::trace;
            }
            if (level == "debug") {
                return spdlog::level::debug;
            }
            if (level == "info") {
                return spdlog::level::info;
            }
            if (level == "warn") {
                return spdlog::level::warn;
            }
            if (level == "error") {
                return spdlog::level::err;
            }
            return spdlog::level::info;
        }
    }

    void Logger::Init(const LogConfig& config)
    {
        auto log_path = std::filesystem::path(config.path);
        auto log_dir = log_path.parent_path();
        if(!log_dir.empty()&&!std::filesystem::exists(log_dir))
        {
            std::filesystem::create_directories(log_dir);
        }
        
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.path, true);
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        logger_ = std::make_shared<spdlog::logger>("tgw",sinks.begin(),sinks.end());
        auto log_level = ParseLogLevel(config.level);
        logger_->set_level(log_level);
        logger_->flush_on(spdlog::level::info);

        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        spdlog::setdefault_logger(logger_);
    }

    std::shared_ptr<spdlog::logger> Logger::Get()
    {
        if(!logger_)
        {
            logger_ = spdlog::stdout_color_mt("tgw");
            logger_->set_level(spdlog::level::info);
        }
        return logger_;
    }
}