#include "tgw/governance/rate_limit_filter.h"

#include "tgw/common/status.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <utility>

namespace tgw {

namespace {

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
    int64_t now = NowUnixSeconds();
    std::string key = BuildKey(request, match);

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
