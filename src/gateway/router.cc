#include "tgw/gateway/router.h"

namespace tgw {

void Router::AddRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    routes_[MakeRouteKey(method, path)] = std::move(handler);
}

void Router::SetFallback(RouteHandler handler) {
    fallback_ = std::move(handler);
}

HttpResponse Router::Dispatch(const HttpRequest& request) const {
    auto key = MakeRouteKey(request.method, request.path);
    auto it = routes_.find(key);

    if (it != routes_.end()) {
        return it->second(request);
    }

    if (fallback_.has_value()) {
        return fallback_.value()(request);
    }

    auto api_rsp = ApiResponse::Error(ErrorCode::NOT_FOUND, "route not found");

    HttpResponse rsp;
    rsp.status = 404;
    rsp.content_type = "application/json";
    rsp.body = api_rsp.ToJson();
    rsp.headers["X-Request-Id"] = request.request_id;

    return rsp;
}

std::vector<std::string> Router::ListRoutes() const {
    std::vector<std::string> result;

    result.reserve(routes_.size());

    for (const auto& item : routes_) {
        result.push_back(item.first);
    }

    if (fallback_.has_value()) {
        result.push_back("FALLBACK /api/*");
    }

    return result;
}

std::string Router::MakeRouteKey(const std::string& method, const std::string& path) {
    return method + " " + path;
}

} // namespace tgw