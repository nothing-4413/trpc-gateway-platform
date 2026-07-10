#pragma once

#include "tgw/auth/auth_filter.h"
#include "tgw/gateway/route_rule.h"
#include "tgw/gateway/upstream_client.h"
#include "tgw/governance/rate_limit_filter.h"

#include <memory>

namespace tgw {

// GatewayHandler 是真正的网关业务入口。
// Router 只负责精确路由，比如 /health、/routes。
// GatewayHandler 负责处理 /api/* 这类动态服务路由。
class GatewayHandler {
public:
    GatewayHandler(
        std::shared_ptr<RouteRuleManager> route_manager,
        UpstreamClientPtr upstream_client,
        AuthFilterPtr auth_filter,
        RateLimitFilterPtr rate_limit_filter
    );

    HttpResponse Handle(const HttpRequest& request);

private:
    std::shared_ptr<RouteRuleManager> route_manager_;
    UpstreamClientPtr upstream_client_;
    AuthFilterPtr auth_filter_;
    RateLimitFilterPtr rate_limit_filter_;
};

} // namespace tgw
