#pragma once

#include "tgw/gateway/http_types.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace tgw {

struct RequestMetricKey {
    std::string path;
    std::string upstream;
    int status = 0;

    bool operator<(const RequestMetricKey& other) const {
        if (path != other.path) {
            return path < other.path;
        }
        if (upstream != other.upstream) {
            return upstream < other.upstream;
        }
        return status < other.status;
    }
};

struct RequestMetricValue {
    uint64_t count = 0;
    uint64_t duration_ms_sum = 0;
};

class MetricsRegistry {
public:
    void RecordRequest(
        const std::string& path,
        const std::string& upstream,
        int status,
        uint64_t duration_ms
    );

    std::string RenderPrometheus() const;

private:
    mutable std::mutex mutex_;
    std::map<RequestMetricKey, RequestMetricValue> requests_;
};

using MetricsRegistryPtr = std::shared_ptr<MetricsRegistry>;

HttpResponse BuildPrometheusResponse(const MetricsRegistryPtr& metrics);

} // namespace tgw
