#pragma once

#include "tgw/common/config.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace tgw {

struct UserRecord {
    int64_t user_id = 0;
    std::string username;
    std::string password;
    std::string email;
    std::string role;
};

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual std::optional<UserRecord> FindByUsername(const std::string& username) = 0;
    virtual std::optional<UserRecord> FindById(int64_t user_id) = 0;
};

class InMemoryUserRepository : public IUserRepository {
public:
    std::optional<UserRecord> FindByUsername(const std::string& username) override;
    std::optional<UserRecord> FindById(int64_t user_id) override;
};

class MysqlCliUserRepository : public IUserRepository {
public:
    explicit MysqlCliUserRepository(MysqlConfig config);

    std::optional<UserRecord> FindByUsername(const std::string& username) override;
    std::optional<UserRecord> FindById(int64_t user_id) override;

private:
    std::optional<UserRecord> QueryOne(const std::string& where_clause);
    std::string BuildCommand(const std::string& sql) const;

private:
    MysqlConfig config_;
};

using UserRepositoryPtr = std::shared_ptr<IUserRepository>;

} // namespace tgw
