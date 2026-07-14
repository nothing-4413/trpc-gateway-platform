#include "tgw/service/user_repository.h"

#include "tgw/common/logger.h"

#include <array>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace tgw {

namespace {

UserRecord AdminUser() {
    UserRecord user;
    user.user_id = 10001;
    user.username = "admin";
    user.password = "123456";
    user.email = "admin@example.com";
    user.role = "admin";
    return user;
}

std::string ShellQuote(const std::string& value) {
    std::string output = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\') {
            output.push_back('\\');
        }
        output.push_back(c);
    }
    output.push_back('"');
    return output;
}

std::string SqlQuote(const std::string& value) {
    std::string output = "'";
    for (char c : value) {
        if (c == '\'') {
            output += "''";
        } else {
            output.push_back(c);
        }
    }
    output.push_back('\'');
    return output;
}

std::vector<std::string> SplitTabLine(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        size_t end = line.find('\t', start);
        if (end == std::string::npos) {
            end = line.size();
        }
        fields.push_back(line.substr(start, end - start));
        if (end == line.size()) {
            break;
        }
        start = end + 1;
    }
    return fields;
}

FILE* OpenPipe(const std::string& command) {
#ifdef _WIN32
    return _popen(command.c_str(), "r");
#else
    return popen(command.c_str(), "r");
#endif
}

int ClosePipe(FILE* pipe) {
#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

} // namespace

std::optional<UserRecord> InMemoryUserRepository::FindByUsername(const std::string& username) {
    auto user = AdminUser();
    if (username == user.username) {
        return user;
    }
    return std::nullopt;
}

std::optional<UserRecord> InMemoryUserRepository::FindById(int64_t user_id) {
    auto user = AdminUser();
    if (user_id == user.user_id) {
        return user;
    }
    return std::nullopt;
}

MysqlCliUserRepository::MysqlCliUserRepository(MysqlConfig config)
    : config_(std::move(config)) {}

std::optional<UserRecord> MysqlCliUserRepository::FindByUsername(const std::string& username) {
    return QueryOne("username=" + SqlQuote(username));
}

std::optional<UserRecord> MysqlCliUserRepository::FindById(int64_t user_id) {
    return QueryOne("user_id=" + std::to_string(user_id));
}

std::optional<UserRecord> MysqlCliUserRepository::QueryOne(const std::string& where_clause) {
    std::string sql =
        "SELECT user_id, username, password, email, role FROM users WHERE " +
        where_clause +
        " LIMIT 1";

    std::string command = BuildCommand(sql);
    FILE* pipe = OpenPipe(command);
    if (!pipe) {
        TGW_ERROR("failed to start mysql client command");
        return std::nullopt;
    }

    std::array<char, 512> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    ClosePipe(pipe);

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }

    if (output.empty()) {
        return std::nullopt;
    }

    auto fields = SplitTabLine(output);
    if (fields.size() < 5) {
        TGW_ERROR("unexpected mysql user row format: {}", output);
        return std::nullopt;
    }

    UserRecord user;
    user.user_id = std::stoll(fields[0]);
    user.username = fields[1];
    user.password = fields[2];
    user.email = fields[3];
    user.role = fields[4];
    return user;
}

std::string MysqlCliUserRepository::BuildCommand(const std::string& sql) const {
    std::ostringstream command;
    command << "mysql --batch --raw --skip-column-names";
    command << " -h " << ShellQuote(config_.host);
    command << " -P " << config_.port;
    command << " -u " << ShellQuote(config_.user);
    if (!config_.password.empty()) {
        command << " --password=" << ShellQuote(config_.password);
    }
    command << " " << ShellQuote(config_.database);
    command << " -e " << ShellQuote(sql);
    return command.str();
}

} // namespace tgw
