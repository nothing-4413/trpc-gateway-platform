#pragma once

#include "tgw/auth/token_service.h"
#include "tgw/gateway/http_types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tgw {

struct AuthResult {
    bool allowed = false;
    int64_t user_id = 0;
    std::string username;
    std::string role;
    std::string token;
    HttpResponse response;
};

class AuthFilter {
public:
    AuthFilter(
        AuthConfig config,
        std::vector<RbacRuleConfig> rbac_rules,
        TokenServicePtr token_service
    );

    AuthResult Check(const HttpRequest& request) const;

private:
    bool IsPublicPath(const std::string& path) const;
    bool IsRoleAllowed(const std::string& path, const std::string& role) const;

private:
    AuthConfig config_;
    std::vector<RbacRuleConfig> rbac_rules_;
    TokenServicePtr token_service_;
};

using AuthFilterPtr = std::shared_ptr<AuthFilter>;

} // namespace tgw
