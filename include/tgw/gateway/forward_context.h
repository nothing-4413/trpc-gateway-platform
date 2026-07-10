#pragma once

#include "tgw/gateway/http_types.h"
#include "tgw/gateway/route_rule.h"

#include <map>
#include <string>

namespace tgw {

// 网关转发上下文。
// 它描述一次请求从网关转发到后端服务所需的全部信息。
struct ForwardContext {
    std::string request_id;

    std::string upstream;
    std::string upstream_path;

    std::string method;
    std::string query;
    std::string body;

    int timeout_ms = 1000;

    // 需要透传到后端的 header。
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
        copy_header("X-User-Id");
        copy_header("X-Username");
        copy_header("Content-Type");

        // Debug Header：仅用于本地验证治理能力。
        // X-Debug-Sleep-Ms 可模拟慢调用。
        // X-Debug-Force-Status 可模拟上游 5xx。
        copy_header("X-Debug-Sleep-Ms");
        copy_header("X-Debug-Force-Status");

        // 无论客户端是否传入，都保证后端能拿到 request id。
        ctx.headers["X-Request-Id"] = request.request_id;

        return ctx;
    }
};

} // namespace tgw