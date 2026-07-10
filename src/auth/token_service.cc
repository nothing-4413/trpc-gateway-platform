#include "tgw/auth/token_service.h"

#include "tgw/common/status.h"

#include <chrono>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace tgw {

namespace {

const char kBase64UrlAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int64_t NowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string Base64UrlEncode(const std::string& input) {
    std::string output;
    int value = 0;
    int bits = -6;

    for (unsigned char c : input) {
        value = (value << 8) + c;
        bits += 8;
        while (bits >= 0) {
            output.push_back(kBase64UrlAlphabet[(value >> bits) & 0x3F]);
            bits -= 6;
        }
    }

    if (bits > -6) {
        output.push_back(kBase64UrlAlphabet[((value << 8) >> (bits + 8)) & 0x3F]);
    }

    return output;
}

std::string HexHash(size_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(sizeof(size_t) * 2) << std::setfill('0') << value;
    return oss.str();
}

} // namespace

TokenService::TokenService(AuthConfig config)
    : config_(std::move(config)) {}

std::string TokenService::IssueToken(
    int64_t user_id,
    const std::string& username,
    const std::string& request_id
) {
    int64_t now = NowUnixSeconds();
    int64_t expires_at = now + config_.token_ttl_seconds;

    std::ostringstream payload;
    payload << "{";
    payload << "\"sub\":" << user_id << ",";
    payload << "\"username\":\"" << JsonEscape(username) << "\",";
    payload << "\"iat\":" << now << ",";
    payload << "\"exp\":" << expires_at << ",";
    payload << "\"rid\":\"" << JsonEscape(request_id) << "\"";
    payload << "}";

    std::string header = Base64UrlEncode("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
    std::string body = Base64UrlEncode(payload.str());
    std::string token = header + "." + body + "." + Sign(header, body);

    TokenRecord record;
    record.user_id = user_id;
    record.username = username;
    record.issued_at = now;
    record.expires_at = expires_at;

    std::lock_guard<std::mutex> lock(mutex_);
    tokens_[token] = record;

    return token;
}

std::optional<TokenRecord> TokenService::ValidateToken(const std::string& token) const {
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot == std::string::npos ? 0 : first_dot + 1);

    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return std::nullopt;
    }

    std::string header = token.substr(0, first_dot);
    std::string payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::string signature = token.substr(second_dot + 1);

    if (signature != Sign(header, payload)) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        return std::nullopt;
    }

    if (it->second.expires_at <= NowUnixSeconds()) {
        tokens_.erase(it);
        return std::nullopt;
    }

    return it->second;
}

std::string TokenService::Sign(const std::string& header, const std::string& payload) const {
    std::hash<std::string> hasher;
    return HexHash(hasher(config_.jwt_secret + "." + header + "." + payload));
}

} // namespace tgw
