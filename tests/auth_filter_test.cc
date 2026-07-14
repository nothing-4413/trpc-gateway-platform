#include "tgw/auth/auth_filter.h"

#include <gtest/gtest.h>

namespace tgw {
namespace {

AuthConfig TestAuthConfig() {
    AuthConfig config;
    config.enabled = true;
    config.jwt_secret = "unit-test-secret";
    config.token_ttl_seconds = 3600;
    config.rbac_enabled = true;
    config.public_paths = {"/health", "/api/user/login"};
    return config;
}

HttpRequest Request(const std::string& path, const std::string& token = "") {
    HttpRequest request;
    request.method = "GET";
    request.path = path;
    request.request_id = "req-1";
    if (!token.empty()) {
        request.headers["Authorization"] = "Bearer " + token;
    }
    return request;
}

std::vector<RbacRuleConfig> RbacRules() {
    RbacRuleConfig admin;
    admin.path = "/admin";
    admin.roles = {"admin"};

    RbacRuleConfig task;
    task.path = "/api/task";
    task.roles = {"admin"};

    return {admin, task};
}

TEST(AuthFilterTest, AllowsPublicPathWithoutToken) {
    auto token_service = std::make_shared<TokenService>(TestAuthConfig());
    AuthFilter filter(TestAuthConfig(), RbacRules(), token_service);

    auto result = filter.Check(Request("/health"));

    EXPECT_TRUE(result.allowed);
}

TEST(AuthFilterTest, RejectsMissingTokenForProtectedPath) {
    auto token_service = std::make_shared<TokenService>(TestAuthConfig());
    AuthFilter filter(TestAuthConfig(), RbacRules(), token_service);

    auto result = filter.Check(Request("/admin/runtime"));

    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.response.status, 401);
}

TEST(AuthFilterTest, AllowsAdminRoleForAdminPath) {
    auto config = TestAuthConfig();
    auto token_service = std::make_shared<TokenService>(config);
    AuthFilter filter(config, RbacRules(), token_service);
    auto token = token_service->IssueToken(10001, "admin", "admin", "req-1");

    auto result = filter.Check(Request("/admin/runtime", token));

    EXPECT_TRUE(result.allowed);
    EXPECT_EQ(result.user_id, 10001);
    EXPECT_EQ(result.role, "admin");
}

TEST(AuthFilterTest, RejectsUserRoleForAdminPath) {
    auto config = TestAuthConfig();
    auto token_service = std::make_shared<TokenService>(config);
    AuthFilter filter(config, RbacRules(), token_service);
    auto token = token_service->IssueToken(10002, "alice", "user", "req-1");

    auto result = filter.Check(Request("/admin/runtime", token));

    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.response.status, 403);
}

TEST(AuthFilterTest, AllowsUserRoleForUnrestrictedPath) {
    auto config = TestAuthConfig();
    auto token_service = std::make_shared<TokenService>(config);
    AuthFilter filter(config, RbacRules(), token_service);
    auto token = token_service->IssueToken(10002, "alice", "user", "req-1");

    auto result = filter.Check(Request("/api/user/profile", token));

    EXPECT_TRUE(result.allowed);
    EXPECT_EQ(result.role, "user");
}

} // namespace
} // namespace tgw
