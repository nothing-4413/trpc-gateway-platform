#include "tgw/common/config.h"
#include "tgw/common/logger.h"
#include "tgw/common/status.h"
#include "tgw/gateway/http_server.h"
#include "tgw/gateway/router.h"

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

    // 健康检查接口。
    // 后续 Docker Compose、Prometheus、负载均衡器都会用它判断网关是否存活。
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

    // 路由查看接口。
    // 当前只是调试用，后续可以接入 admin 权限控制。
    router->AddRoute("GET", "/routes", [router](const tgw::HttpRequest& req) {
        (void)req;

        auto routes = router->ListRoutes();

        std::ostringstream data;
        data << "[";

        for (size_t i = 0; i < routes.size(); ++i) {
            if (i > 0) {
                data << ",";
            }
            data << "\"" << tgw::JsonEscape(routes[i]) << "\"";
        }

        data << "]";

        tgw::HttpResponse rsp;
        rsp.status = 200;
        rsp.content_type = "application/json";
        rsp.body = tgw::ApiResponse::Success(data.str()).ToJson();

        return rsp;
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
        TGW_INFO("auth enabled: {}", config.gateway.enable_auth);
        TGW_INFO("rate limit enabled: {}", config.gateway.enable_rate_limit);
        TGW_INFO("tracing enabled: {}", config.gateway.enable_tracing);
        TGW_INFO("request timeout: {} ms", config.gateway.request_timeout_ms);
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