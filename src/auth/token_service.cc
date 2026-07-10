#include "tgw/auth/token_service.h"

#include "tgw/common/status.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tgw {

namespace {

const char kBase64UrlAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int64_t NowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

uint32_t RotR(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
}

std::array<uint8_t, 32> Sha256(const std::vector<uint8_t>& input) {
    static constexpr uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    std::vector<uint8_t> data = input;
    uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8;

    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xff));
    }

    uint32_t h0 = 0x6a09e667;
    uint32_t h1 = 0xbb67ae85;
    uint32_t h2 = 0x3c6ef372;
    uint32_t h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f;
    uint32_t h5 = 0x9b05688c;
    uint32_t h6 = 0x1f83d9ab;
    uint32_t h7 = 0x5be0cd19;

    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t w[64] = {};

        for (int i = 0; i < 16; ++i) {
            size_t j = chunk + static_cast<size_t>(i) * 4;
            w[i] = (static_cast<uint32_t>(data[j]) << 24) |
                   (static_cast<uint32_t>(data[j + 1]) << 16) |
                   (static_cast<uint32_t>(data[j + 2]) << 8) |
                   static_cast<uint32_t>(data[j + 3]);
        }

        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;
        uint32_t f = h5;
        uint32_t g = h6;
        uint32_t h = h7;

        for (int i = 0; i < 64; ++i) {
            uint32_t s1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + s1 + ch + k[i] + w[i];
            uint32_t s0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
        h5 += f;
        h6 += g;
        h7 += h;
    }

    std::array<uint8_t, 32> digest{};
    uint32_t words[8] = {h0, h1, h2, h3, h4, h5, h6, h7};
    for (int i = 0; i < 8; ++i) {
        digest[static_cast<size_t>(i) * 4] = static_cast<uint8_t>((words[i] >> 24) & 0xff);
        digest[static_cast<size_t>(i) * 4 + 1] = static_cast<uint8_t>((words[i] >> 16) & 0xff);
        digest[static_cast<size_t>(i) * 4 + 2] = static_cast<uint8_t>((words[i] >> 8) & 0xff);
        digest[static_cast<size_t>(i) * 4 + 3] = static_cast<uint8_t>(words[i] & 0xff);
    }

    return digest;
}

std::array<uint8_t, 32> HmacSha256(const std::string& key, const std::string& message) {
    constexpr size_t kBlockSize = 64;

    std::vector<uint8_t> key_bytes(key.begin(), key.end());
    if (key_bytes.size() > kBlockSize) {
        auto digest = Sha256(key_bytes);
        key_bytes.assign(digest.begin(), digest.end());
    }
    key_bytes.resize(kBlockSize, 0x00);

    std::vector<uint8_t> o_key_pad(kBlockSize);
    std::vector<uint8_t> i_key_pad(kBlockSize);
    for (size_t i = 0; i < kBlockSize; ++i) {
        o_key_pad[i] = key_bytes[i] ^ 0x5c;
        i_key_pad[i] = key_bytes[i] ^ 0x36;
    }

    std::vector<uint8_t> inner = i_key_pad;
    inner.insert(inner.end(), message.begin(), message.end());
    auto inner_digest = Sha256(inner);

    std::vector<uint8_t> outer = o_key_pad;
    outer.insert(outer.end(), inner_digest.begin(), inner_digest.end());
    return Sha256(outer);
}

std::string Base64UrlEncodeBytes(const uint8_t* data, size_t size) {
    std::string output;
    int value = 0;
    int bits = -6;

    for (size_t i = 0; i < size; ++i) {
        value = (value << 8) + data[i];
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

std::string Base64UrlEncode(const std::string& input) {
    return Base64UrlEncodeBytes(
        reinterpret_cast<const uint8_t*>(input.data()),
        input.size()
    );
}

int Base64UrlValue(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '-') {
        return 62;
    }
    if (c == '_') {
        return 63;
    }
    return -1;
}

std::optional<std::string> Base64UrlDecode(const std::string& input) {
    std::string output;
    int value = 0;
    int bits = -8;

    for (char c : input) {
        int decoded = Base64UrlValue(c);
        if (decoded < 0) {
            return std::nullopt;
        }

        value = (value << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<char>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return output;
}

bool ConstantTimeEquals(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    unsigned char diff = 0;
    for (size_t i = 0; i < lhs.size(); ++i) {
        diff |= static_cast<unsigned char>(lhs[i] ^ rhs[i]);
    }
    return diff == 0;
}

std::optional<std::string> JsonStringValue(const std::string& json, const std::string& key) {
    std::string marker = "\"" + key + "\":\"";
    auto start = json.find(marker);
    if (start == std::string::npos) {
        return std::nullopt;
    }

    start += marker.size();
    std::string value;
    bool escaped = false;

    for (size_t i = start; i < json.size(); ++i) {
        char c = json[i];
        if (escaped) {
            value.push_back(c);
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            return value;
        }
        value.push_back(c);
    }

    return std::nullopt;
}

std::optional<int64_t> JsonIntValue(const std::string& json, const std::string& key) {
    std::string marker = "\"" + key + "\":";
    auto start = json.find(marker);
    if (start == std::string::npos) {
        return std::nullopt;
    }

    start += marker.size();
    auto end = start;
    while (end < json.size() && (json[end] == '-' || (json[end] >= '0' && json[end] <= '9'))) {
        ++end;
    }

    if (end == start) {
        return std::nullopt;
    }

    try {
        return std::stoll(json.substr(start, end - start));
    } catch (const std::exception&) {
        return std::nullopt;
    }
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
    return header + "." + body + "." + Sign(header, body);
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

    auto decoded_header = Base64UrlDecode(header);
    if (!decoded_header.has_value() || decoded_header->find("\"alg\":\"HS256\"") == std::string::npos) {
        return std::nullopt;
    }

    if (!ConstantTimeEquals(signature, Sign(header, payload))) {
        return std::nullopt;
    }

    auto decoded_payload = Base64UrlDecode(payload);
    if (!decoded_payload.has_value()) {
        return std::nullopt;
    }

    auto user_id = JsonIntValue(decoded_payload.value(), "sub");
    auto username = JsonStringValue(decoded_payload.value(), "username");
    auto issued_at = JsonIntValue(decoded_payload.value(), "iat");
    auto expires_at = JsonIntValue(decoded_payload.value(), "exp");

    if (!user_id.has_value() || !username.has_value() || !issued_at.has_value() || !expires_at.has_value()) {
        return std::nullopt;
    }

    if (expires_at.value() <= NowUnixSeconds()) {
        return std::nullopt;
    }

    TokenRecord record;
    record.user_id = user_id.value();
    record.username = username.value();
    record.issued_at = issued_at.value();
    record.expires_at = expires_at.value();

    return record;
}

std::string TokenService::Sign(const std::string& header, const std::string& payload) const {
    std::string message = header + "." + payload;
    auto digest = HmacSha256(config_.jwt_secret, message);
    return Base64UrlEncodeBytes(digest.data(), digest.size());
}

} // namespace tgw
