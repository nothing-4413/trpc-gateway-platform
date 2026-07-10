#include "tgw/common/config.h"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace tgw {

namespace {

template <typename T>
T GetOrDefault(const YAML::Node& node, const std::string& key, const T& default_value) {
    if (!node || !node[key]) {
        return default_value;
    }
    return node[key].as<T>();
}

} // namespace

AppConfig ConfigLoader::LoadFromFile(const std::string& path) {
    AppConfig config;

    YAML::Node root;

    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("failed to load config file: " + path + ", error: " + e.what());
    }

    if (root["server"]) {
        auto server = root["server"];
        config.server.name = GetOrDefault<std::string>(server, "name", config.server.name);
        config.server.host = GetOrDefault<std::string>(server, "host", config.server.host);
        config.server.port = GetOrDefault<uint16_t>(server, "port", config.server.port);
    }

    if (root["runtime"]) {
        auto runtime = root["runtime"];
        config.runtime.io_threads = GetOrDefault<int>(runtime, "io_threads", config.runtime.io_threads);
        config.runtime.worker_threads = GetOrDefault<int>(runtime, "worker_threads", config.runtime.worker_threads);
    }

    if (root["log"]) {
        auto log = root["log"];
        config.log.level = GetOrDefault<std::string>(log, "level", config.log.level);
        config.log.path = GetOrDefault<std::string>(log, "path", config.log.path);
        config.log.async = GetOrDefault<bool>(log, "async", config.log.async);
    }

    if (root["gateway"]) {
        auto gateway = root["gateway"];
        config.gateway.enable_auth = GetOrDefault<bool>(gateway, "enable_auth", config.gateway.enable_auth);
        config.gateway.enable_rate_limit = GetOrDefault<bool>(gateway, "enable_rate_limit", config.gateway.enable_rate_limit);
        config.gateway.enable_tracing = GetOrDefault<bool>(gateway, "enable_tracing", config.gateway.enable_tracing);
        config.gateway.request_timeout_ms = GetOrDefault<int>(
            gateway,
            "request_timeout_ms",
            config.gateway.request_timeout_ms
        );
    }

    // 读取 routes 配置
    config.auth.enabled = config.gateway.enable_auth;

    if (root["auth"]) {
        auto auth = root["auth"];
        config.auth.enabled = GetOrDefault<bool>(auth, "enabled", config.auth.enabled);
        config.auth.jwt_secret = GetOrDefault<std::string>(auth, "jwt_secret", config.auth.jwt_secret);
        config.auth.token_ttl_seconds = GetOrDefault<int>(
            auth,
            "token_ttl_seconds",
            config.auth.token_ttl_seconds
        );

        if (auth["public_paths"]) {
            const auto& public_paths = auth["public_paths"];
            if (!public_paths.IsSequence()) {
                throw std::runtime_error("auth.public_paths must be a yaml sequence");
            }

            config.auth.public_paths.clear();
            for (const auto& item : public_paths) {
                auto path_item = item.as<std::string>();
                if (path_item.empty()) {
                    throw std::runtime_error("auth.public_paths cannot contain empty path");
                }
                config.auth.public_paths.push_back(path_item);
            }
        }
    }

    if (root["routes"]) {
        const auto& routes = root["routes"];

        if (!routes.IsSequence()) {
            throw std::runtime_error("routes must be a yaml sequence");
        }

        for (const auto& item : routes) {
            RouteConfig route;

            route.name = GetOrDefault<std::string>(item, "name", "");
            route.match_type = GetOrDefault<std::string>(item, "match_type", "prefix");
            route.path = GetOrDefault<std::string>(item, "path", "");
            route.upstream = GetOrDefault<std::string>(item, "upstream", "");
            route.strip_prefix = GetOrDefault<bool>(item, "strip_prefix", true);
            route.timeout_ms = GetOrDefault<int>(item, "timeout_ms", 1000);

            if (route.name.empty()) {
                throw std::runtime_error("route name cannot be empty");
            }

            if (route.path.empty()) {
                throw std::runtime_error("route path cannot be empty, route=" + route.name);
            }

            if (route.upstream.empty()) {
                throw std::runtime_error("route upstream cannot be empty, route=" + route.name);
            }

            config.routes.push_back(route);
        }
    }

    return config;
}

} // namespace tgw
