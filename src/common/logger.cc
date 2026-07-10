#include "tgw/common/logger.h"

#include <cerrno>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace tgw {

std::shared_ptr<spdlog::logger> Logger::logger_;

namespace {

spdlog::level::level_enum ParseLevel(const std::string& level) {
    if (level == "trace") {
        return spdlog::level::trace;
    }
    if (level == "debug") {
        return spdlog::level::debug;
    }
    if (level == "warn" || level == "warning") {
        return spdlog::level::warn;
    }
    if (level == "error") {
        return spdlog::level::err;
    }
    if (level == "critical") {
        return spdlog::level::critical;
    }
    return spdlog::level::info;
}

bool CreateDirectoryIfMissing(const std::string& path) {
    if (path.empty()) {
        return true;
    }

#ifdef _WIN32
    int rc = _mkdir(path.c_str());
#else
    int rc = mkdir(path.c_str(), 0755);
#endif

    return rc == 0 || errno == EEXIST;
}

void EnsureParentDirectory(const std::string& file_path) {
    size_t pos = 0;

    while (true) {
        pos = file_path.find_first_of("/\\", pos);
        if (pos == std::string::npos) {
            break;
        }

        std::string dir = file_path.substr(0, pos);
        if (!dir.empty() && dir.find(':') == std::string::npos) {
            CreateDirectoryIfMissing(dir);
        }

        ++pos;
    }
}

} // namespace

void Logger::Init(const LogConfig& config) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    if (!config.path.empty()) {
        EnsureParentDirectory(config.path);
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.path, true));
    }

    if (config.async) {
        spdlog::init_thread_pool(8192, 1);
        logger_ = std::make_shared<spdlog::async_logger>(
            "tgw",
            sinks.begin(),
            sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block
        );
        spdlog::register_logger(logger_);
    } else {
        logger_ = std::make_shared<spdlog::logger>("tgw", sinks.begin(), sinks.end());
        spdlog::register_logger(logger_);
    }

    logger_->set_level(ParseLevel(config.level));
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
    logger_->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger_);
}

std::shared_ptr<spdlog::logger> Logger::Get() {
    if (!logger_) {
        LogConfig config;
        Init(config);
    }
    return logger_;
}

} // namespace tgw
