#include "tgw/admin/admin_handler.h"
#include "tgw/common/config.h"
#include "tgw/common/logger.h"
#include "tgw/common/status.h"
#include "tgw/auth/auth_filter.h"
#include "tgw/auth/token_service.h"
#include "tgw/gateway/gateway_handler.h"
#include "tgw/gateway/http_server.h"
#include "tgw/gateway/local_rpc_upstream_client.h"
#include "tgw/gateway/route_rule.h"
#include "tgw/gateway/router.h"
#include "tgw/governance/rate_limit_filter.h"
#include "tgw/observability/metrics.h"
#include "tgw/service/file_meta_service.h"
#include "tgw/service/task_service.h"
#include "tgw/service/user_service.h"
#include "tgw/admin/admin_handler.h"

#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

std::string ParseConfigPath(int argc, char* argv[]) {
    std::string config_path = "configs/gateway.yaml";
    const std::string prefix = "--config=";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.rfind(prefix, 0) == 0) {
            config_path = arg.substr(prefix.size());
        }
    }

    return config_path;
}

std::shared_ptr<tgw::Router> BuildRouter(const tgw::AppConfig& config) {
    auto router = std::make_shared<tgw::Router>();

    auto route_manager = std::make_shared<tgw::RouteRuleManager>(config.routes);
    auto user_service = std::make_shared<tgw::UserServiceImpl>();
    auto file_meta_service = std::make_shared<tgw::FileMetaServiceImpl>();
    auto task_service = std::make_shared<tgw::TaskServiceImpl>();
    auto token_service = std::make_shared<tgw::TokenService>(config.auth);
    auto auth_filter = std::make_shared<tgw::AuthFilter>(config.auth, token_service);
    auto rate_limit_filter = std::make_shared<tgw::RateLimitFilter>(config.rate_limit);
    auto metrics = std::make_shared<tgw::MetricsRegistry>();

    auto upstream_client = std::make_shared<tgw::LocalRpcUpstreamClient>(
        user_service,
        file_meta_service,
        task_service,
        token_service
    );
    auto gateway_handler = std::make_shared<tgw::GatewayHandler>(
        route_manager,
        upstream_client,
        auth_filter,
        rate_limit_filter,
        metrics
    );
    auto admin_handler = std::make_shared<tgw::AdminHandler>(
        config,
        router,
        route_manager
    );

    router->AddRoute("GET", "/health", [config](const tgw::HttpRequest& req) {
        (void)req;

        std::ostringstream data;
        data << "{";
        data << "\"status\":\"ok\",";
        data << "\"service\":\"" << tgw::JsonEscape(config.server.name) << "\"";
        data << "}";

        tgw::HttpResponse rsp;
        rsp.status = 200;
        rsp.content_type = "application/json";
        rsp.body = tgw::ApiResponse::Success(data.str()).ToJson();

        return rsp;
    });

    router->AddRoute("GET", "/routes", [router, route_manager](const tgw::HttpRequest& req) {
        (void)req;

        auto exact_routes = router->ListRoutes();
        auto gateway_routes = route_manager->ListRoutes();

        std::ostringstream data;
        data << "{";

        data << "\"exact_routes\":[";
        for (size_t i = 0; i < exact_routes.size(); ++i) {
            if (i > 0) {
                data << ",";
            }
            data << "\"" << tgw::JsonEscape(exact_routes[i]) << "\"";
        }
        data << "],";

        data << "\"gateway_routes\":[";
        for (size_t i = 0; i < gateway_routes.size(); ++i) {
            if (i > 0) {
                data << ",";
            }

            const auto& route = gateway_routes[i];

            data << "{";
            data << "\"name\":\"" << tgw::JsonEscape(route.name) << "\",";
            data << "\"match_type\":\"" << tgw::JsonEscape(route.match_type) << "\",";
            data << "\"path\":\"" << tgw::JsonEscape(route.path) << "\",";
            data << "\"upstream\":\"" << tgw::JsonEscape(route.upstream) << "\",";
            data << "\"strip_prefix\":" << (route.strip_prefix ? "true" : "false") << ",";
            data << "\"timeout_ms\":" << route.timeout_ms;
            data << "}";
        }
        data << "]";

        data << "}";

        tgw::HttpResponse rsp;
        rsp.status = 200;
        rsp.content_type = "application/json";
        rsp.body = tgw::ApiResponse::Success(data.str()).ToJson();

        return rsp;
    });

    router->AddRoute("GET", "/metrics", [metrics](const tgw::HttpRequest& req) {
        (void)req;
        return tgw::BuildPrometheusResponse(metrics);
    });

    router->AddRoute("GET", "/admin/runtime", [admin_handler](const tgw::HttpRequest& req) {
        return admin_handler->Runtime(req);
    });

    router->AddRoute("GET", "/admin/routes", [admin_handler](const tgw::HttpRequest& req) {
        return admin_handler->Routes(req);
    });
    
    router->AddRoute("GET", "/routes", [admin_handler](const tgw::HttpRequest& req) {
        return admin_handler->Routes(req);
    });

    router->AddRoute("GET", "/admin/routes", [admin_handler](const tgw::HttpRequest& req) {
        return admin_handler->Routes(req);
    });

    router->AddRoute("GET", "/admin/runtime", [admin_handler](const tgw::HttpRequest& req) {
        return admin_handler->Runtime(req);
    });

    router->AddRoute("GET", "/admin/features", [admin_handler](const tgw::HttpRequest& req) {
        return admin_handler->Features(req);
    });
    // 所有没有精确匹配到的请求都进入 GatewayHandler。
    // GatewayHandler 内部会根据配置路由决定转发到哪个 upstream。
    router->SetFallback([gateway_handler](const tgw::HttpRequest& req) {
        return gateway_handler->Handle(req);
    });

    return router;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        std::string config_path = ParseConfigPath(argc, argv);

        tgw::AppConfig config = tgw::ConfigLoader::LoadFromFile(config_path);

        tgw::Logger::Init(config.log);

        TGW_INFO("==================================================");
        TGW_INFO("tRPC Gateway Platform starting...");
        TGW_INFO("service name: {}", config.server.name);
        TGW_INFO("listen address: {}:{}", config.server.host, config.server.port);
        TGW_INFO("io threads: {}", config.runtime.io_threads);
        TGW_INFO("worker threads: {}", config.runtime.worker_threads);
        TGW_INFO("route count: {}", config.routes.size());
        TGW_INFO("auth enabled: {}", config.auth.enabled);
        TGW_INFO("rate limit enabled: {}", config.rate_limit.enabled);
        TGW_INFO("tracing enabled: {}", config.gateway.enable_tracing);
        TGW_INFO("request timeout: {} ms", config.gateway.request_timeout_ms);
        TGW_INFO("token ttl: {} seconds", config.auth.token_ttl_seconds);
        TGW_INFO("rate limit window: {} seconds", config.rate_limit.window_seconds);
        TGW_INFO("rate limit default max requests: {}", config.rate_limit.default_max_requests);
        TGW_INFO("==================================================");

        auto router = BuildRouter(config);

        tgw::HttpServer server(
            config.server,
            config.runtime,
            router
        );

        server.Start();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal error: " << e.what() << std::endl;
        return 1;
    }
}
