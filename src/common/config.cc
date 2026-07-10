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
        config.server.keep_alive = GetOrDefault<bool>(server, "keep_alive", config.server.keep_alive);
        config.server.read_timeout_ms = GetOrDefault<int>(
            server,
            "read_timeout_ms",
            config.server.read_timeout_ms
        );
        config.server.write_timeout_ms = GetOrDefault<int>(
            server,
            "write_timeout_ms",
            config.server.write_timeout_ms
        );
        config.server.body_limit_bytes = GetOrDefault<std::size_t>(
            server,
            "body_limit_bytes",
            config.server.body_limit_bytes
        );

        if (config.server.read_timeout_ms <= 0) {
            throw std::runtime_error("server.read_timeout_ms must be positive");
        }
        if (config.server.write_timeout_ms <= 0) {
            throw std::runtime_error("server.write_timeout_ms must be positive");
        }
        if (config.server.body_limit_bytes == 0) {
            throw std::runtime_error("server.body_limit_bytes must be positive");
        }
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

    if (root["user_service_rpc"]) {
        auto user_service_rpc = root["user_service_rpc"];
        config.user_service_rpc.enabled = GetOrDefault<bool>(
            user_service_rpc,
            "enabled",
            config.user_service_rpc.enabled
        );
        config.user_service_rpc.host = GetOrDefault<std::string>(
            user_service_rpc,
            "host",
            config.user_service_rpc.host
        );
        config.user_service_rpc.port = GetOrDefault<uint16_t>(
            user_service_rpc,
            "port",
            config.user_service_rpc.port
        );
        config.user_service_rpc.timeout_ms = GetOrDefault<int>(
            user_service_rpc,
            "timeout_ms",
            config.user_service_rpc.timeout_ms
        );

        if (config.user_service_rpc.timeout_ms <= 0) {
            throw std::runtime_error("user_service_rpc.timeout_ms must be positive");
        }
    }

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

    config.rate_limit.enabled = config.gateway.enable_rate_limit;

    if (root["rate_limit"]) {
        auto rate_limit = root["rate_limit"];

        config.rate_limit.enabled = GetOrDefault<bool>(
            rate_limit,
            "enabled",
            config.rate_limit.enabled
        );
        config.rate_limit.window_seconds = GetOrDefault<int>(
            rate_limit,
            "window_seconds",
            config.rate_limit.window_seconds
        );
        config.rate_limit.default_max_requests = GetOrDefault<int>(
            rate_limit,
            "default_max_requests",
            config.rate_limit.default_max_requests
        );

        if (rate_limit["rules"]) {
            const auto& rules = rate_limit["rules"];
            if (!rules.IsSequence()) {
                throw std::runtime_error("rate_limit.rules must be a yaml sequence");
            }

            config.rate_limit.rules.clear();

            for (const auto& item : rules) {
                RateLimitRuleConfig rule;
                rule.path = GetOrDefault<std::string>(item, "path", "");
                rule.max_requests = GetOrDefault<int>(
                    item,
                    "max_requests",
                    config.rate_limit.default_max_requests
                );

                if (rule.path.empty()) {
                    throw std::runtime_error("rate_limit rule path cannot be empty");
                }

                if (rule.max_requests <= 0) {
                    throw std::runtime_error("rate_limit max_requests must be positive, path=" + rule.path);
                }

                config.rate_limit.rules.push_back(rule);
            }
        }
    }

    if (root["governance"]) {
        auto governance = root["governance"];

        config.governance.enabled = GetOrDefault<bool>(
            governance,
            "enabled",
            config.governance.enabled
        );

        if (governance["retry"]) {
            auto retry = governance["retry"];

            config.governance.retry.enabled = GetOrDefault<bool>(
                retry,
                "enabled",
                config.governance.retry.enabled
            );
            config.governance.retry.max_attempts = GetOrDefault<int>(
                retry,
                "max_attempts",
                config.governance.retry.max_attempts
            );
            config.governance.retry.backoff_ms = GetOrDefault<int>(
                retry,
                "backoff_ms",
                config.governance.retry.backoff_ms
            );
            config.governance.retry.retry_non_idempotent = GetOrDefault<bool>(
                retry,
                "retry_non_idempotent",
                config.governance.retry.retry_non_idempotent
            );

            if (config.governance.retry.max_attempts <= 0) {
                throw std::runtime_error("governance.retry.max_attempts must be positive");
            }

            if (config.governance.retry.backoff_ms < 0) {
                throw std::runtime_error("governance.retry.backoff_ms cannot be negative");
            }
        }

        if (governance["circuit_breaker"]) {
            auto circuit_breaker = governance["circuit_breaker"];

            config.governance.circuit_breaker.enabled = GetOrDefault<bool>(
                circuit_breaker,
                "enabled",
                config.governance.circuit_breaker.enabled
            );
            config.governance.circuit_breaker.failure_threshold = GetOrDefault<int>(
                circuit_breaker,
                "failure_threshold",
                config.governance.circuit_breaker.failure_threshold
            );
            config.governance.circuit_breaker.open_seconds = GetOrDefault<int>(
                circuit_breaker,
                "open_seconds",
                config.governance.circuit_breaker.open_seconds
            );
            config.governance.circuit_breaker.half_open_success_threshold = GetOrDefault<int>(
                circuit_breaker,
                "half_open_success_threshold",
                config.governance.circuit_breaker.half_open_success_threshold
            );

            if (config.governance.circuit_breaker.failure_threshold <= 0) {
                throw std::runtime_error("governance.circuit_breaker.failure_threshold must be positive");
            }

            if (config.governance.circuit_breaker.open_seconds <= 0) {
                throw std::runtime_error("governance.circuit_breaker.open_seconds must be positive");
            }

            if (config.governance.circuit_breaker.half_open_success_threshold <= 0) {
                throw std::runtime_error(
                    "governance.circuit_breaker.half_open_success_threshold must be positive"
                );
            }
        }

        if (governance["fallback"]) {
            auto fallback = governance["fallback"];

            config.governance.fallback.enabled = GetOrDefault<bool>(
                fallback,
                "enabled",
                config.governance.fallback.enabled
            );
            config.governance.fallback.message = GetOrDefault<std::string>(
                fallback,
                "message",
                config.governance.fallback.message
            );
        }
    }

    config.tracing.enabled = config.gateway.enable_tracing;

    if (root["tracing"]) {
        auto tracing = root["tracing"];

        config.tracing.enabled = GetOrDefault<bool>(
            tracing,
            "enabled",
            config.tracing.enabled
        );
        config.tracing.service_name = GetOrDefault<std::string>(
            tracing,
            "service_name",
            config.tracing.service_name
        );
        config.tracing.sample_ratio = GetOrDefault<double>(
            tracing,
            "sample_ratio",
            config.tracing.sample_ratio
        );
        config.tracing.max_finished_spans = GetOrDefault<std::size_t>(
            tracing,
            "max_finished_spans",
            config.tracing.max_finished_spans
        );
        config.tracing.accept_traceparent = GetOrDefault<bool>(
            tracing,
            "accept_traceparent",
            config.tracing.accept_traceparent
        );
        config.tracing.expose_debug_headers = GetOrDefault<bool>(
            tracing,
            "expose_debug_headers",
            config.tracing.expose_debug_headers
        );

        if (config.tracing.sample_ratio < 0.0 || config.tracing.sample_ratio > 1.0) {
            throw std::runtime_error("tracing.sample_ratio must be between 0.0 and 1.0");
        }

        if (config.tracing.max_finished_spans == 0) {
            throw std::runtime_error("tracing.max_finished_spans must be positive");
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

            if (route.timeout_ms <= 0) {
                throw std::runtime_error("route timeout_ms must be positive, route=" + route.name);
            }

            config.routes.push_back(route);
        }
    }

    return config;
}

} // namespace tgw
