#pragma once

#include "tgw/common/config.h"
#include "tgw/gateway/http_types.h"
#include "tgw/gateway/route_rule.h"
#include "tgw/gateway/router.h"

#include <memory>
#include <string>

namespace tgw {

// AdminHandler 提供网关管理接口。
// 当前用于查询运行时配置、路由表、功能开关。
// 后续可以扩展：配置热加载、线程池状态、熔断器状态、上游实例状态。
class AdminHandler {
public:
    AdminHandler(
        AppConfig config,
        std::shared_ptr<Router> router,
        std::shared_ptr<RouteRuleManager> route_manager
    );

    HttpResponse Runtime(const HttpRequest& request) const;
    HttpResponse Routes(const HttpRequest& request) const;
    HttpResponse Features(const HttpRequest& request) const;

private:
    HttpResponse JsonResponse(const std::string& raw_json) const;

private:
    AppConfig config_;
    std::shared_ptr<Router> router_;
    std::shared_ptr<RouteRuleManager> route_manager_;
};

using AdminHandlerPtr = std::shared_ptr<AdminHandler>;

} // namespace tgw