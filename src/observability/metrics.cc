#include "tgw/observability/metrics.h"

#include <sstream>

namespace tgw {

namespace {

std::string EscapeLabelValue(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (char c : input) {
        switch (c) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '\n':
                output += "\\n";
                break;
            default:
                output.push_back(c);
                break;
        }
    }

    return output;
}

} // namespace

void MetricsRegistry::RecordRequest(
    const std::string& path,
    const std::string& upstream,
    int status,
    uint64_t duration_ms
) {
    RequestMetricKey key;
    key.path = path;
    key.upstream = upstream;
    key.status = status;

    std::lock_guard<std::mutex> lock(mutex_);
    auto& value = requests_[key];
    ++value.count;
    value.duration_ms_sum += duration_ms;
}

std::string MetricsRegistry::RenderPrometheus() const {
    std::ostringstream out;

    out << "# HELP tgw_gateway_requests_total Total gateway HTTP requests.\n";
    out << "# TYPE tgw_gateway_requests_total counter\n";

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& item : requests_) {
        const auto& key = item.first;
        const auto& value = item.second;

        out << "tgw_gateway_requests_total{path=\""
            << EscapeLabelValue(key.path)
            << "\",upstream=\""
            << EscapeLabelValue(key.upstream)
            << "\",status=\""
            << key.status
            << "\"} "
            << value.count
            << "\n";
    }

    out << "# HELP tgw_gateway_request_duration_ms_sum Gateway request duration sum in milliseconds.\n";
    out << "# TYPE tgw_gateway_request_duration_ms_sum counter\n";
    for (const auto& item : requests_) {
        const auto& key = item.first;
        const auto& value = item.second;

        out << "tgw_gateway_request_duration_ms_sum{path=\""
            << EscapeLabelValue(key.path)
            << "\",upstream=\""
            << EscapeLabelValue(key.upstream)
            << "\",status=\""
            << key.status
            << "\"} "
            << value.duration_ms_sum
            << "\n";
    }

    out << "# HELP tgw_gateway_request_duration_ms_count Gateway request duration sample count.\n";
    out << "# TYPE tgw_gateway_request_duration_ms_count counter\n";
    for (const auto& item : requests_) {
        const auto& key = item.first;
        const auto& value = item.second;

        out << "tgw_gateway_request_duration_ms_count{path=\""
            << EscapeLabelValue(key.path)
            << "\",upstream=\""
            << EscapeLabelValue(key.upstream)
            << "\",status=\""
            << key.status
            << "\"} "
            << value.count
            << "\n";
    }

    return out.str();
}

HttpResponse BuildPrometheusResponse(const MetricsRegistryPtr& metrics) {
    HttpResponse rsp;
    rsp.status = 200;
    rsp.content_type = "text/plain; version=0.0.4; charset=utf-8";
    rsp.body = metrics ? metrics->RenderPrometheus() : "";
    return rsp;
}

} // namespace tgw
