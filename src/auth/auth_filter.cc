#include "tgw/auth/auth_filter.h"

#include "tgw/common/status.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace tgw {

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string FindHeader(const HttpRequest& request, const std::string& key) {
    std::string expected = ToLower(key);
    for (const auto& item : request.headers) {
        if (ToLower(item.first) == expected) {
            return item.second;
        }
    }
    return "";
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

HttpResponse Unauthorized(const std::string& message, const std::string& request_id) {
    HttpResponse rsp;
    rsp.status = 401;
    rsp.content_type = "application/json";
    rsp.body = ApiResponse::Error(ErrorCode::UNAUTHORIZED, message).ToJson();
    rsp.headers["X-Request-Id"] = request_id;
    return rsp;
}

HttpResponse Forbidden(const std::string& message, const std::string& request_id) {
    HttpResponse rsp;
    rsp.status = 403;
    rsp.content_type = "application/json";
    rsp.body = ApiResponse::Error(ErrorCode::FORBIDDEN, message).ToJson();
    rsp.headers["X-Request-Id"] = request_id;
    return rsp;
}

} // namespace

AuthFilter::AuthFilter(
    AuthConfig config,
    std::vector<RbacRuleConfig> rbac_rules,
    TokenServicePtr token_service
)
    : config_(std::move(config)),
      rbac_rules_(std::move(rbac_rules)),
      token_service_(std::move(token_service)) {}

AuthResult AuthFilter::Check(const HttpRequest& request) const {
    AuthResult result;

    if (!config_.enabled || IsPublicPath(request.path)) {
        result.allowed = true;
        return result;
    }

    if (!token_service_) {
        result.response = Unauthorized("auth service unavailable", request.request_id);
        return result;
    }

    std::string authorization = FindHeader(request, "Authorization");
    const std::string prefix = "Bearer ";

    if (!StartsWith(authorization, prefix)) {
        result.response = Unauthorized("missing bearer token", request.request_id);
        return result;
    }

    std::string token = authorization.substr(prefix.size());
    auto record = token_service_->ValidateToken(token);
    if (!record.has_value()) {
        result.response = Unauthorized("invalid or expired token", request.request_id);
        return result;
    }

    result.allowed = true;
    result.user_id = record->user_id;
    result.username = record->username;
    result.role = record->role;
    result.token = token;

    if (!IsRoleAllowed(request.path, result.role)) {
        result.allowed = false;
        result.response = Forbidden("permission denied", request.request_id);
        return result;
    }

    return result;
}

bool AuthFilter::IsPublicPath(const std::string& path) const {
    for (const auto& public_path : config_.public_paths) {
        if (path == public_path) {
            return true;
        }
    }
    return false;
}

bool AuthFilter::IsRoleAllowed(const std::string& path, const std::string& role) const {
    if (!config_.rbac_enabled || rbac_rules_.empty()) {
        return true;
    }

    const RbacRuleConfig* best_rule = nullptr;
    for (const auto& rule : rbac_rules_) {
        if (!StartsWith(path, rule.path)) {
            continue;
        }
        if (path.size() > rule.path.size() && path[rule.path.size()] != '/') {
            continue;
        }
        if (!best_rule || rule.path.size() > best_rule->path.size()) {
            best_rule = &rule;
        }
    }

    if (!best_rule) {
        return true;
    }

    return std::find(best_rule->roles.begin(), best_rule->roles.end(), role) != best_rule->roles.end();
}

} // namespace tgw
