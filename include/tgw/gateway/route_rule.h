#pragma once

#include "tgw/common/config.h"

#include <optional>
#include <string>
#include <vector>

namespace tgw {

struct RouteMatchResult {
    RouteConfig route;
    std::string original_path;
    std::string upstream_path;
};

class RouteRuleManager {
public:
    explicit RouteRuleManager(std::vector<RouteConfig> routes);

    std::optional<RouteMatchResult> Match(const std::string& request_path) const;

    std::vector<RouteConfig> ListRoutes() const;

private:
    static bool IsPrefixMatch(const std::string& rule_path, const std::string& request_path);

    static std::string BuildUpstreamPath(
        const std::string& rule_path,
        const std::string& request_path,
        bool strip_prefix
    );

private:
    std::vector<RouteConfig> routes_;
};

} // namespace tgw
