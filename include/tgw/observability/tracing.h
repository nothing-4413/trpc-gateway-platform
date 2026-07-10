#pragma once

#include "tgw/common/config.h"
#include "tgw/gateway/http_types.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace tgw {

struct TraceSpan {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string name;

    bool sampled = true;

    std::chrono::steady_clock::time_point started_at;
    uint64_t start_unix_ms = 0;
};

struct FinishedSpan {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string name;
    std::string service_name;
    std::string kind;

    std::string path;
    std::string upstream;
    int status = 0;

    uint64_t start_unix_ms = 0;
    uint64_t duration_ms = 0;

    std::map<std::string, std::string> attributes;
};

// Tracer 是项目内部的轻量 tracing 核心。
// 当前模块负责 TraceId 贯穿和 Span 记录。
// 后续可在 FinishSpan 中接入 OpenTelemetry / Jaeger exporter。
class Tracer {
public:
    explicit Tracer(TracingConfig config);

    TraceSpan StartServerSpan(
        const HttpRequest& request,
        const std::string& name
    );

    TraceSpan StartClientSpan(
        const TraceSpan& parent,
        const std::string& name
    );

    void FinishSpan(
        const TraceSpan& span,
        const std::string& kind,
        const std::string& path,
        const std::string& upstream,
        int status,
        const std::map<std::string, std::string>& attributes = {}
    );

    std::string RenderRecentSpansJson() const;

    bool Enabled() const;

private:
    bool ShouldSample() const;

    std::string GenerateTraceId();
    std::string GenerateSpanId();

    std::string ExtractTraceIdFromRequest(const HttpRequest& request) const;
    std::string ExtractParentSpanIdFromRequest(const HttpRequest& request) const;

    std::string ExtractTraceIdFromTraceParent(const std::string& traceparent) const;
    std::string ExtractSpanIdFromTraceParent(const std::string& traceparent) const;

    uint64_t NowUnixMs() const;

private:
    TracingConfig config_;

    mutable std::mutex mutex_;
    std::deque<FinishedSpan> finished_spans_;

    uint64_t id_counter_ = 0;
};

using TracerPtr = std::shared_ptr<Tracer>;

HttpResponse BuildTraceDebugResponse(const TracerPtr& tracer);

} // namespace tgw