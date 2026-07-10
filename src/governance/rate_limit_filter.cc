#include "tgw/governance/rate_limit_filter.h"

#include "tgw/common/logger.h"
#include "tgw/common/status.h"

#include <algorithm>
#include <chrono>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>

namespace tgw {

namespace {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

int64_t NowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

std::string HeaderValue(const HttpRequest& request, const std::string& key) {
    auto it = request.headers.find(key);
    if (it != request.headers.end()) {
        return it->second;
    }
    return "";
}

HttpResponse TooManyRequests(
    const std::string& request_id,
    int limit,
    int remaining,
    int reset_seconds
) {
    HttpResponse rsp;
    rsp.status = 429;
    rsp.content_type = "application/json";
    rsp.body = ApiResponse::Error(ErrorCode::RATE_LIMITED, "rate limit exceeded").ToJson();
    rsp.headers["X-Request-Id"] = request_id;
    rsp.headers["X-RateLimit-Limit"] = std::to_string(limit);
    rsp.headers["X-RateLimit-Remaining"] = std::to_string(remaining);
    rsp.headers["X-RateLimit-Reset"] = std::to_string(reset_seconds);
    return rsp;
}

bool IsPrefixMatch(const std::string& rule_path, const std::string& request_path) {
    if (rule_path.empty() || request_path.size() < rule_path.size()) {
        return false;
    }
    if (request_path.compare(0, rule_path.size(), rule_path) != 0) {
        return false;
    }
    return request_path.size() == rule_path.size() || request_path[rule_path.size()] == '/';
}

std::string RedisBulkString(const std::string& value) {
    std::ostringstream oss;
    oss << "$" << value.size() << "\r\n" << value << "\r\n";
    return oss.str();
}

std::string RedisCommand(const std::vector<std::string>& args) {
    std::ostringstream oss;
    oss << "*" << args.size() << "\r\n";
    for (const auto& arg : args) {
        oss << RedisBulkString(arg);
    }
    return oss.str();
}

std::string ReadRedisLine(tcp::socket& socket) {
    asio::streambuf buffer;
    asio::read_until(socket, buffer, "\r\n");

    std::istream input(&buffer);
    std::string line;
    std::getline(input, line);
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    return line;
}

int64_t ReadRedisInteger(tcp::socket& socket) {
    std::string line = ReadRedisLine(socket);
    if (line.empty()) {
        throw std::runtime_error("empty redis response");
    }

    if (line[0] == ':') {
        return std::stoll(line.substr(1));
    }

    if (line[0] == '+') {
        return 0;
    }

    if (line[0] == '-') {
        throw std::runtime_error("redis error: " + line.substr(1));
    }

    throw std::runtime_error("unexpected redis response: " + line);
}

int64_t ExecuteRedisInteger(
    tcp::socket& socket,
    const std::vector<std::string>& args
) {
    auto command = RedisCommand(args);
    asio::write(socket, asio::buffer(command));
    return ReadRedisInteger(socket);
}

} // namespace

RateLimitFilter::RateLimitFilter(RateLimitConfig config)
    : config_(std::move(config)) {}

RateLimitDecision RateLimitFilter::Check(
    const HttpRequest& request,
    const RouteMatchResult& match
) {
    RateLimitDecision decision;

    if (!config_.enabled) {
        return decision;
    }

    int window_seconds = std::max(config_.window_seconds, 1);
    int limit = std::max(ResolveLimit(match), 1);
    std::string key = BuildKey(request, match);

    if (config_.redis.enabled) {
        try {
            return CheckRedis(request, window_seconds, limit, key);
        } catch (const std::exception& e) {
            TGW_ERROR(
                "redis rate limit failed, request_id={}, key={}, error={}",
                request.request_id,
                key,
                e.what()
            );

            if (!config_.redis.fail_open) {
                decision.allowed = false;
                decision.limit = limit;
                decision.remaining = 0;
                decision.reset_seconds = window_seconds;
                decision.key = key;
                decision.response = TooManyRequests(
                    request.request_id,
                    limit,
                    0,
                    window_seconds
                );
                return decision;
            }
        }
    }

    return CheckLocal(request, match, window_seconds, limit, key);
}

RateLimitDecision RateLimitFilter::CheckLocal(
    const HttpRequest& request,
    const RouteMatchResult& match,
    int window_seconds,
    int limit,
    const std::string& key
) {
    (void)match;

    RateLimitDecision decision;
    int64_t now = NowUnixSeconds();

    std::lock_guard<std::mutex> lock(mutex_);
    auto& bucket = buckets_[key];
    if (bucket.window_start == 0 || now - bucket.window_start >= window_seconds) {
        bucket.window_start = now;
        bucket.count = 0;
    }

    int reset_seconds = static_cast<int>(window_seconds - (now - bucket.window_start));
    if (reset_seconds < 0) {
        reset_seconds = 0;
    }

    decision.limit = limit;
    decision.remaining = std::max(limit - bucket.count, 0);
    decision.reset_seconds = reset_seconds;
    decision.key = key;

    if (bucket.count >= limit) {
        decision.allowed = false;
        decision.remaining = 0;
        decision.response = TooManyRequests(
            request.request_id,
            limit,
            decision.remaining,
            reset_seconds
        );
        return decision;
    }

    ++bucket.count;
    decision.remaining = std::max(limit - bucket.count, 0);

    return decision;
}

RateLimitDecision RateLimitFilter::CheckRedis(
    const HttpRequest& request,
    int window_seconds,
    int limit,
    const std::string& key
) {
    RateLimitDecision decision;
    std::string redis_key = config_.redis.key_prefix + ":" + key + ":" +
                            std::to_string(NowUnixSeconds() / window_seconds);

    asio::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::socket socket(io_context);

    auto endpoints = resolver.resolve(
        config_.redis.host,
        std::to_string(config_.redis.port)
    );
    asio::connect(socket, endpoints);

    int64_t count = ExecuteRedisInteger(socket, {"INCR", redis_key});
    if (count == 1) {
        ExecuteRedisInteger(socket, {
            "EXPIRE",
            redis_key,
            std::to_string(window_seconds)
        });
    }

    int64_t ttl = ExecuteRedisInteger(socket, {"TTL", redis_key});
    int reset_seconds = ttl > 0 ? static_cast<int>(ttl) : window_seconds;

    decision.limit = limit;
    decision.remaining = std::max<int64_t>(limit - count, 0);
    decision.reset_seconds = reset_seconds;
    decision.key = redis_key;

    if (count > limit) {
        decision.allowed = false;
        decision.remaining = 0;
        decision.response = TooManyRequests(
            request.request_id,
            limit,
            0,
            reset_seconds
        );
    }

    return decision;
}

int RateLimitFilter::ResolveLimit(const RouteMatchResult& match) const {
    int limit = config_.default_max_requests;
    size_t best_size = 0;

    for (const auto& rule : config_.rules) {
        if (!IsPrefixMatch(rule.path, match.original_path)) {
            continue;
        }

        if (rule.path.size() >= best_size) {
            best_size = rule.path.size();
            limit = rule.max_requests;
        }
    }

    return limit;
}

std::string RateLimitFilter::BuildKey(
    const HttpRequest& request,
    const RouteMatchResult& match
) const {
    std::ostringstream oss;

    std::string user_id = HeaderValue(request, "X-User-Id");
    if (!user_id.empty()) {
        oss << "user:" << user_id;
    } else {
        oss << "ip:" << (request.client_ip.empty() ? "unknown" : request.client_ip);
    }

    oss << "|route:" << match.route.path;
    return oss.str();
}

} // namespace tgw
