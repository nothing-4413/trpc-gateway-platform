#include "tgw/auth/token_service.h"

#include <gtest/gtest.h>

namespace tgw {
namespace {

AuthConfig TestAuthConfig() {
    AuthConfig config;
    config.jwt_secret = "unit-test-secret";
    config.token_ttl_seconds = 3600;
    return config;
}

TEST(TokenServiceTest, IssuesAndValidatesJwtWithRole) {
    TokenService service(TestAuthConfig());

    auto token = service.IssueToken(10001, "admin", "admin", "req-1");
    auto record = service.ValidateToken(token);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->user_id, 10001);
    EXPECT_EQ(record->username, "admin");
    EXPECT_EQ(record->role, "admin");
    EXPECT_GT(record->expires_at, record->issued_at);
}

TEST(TokenServiceTest, RejectsTamperedToken) {
    TokenService service(TestAuthConfig());

    auto token = service.IssueToken(10001, "admin", "admin", "req-1");
    token.back() = token.back() == 'a' ? 'b' : 'a';

    EXPECT_FALSE(service.ValidateToken(token).has_value());
}

TEST(TokenServiceTest, RejectsExpiredToken) {
    AuthConfig config = TestAuthConfig();
    config.token_ttl_seconds = -1;
    TokenService service(config);

    auto token = service.IssueToken(10001, "admin", "admin", "req-1");

    EXPECT_FALSE(service.ValidateToken(token).has_value());
}

} // namespace
} // namespace tgw
