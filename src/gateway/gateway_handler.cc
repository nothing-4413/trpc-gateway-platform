#include "tgw/gateway/gateway_handler.h"

#include "tgw/common/logger.h"
#include "tgw/common/status.h"

#include <string>
#include <utility>

namespace tgw {

GatewayHandler::GatewayHandler(
    std::shared_ptr<RouteRuleManager> route_manager,
    UpstreamClientPtr upstream_client,
    AuthFilterPtr auth_filter,
    RateLimitFilterPtr rate_limit_filter
)
    : route_manager_(std::move(route_manager)),
      upstream_client_(std::move(upstream_client)),
      auth_filter_(std::move(auth_filter)),
      rate_limit_filter_(std::move(rate_limit_filter)) {}

HttpResponse GatewayHandler::Handle(const HttpRequest& request) {
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

        return rsp;
    }

    HttpRequest forward_request = request;

    if (auth_filter_) {
        auto auth_result = auth_filter_->Check(request);
        if (!auth_result.allowed) {
            TGW_WARN(
                "gateway auth rejected, request_id={}, path={}",
                request.request_id,
                request.path
            );
            return auth_result.response;
        }

        if (auth_result.user_id > 0) {
            forward_request.headers["X-User-Id"] = std::to_string(auth_result.user_id);
            forward_request.headers["X-Username"] = auth_result.username;
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
            return rate_limit_decision.response;
        }
    }

    ForwardContext ctx = ForwardContext::Build(forward_request, match.value());

    TGW_INFO(
        "gateway forward start, request_id={}, path={}, upstream={}, upstream_path={}, timeout_ms={}",
        ctx.request_id,
        request.path,
        ctx.upstream,
        ctx.upstream_path,
        ctx.timeout_ms
    );

    HttpResponse rsp = upstream_client_->Forward(ctx);

    if (rate_limit_filter_ && rate_limit_decision.limit > 0) {
        rsp.headers["X-RateLimit-Limit"] = std::to_string(rate_limit_decision.limit);
        rsp.headers["X-RateLimit-Remaining"] = std::to_string(rate_limit_decision.remaining);
        rsp.headers["X-RateLimit-Reset"] = std::to_string(rate_limit_decision.reset_seconds);
    }

    TGW_INFO(
        "gateway forward finished, request_id={}, upstream={}, status={}",
        ctx.request_id,
        ctx.upstream,
        rsp.status
    );

    return rsp;
}

} // namespace tgw
