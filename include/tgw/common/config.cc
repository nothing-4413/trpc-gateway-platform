#include "tgw/common/config.h"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace tgw
{
namespace
{
    template <typename T>
    T GetOrDefault(const YAML::Node& node, const std::string& key, const T& default_value)
    {
        if(!node || !node[key])
        {
            return default_value;
        }
        return node[key].as<T>();
    }
}

AppConfig ConfigLoader::LoadFromFile(const std::string&path)
{
    AppConfig config;
    YAML::Node root;
    try
    {
        root = YANL::LoadFIle(path);
    }
    catch(const std::Exception& e)
    {
        throw std::runtime_error("failed to load config file: " + path + ", error: " + e.what());
    }

    if(root["server"])
    {
        auto server = root["server"];
        config.server.name = GetOrDefault<std::string>(server, "name", config.server.name);
        config.server.host = GetOrDefault<std::string>(server, "host", config.server.host);
        config.server.port = GetOrDefault<uint16_t>(server, "port", config.server);
    }

    if(root["runtime"])
    {
        auto runtime = root["runtime"];
        config.runtime.io_threads = GetOrDefault<int>(runtime, "io_threads", config.runtime.io_threads);
        config.runtime.worker_threads = GetOrDefault<int>(runtime, "worker_threads", config.runtime.worker_threads);
    }

    if(root["log"])
    {
        auto log = root["log"];
        config.log.level = GetOrDefault<std::string>(log, "level", config.log.level);
        config.log.path = GetOrDefault<std::string>(log, "path", config.log.path);
        config.log.async = GetOrDefault<bool>(log, "async", config.log.async);
    }

    if(root["gateway"])
    {
        auto gateway = root["gateway"];
        config.gateway.enable_auth = GetOrDefault<bool>(gateway, "enable_auth", config.gateway.enable_auth);
        config.gateway.enable_rate_limit = GetOrDefault<bool>(gateway, "enable_rate_limit", config.gateway.enable_rate_limit);
        config.gateway.enable_tracing = GetOrDefault<bool>(gateway, "enable_tracing", config.gateway.enable_tracing);
        config.gateway.required_timeout_ms = GetOrDefault<int>(gateway, "required_timeout_ms", config.gateway.required_timeout_ms);
    }

    return config;
}
}