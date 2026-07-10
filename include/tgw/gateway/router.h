#pragma once

#include "tgw/common/status.h"
#include "tgw/gateway/http_types.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tgw {

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class Router {
public:
    void AddRoute(const std::string& method, const std::string& path, RouteHandler handler);

    // 设置兜底处理器。
    // 精确路由没有匹配时，会进入 fallback。
    void SetFallback(RouteHandler handler);

    HttpResponse Dispatch(const HttpRequest& request) const;

    std::vector<std::string> ListRoutes() const;

private:
    static std::string MakeRouteKey(const std::string& method, const std::string& path);

private:
    std::unordered_map<std::string, RouteHandler> routes_;
    std::optional<RouteHandler> fallback_;
};

} // namespace tgw