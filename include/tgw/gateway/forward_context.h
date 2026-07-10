#pragma once

#include "tgw/gateway/http_types.h"
#include "tgw/gateway/route_rule.h"

#include <map>
#include <string>

namespace tgw {

struct ForwardContext {
    std::string request_id;

    std::string upstream;
    std::string upstream_path;

    std::string method;
    std::string query;
    std::string body;

    int timeout_ms = 1000;

    std::map<std::string, std::string> headers;

    static ForwardContext Build(
        const HttpRequest& request,
        const RouteMatchResult& match
    ) {
        ForwardContext ctx;

        ctx.request_id = request.request_id;
        ctx.upstream = match.route.upstream;
        ctx.upstream_path = match.upstream_path;
        ctx.method = request.method;
        ctx.query = request.query;
        ctx.body = request.body;
        ctx.timeout_ms = match.route.timeout_ms;

        auto copy_header = [&](const std::string& key) {
            auto it = request.headers.find(key);
            if (it != request.headers.end()) {
                ctx.headers[key] = it->second;
            }
        };

        copy_header("Authorization");
        copy_header("X-Request-Id");
        copy_header("X-Trace-Id");
        copy_header("X-Span-Id");
        copy_header("X-Parent-Span-Id");
        copy_header("X-Username");
        copy_header("X-User-Id");
        copy_header("Content-Type");
        copy_header("traceparent");

        // Debug Header：仅用于本地验证治理能力。
        copy_header("X-Debug-Sleep-Ms");
        copy_header("X-Debug-Force-Status");

        ctx.headers["X-Request-Id"] = request.request_id;

        return ctx;
    }
};

} // namespace tgw