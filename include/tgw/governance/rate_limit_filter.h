#pragma once

#include "tgw/common/config.h"
#include "tgw/gateway/http_types.h"
#include "tgw/gateway/route_rule.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace tgw {

struct RateLimitDecision {
    bool allowed = true;
    int limit = 0;
    int remaining = 0;
    int reset_seconds = 0;
    std::string key;
    HttpResponse response;
};

class RateLimitFilter {
public:
    explicit RateLimitFilter(RateLimitConfig config);

    RateLimitDecision Check(
        const HttpRequest& request,
        const RouteMatchResult& match
    );

private:
    struct Bucket {
        int64_t window_start = 0;
        int count = 0;
    };

    int ResolveLimit(const RouteMatchResult& match) const;
    std::string BuildKey(const HttpRequest& request, const RouteMatchResult& match) const;

private:
    RateLimitConfig config_;
    std::mutex mutex_;
    std::map<std::string, Bucket> buckets_;
};

using RateLimitFilterPtr = std::shared_ptr<RateLimitFilter>;

} // namespace tgw
