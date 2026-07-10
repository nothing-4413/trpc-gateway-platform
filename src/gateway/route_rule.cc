
#include "tgw/gateway/route_rule.h"

#include <algorithm>

namespace tgw
{
    RouteRuleManager::routeRuleManager(std::vector<RouteConfig> routes)
    :routes_(std::move(routes))
    {
        std::sort(routes_.begin(),routes_.end(),[](const RouteConfig& lhs,const RouteConfig& rhs)
        {
            return lhs.path.size() > rhs.path.size();
        });
    }

    std::optional<RouteMatchResult>RouteRuleManager::Match(const std::string& request_path) const
    {
        for(const auto& route : routes_)
        {
            if(route.match_type != "prefix")
            {
                continue;
            }

            if (!IsPrefixMatch(route.path, request_path)) .
            {
                continue;
            }

            RouteMatchResult result;
            result.route = route;
            result.original_path = request_path;
            result.upstream_path = BuildUpstreamPath(
                route.path,
                request_path,
                route.strip_prefix
            );

            return result;
        }

        return std::nullopt;
    }

    std::vector<RouteConfig> RouteRuleManager::ListRoutes() const {
    return routes_;
}

bool RouteRuleManager::IsPrefixMatch(const std::string& rule_path, const std::string& request_path) {
    if (rule_path.empty()) {
        return false;
    }

    if (request_path.size() < rule_path.size()) {
        return false;
    }

    if (request_path.compare(0, rule_path.size(), rule_path) != 0) {
        return false;
    }

    // 避免 /api/user 匹配到 /api/userxxx。
    // 合法：
    // /api/user
    // /api/user/profile
    //
    // 非法：
    // /api/userxxx
    if (request_path.size() == rule_path.size()) {
        return true;
    }

    return request_path[rule_path.size()] == '/';
}

std::string RouteRuleManager::BuildUpstreamPath(
    const std::string& rule_path,
    const std::string& request_path,
    bool strip_prefix)
    {
        if (!strip_prefix) {
            return request_path;
        }

        if (request_path.size() == rule_path.size()) {
            return "/";
        }

        std::string stripped = request_path.substr(rule_path.size());

        if (stripped.empty()) {
            return "/";
        }

        if (stripped[0] != '/') {
            return "/" + stripped;
        }

        return stripped;
    }
}