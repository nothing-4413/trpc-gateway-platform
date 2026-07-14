#include "tgw/admin/admin_handler.h"

#include "tgw/common/status.h"

#include <sstream>
#include <utility>
#include <vector>

namespace tgw {

namespace {

std::string BoolToJson(bool value) {
    return value ? "true" : "false";
}

} // namespace

AdminHandler::AdminHandler(
    AppConfig config,
    std::shared_ptr<Router> router,
    std::shared_ptr<RouteRuleManager> route_manager
)
    : config_(std::move(config)),
      router_(std::move(router)),
      route_manager_(std::move(route_manager)) {}

HttpResponse AdminHandler::JsonResponse(const std::string& raw_json) const {
    HttpResponse rsp;
    rsp.status = 200;
    rsp.content_type = "application/json";
    rsp.body = ApiResponse::Success(raw_json).ToJson();
    return rsp;
}

HttpResponse AdminHandler::Runtime(const HttpRequest& request) const {
    (void)request;

    std::ostringstream data;

    data << "{";

    data << "\"service\":{";
    data << "\"name\":\"" << JsonEscape(config_.server.name) << "\",";
    data << "\"host\":\"" << JsonEscape(config_.server.host) << "\",";
    data << "\"port\":" << config_.server.port;
    data << "},";

    data << "\"runtime\":{";
    data << "\"io_threads\":" << config_.runtime.io_threads << ",";
    data << "\"worker_threads\":" << config_.runtime.worker_threads;
    data << "},";

    data << "\"gateway\":{";
    data << "\"request_timeout_ms\":" << config_.gateway.request_timeout_ms << ",";
    data << "\"enable_tracing\":" << BoolToJson(config_.gateway.enable_tracing);
    data << "},";

    data << "\"user_service_rpc\":{";
    data << "\"enabled\":" << BoolToJson(config_.user_service_rpc.enabled) << ",";
    data << "\"host\":\"" << JsonEscape(config_.user_service_rpc.host) << "\",";
    data << "\"port\":" << config_.user_service_rpc.port << ",";
    data << "\"timeout_ms\":" << config_.user_service_rpc.timeout_ms;
    data << "},";

    data << "\"auth\":{";
    data << "\"enabled\":" << BoolToJson(config_.auth.enabled) << ",";
    data << "\"token_ttl_seconds\":" << config_.auth.token_ttl_seconds << ",";
    data << "\"rbac_enabled\":" << BoolToJson(config_.auth.rbac_enabled) << ",";
    data << "\"rbac_rule_count\":" << config_.rbac_rules.size();
    data << "},";

    data << "\"mysql\":{";
    data << "\"enabled\":" << BoolToJson(config_.mysql.enabled) << ",";
    data << "\"host\":\"" << JsonEscape(config_.mysql.host) << "\",";
    data << "\"port\":" << config_.mysql.port << ",";
    data << "\"database\":\"" << JsonEscape(config_.mysql.database) << "\"";
    data << "},";

    data << "\"rate_limit\":{";
    data << "\"enabled\":" << BoolToJson(config_.rate_limit.enabled) << ",";
    data << "\"window_seconds\":" << config_.rate_limit.window_seconds << ",";
    data << "\"default_max_requests\":" << config_.rate_limit.default_max_requests << ",";
    data << "\"redis\":{";
    data << "\"enabled\":" << BoolToJson(config_.rate_limit.redis.enabled) << ",";
    data << "\"host\":\"" << JsonEscape(config_.rate_limit.redis.host) << "\",";
    data << "\"port\":" << config_.rate_limit.redis.port << ",";
    data << "\"key_prefix\":\"" << JsonEscape(config_.rate_limit.redis.key_prefix) << "\",";
    data << "\"fail_open\":" << BoolToJson(config_.rate_limit.redis.fail_open);
    data << "}";
    data << "},";

    data << "\"governance\":{";
    data << "\"enabled\":" << BoolToJson(config_.governance.enabled) << ",";

    data << "\"retry\":{";
    data << "\"enabled\":" << BoolToJson(config_.governance.retry.enabled) << ",";
    data << "\"max_attempts\":" << config_.governance.retry.max_attempts << ",";
    data << "\"backoff_ms\":" << config_.governance.retry.backoff_ms << ",";
    data << "\"retry_non_idempotent\":"
         << BoolToJson(config_.governance.retry.retry_non_idempotent);
    data << "},";

    data << "\"circuit_breaker\":{";
    data << "\"enabled\":"
         << BoolToJson(config_.governance.circuit_breaker.enabled) << ",";
    data << "\"failure_threshold\":"
         << config_.governance.circuit_breaker.failure_threshold << ",";
    data << "\"open_seconds\":"
         << config_.governance.circuit_breaker.open_seconds << ",";
    data << "\"half_open_success_threshold\":"
         << config_.governance.circuit_breaker.half_open_success_threshold;
    data << "},";

    data << "\"fallback\":{";
    data << "\"enabled\":"
         << BoolToJson(config_.governance.fallback.enabled) << ",";
    data << "\"message\":\""
         << JsonEscape(config_.governance.fallback.message) << "\"";
    data << "}";

    data << "},";

    data << "\"tracing\":{";
    data << "\"enabled\":" << BoolToJson(config_.tracing.enabled) << ",";
    data << "\"service_name\":\"" << JsonEscape(config_.tracing.service_name) << "\",";
    data << "\"sample_ratio\":" << config_.tracing.sample_ratio << ",";
    data << "\"max_finished_spans\":" << config_.tracing.max_finished_spans << ",";
    data << "\"accept_traceparent\":"
         << BoolToJson(config_.tracing.accept_traceparent);
    data << "}";

    data << "}";

    return JsonResponse(data.str());
}

HttpResponse AdminHandler::Routes(const HttpRequest& request) const {
    (void)request;

    std::ostringstream data;

    auto exact_routes = router_ ? router_->ListRoutes() : std::vector<std::string>{};
    auto gateway_routes = route_manager_ ? route_manager_->ListRoutes() : std::vector<RouteConfig>{};

    data << "{";

    data << "\"exact_routes\":[";
    for (size_t i = 0; i < exact_routes.size(); ++i) {
        if (i > 0) {
            data << ",";
        }
        data << "\"" << JsonEscape(exact_routes[i]) << "\"";
    }
    data << "],";

    data << "\"gateway_routes\":[";
    for (size_t i = 0; i < gateway_routes.size(); ++i) {
        if (i > 0) {
            data << ",";
        }

        const auto& route = gateway_routes[i];

        data << "{";
        data << "\"name\":\"" << JsonEscape(route.name) << "\",";
        data << "\"match_type\":\"" << JsonEscape(route.match_type) << "\",";
        data << "\"path\":\"" << JsonEscape(route.path) << "\",";
        data << "\"upstream\":\"" << JsonEscape(route.upstream) << "\",";
        data << "\"strip_prefix\":" << BoolToJson(route.strip_prefix) << ",";
        data << "\"timeout_ms\":" << route.timeout_ms;
        data << "}";
    }
    data << "]";

    data << "}";

    return JsonResponse(data.str());
}

HttpResponse AdminHandler::Features(const HttpRequest& request) const {
    (void)request;

    std::ostringstream data;

    data << "{";
    data << "\"auth_enabled\":" << BoolToJson(config_.auth.enabled) << ",";
    data << "\"rbac_enabled\":" << BoolToJson(config_.auth.rbac_enabled) << ",";
    data << "\"mysql_enabled\":" << BoolToJson(config_.mysql.enabled) << ",";
    data << "\"rate_limit_enabled\":" << BoolToJson(config_.rate_limit.enabled) << ",";
    data << "\"redis_rate_limit_enabled\":"
         << BoolToJson(config_.rate_limit.redis.enabled) << ",";
    data << "\"governance_enabled\":" << BoolToJson(config_.governance.enabled) << ",";
    data << "\"retry_enabled\":" << BoolToJson(config_.governance.retry.enabled) << ",";
    data << "\"circuit_breaker_enabled\":"
         << BoolToJson(config_.governance.circuit_breaker.enabled) << ",";
    data << "\"fallback_enabled\":"
         << BoolToJson(config_.governance.fallback.enabled) << ",";
    data << "\"remote_user_service_enabled\":"
         << BoolToJson(config_.user_service_rpc.enabled) << ",";
    data << "\"tracing_enabled\":" << BoolToJson(config_.tracing.enabled) << ",";
    data << "\"metrics_enabled\":true,";
    data << "\"admin_enabled\":true";
    data << "}";

    return JsonResponse(data.str());
}

} // namespace tgw
