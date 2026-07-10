#pragma once

#include "tgw/common/config.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace tgw {

struct TokenRecord {
    int64_t user_id = 0;
    std::string username;
    int64_t issued_at = 0;
    int64_t expires_at = 0;
};

class TokenService {
public:
    explicit TokenService(AuthConfig config);

    std::string IssueToken(
        int64_t user_id,
        const std::string& username,
        const std::string& request_id
    );

    std::optional<TokenRecord> ValidateToken(const std::string& token) const;

private:
    std::string Sign(const std::string& header, const std::string& payload) const;

private:
    AuthConfig config_;
    mutable std::mutex mutex_;
    mutable std::map<std::string, TokenRecord> tokens_;
};

using TokenServicePtr = std::shared_ptr<TokenService>;

} // namespace tgw
