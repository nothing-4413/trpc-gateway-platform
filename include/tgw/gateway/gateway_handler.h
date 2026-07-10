#pragma once

#include "tgw/auth/auth_filter.h"
#include "tgw/gateway/route_rule.h"
#include "tgw/gateway/upstream_client.h"
#include "tgw/governance/rate_limit_filter.h"
#include "tgw/observability/metrics.h"
#include "tgw/observability/tracing.h"

#include <memory>

namespace tgw {

class GatewayHandler {
public:
    GatewayHandler(
        std::shared_ptr<RouteRuleManager> route_manager,
        UpstreamClientPtr upstream_client,
        AuthFilterPtr auth_filter,
        RateLimitFilterPtr rate_limit_filter,
        MetricsRegistryPtr metrics,
        TracerPtr tracer
    );

    HttpResponse Handle(const HttpRequest& request);

private:
    std::shared_ptr<RouteRuleManager> route_manager_;
    UpstreamClientPtr upstream_client_;
    AuthFilterPtr auth_filter_;
    RateLimitFilterPtr rate_limit_filter_;
    MetricsRegistryPtr metrics_;
    TracerPtr tracer_;
};

} // namespace tgw