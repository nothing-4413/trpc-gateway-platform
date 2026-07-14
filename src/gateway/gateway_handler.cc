#include "tgw/gateway/gateway_handler.h"

#include "tgw/common/logger.h"
#include "tgw/common/status.h"

#include <chrono>
#include <map>
#include <string>
#include <utility>

namespace tgw {

GatewayHandler::GatewayHandler(
    std::shared_ptr<RouteRuleManager> route_manager,
    UpstreamClientPtr upstream_client,
    AuthFilterPtr auth_filter,
    RateLimitFilterPtr rate_limit_filter,
    MetricsRegistryPtr metrics,
    TracerPtr tracer
)
    : route_manager_(std::move(route_manager)),
      upstream_client_(std::move(upstream_client)),
      auth_filter_(std::move(auth_filter)),
      rate_limit_filter_(std::move(rate_limit_filter)),
      metrics_(std::move(metrics)),
      tracer_(std::move(tracer)) {}

HttpResponse GatewayHandler::Handle(const HttpRequest& request) {
    auto started_at = std::chrono::steady_clock::now();

    TraceSpan gateway_span;
    if (tracer_) {
        gateway_span = tracer_->StartServerSpan(request, "gateway.handle");
    }

    auto finish = [&](HttpResponse rsp, const std::string& upstream) {
        if (metrics_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_at
            ).count();

            metrics_->RecordRequest(
                request.path,
                upstream.empty() ? "none" : upstream,
                rsp.status,
                static_cast<uint64_t>(elapsed)
            );
        }

        if (tracer_) {
            tracer_->FinishSpan(
                gateway_span,
                "server",
                request.path,
                upstream.empty() ? "none" : upstream,
                rsp.status,
                {
                    {"http.method", request.method},
                    {"request.id", request.request_id}
                }
            );
        }

        if (tracer_ && tracer_->ExposeDebugHeaders() && !gateway_span.trace_id.empty()) {
            rsp.headers["X-Trace-Id"] = gateway_span.trace_id;
        }

        if (tracer_ && tracer_->ExposeDebugHeaders() && !gateway_span.span_id.empty()) {
            rsp.headers["X-Span-Id"] = gateway_span.span_id;
        }

        return rsp;
    };

    auto match = route_manager_->Match(request.path);

    if (!match.has_value()) {
        TGW_WARN(
            "gateway route not found, request_id={}, path={}",
            request.request_id,
            request.path
        );

        HttpResponse rsp;
        rsp.status = 404;
        rsp.content_type = "application/json";
        rsp.body = ApiResponse::Error(ErrorCode::NOT_FOUND, "gateway route not found").ToJson();
        rsp.headers["X-Request-Id"] = request.request_id;

        return finish(rsp, "none");
    }

    HttpRequest forward_request = request;

    if (!gateway_span.trace_id.empty()) {
        forward_request.headers["X-Trace-Id"] = gateway_span.trace_id;
    }

    if (!gateway_span.span_id.empty()) {
        forward_request.headers["X-Parent-Span-Id"] = gateway_span.span_id;
    }

    if (auth_filter_) {
        auto auth_result = auth_filter_->Check(request);

        if (!auth_result.allowed) {
            TGW_WARN(
                "gateway auth rejected, request_id={}, path={}",
                request.request_id,
                request.path
            );

            return finish(auth_result.response, match->route.upstream);
        }

        if (auth_result.user_id > 0) {
            forward_request.headers["X-User-Id"] = std::to_string(auth_result.user_id);
            forward_request.headers["X-Username"] = auth_result.username;
            forward_request.headers["X-Role"] = auth_result.role;
        }
    }

    RateLimitDecision rate_limit_decision;

    if (rate_limit_filter_) {
        rate_limit_decision = rate_limit_filter_->Check(forward_request, match.value());

        if (!rate_limit_decision.allowed) {
            TGW_WARN(
                "gateway rate limited, request_id={}, path={}, key={}, limit={}",
                request.request_id,
                request.path,
                rate_limit_decision.key,
                rate_limit_decision.limit
            );

            return finish(rate_limit_decision.response, match->route.upstream);
        }
    }

    ForwardContext ctx = ForwardContext::Build(forward_request, match.value());

    TraceSpan upstream_span;
    if (tracer_) {
        upstream_span = tracer_->StartClientSpan(
            gateway_span,
            "upstream." + ctx.upstream
        );

        if (!upstream_span.trace_id.empty()) {
            ctx.headers["X-Trace-Id"] = upstream_span.trace_id;
        }

        if (!upstream_span.span_id.empty()) {
            ctx.headers["X-Span-Id"] = upstream_span.span_id;
        }

        if (!upstream_span.parent_span_id.empty()) {
            ctx.headers["X-Parent-Span-Id"] = upstream_span.parent_span_id;
        }
    }

    TGW_INFO(
        "gateway forward start, request_id={}, trace_id={}, path={}, upstream={}, upstream_path={}, timeout_ms={}",
        ctx.request_id,
        gateway_span.trace_id,
        request.path,
        ctx.upstream,
        ctx.upstream_path,
        ctx.timeout_ms
    );

    HttpResponse rsp = upstream_client_->Forward(ctx);

    if (tracer_) {
        tracer_->FinishSpan(
            upstream_span,
            "client",
            ctx.upstream_path,
            ctx.upstream,
            rsp.status,
            {
                {"http.method", ctx.method},
                {"request.id", ctx.request_id},
                {"route.path", request.path}
            }
        );
    }

    if (rate_limit_filter_ && rate_limit_decision.limit > 0) {
        rsp.headers["X-RateLimit-Limit"] = std::to_string(rate_limit_decision.limit);
        rsp.headers["X-RateLimit-Remaining"] = std::to_string(rate_limit_decision.remaining);
        rsp.headers["X-RateLimit-Reset"] = std::to_string(rate_limit_decision.reset_seconds);
    }

    TGW_INFO(
        "gateway forward finished, request_id={}, trace_id={}, upstream={}, status={}",
        ctx.request_id,
        gateway_span.trace_id,
        ctx.upstream,
        rsp.status
    );

    return finish(rsp, ctx.upstream);
}

} // namespace tgw
