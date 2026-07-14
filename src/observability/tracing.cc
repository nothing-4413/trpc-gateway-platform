#include "tgw/observability/tracing.h"

#include "tgw/common/logger.h"
#include "tgw/common/status.h"

#include <chrono>
#include <exception>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

namespace tgw {

namespace {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::string JsonKV(const std::string& key, const std::string& value) {
    std::ostringstream out;
    out << "\"" << JsonEscape(key) << "\":\"" << JsonEscape(value) << "\"";
    return out.str();
}

std::string HexRandom(size_t length) {
    static thread_local std::mt19937_64 rng{
        static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        )
    };

    static constexpr char kHex[] = "0123456789abcdef";

    std::string out;
    out.reserve(length);

    while (out.size() < length) {
        auto value = rng();
        for (int i = 0; i < 16 && out.size() < length; ++i) {
            out.push_back(kHex[value & 0xF]);
            value >>= 4;
        }
    }

    return out;
}

} // namespace

Tracer::Tracer(TracingConfig config)
    : config_(std::move(config)) {}

bool Tracer::Enabled() const {
    return config_.enabled;
}

bool Tracer::ExposeDebugHeaders() const {
    return config_.expose_debug_headers;
}

TraceSpan Tracer::StartServerSpan(
    const HttpRequest& request,
    const std::string& name
) {
    TraceSpan span;

    span.name = name;
    span.started_at = std::chrono::steady_clock::now();
    span.start_unix_ms = NowUnixMs();
    span.sampled = Enabled() && ShouldSample();

    if (!span.sampled) {
        return span;
    }

    span.trace_id = ExtractTraceIdFromRequest(request);
    if (span.trace_id.empty()) {
        span.trace_id = GenerateTraceId();
    }

    span.parent_span_id = ExtractParentSpanIdFromRequest(request);
    span.span_id = GenerateSpanId();

    return span;
}

TraceSpan Tracer::StartClientSpan(
    const TraceSpan& parent,
    const std::string& name
) {
    TraceSpan span;

    span.name = name;
    span.started_at = std::chrono::steady_clock::now();
    span.start_unix_ms = NowUnixMs();
    span.sampled = parent.sampled;

    if (!span.sampled) {
        return span;
    }

    span.trace_id = parent.trace_id;
    span.parent_span_id = parent.span_id;
    span.span_id = GenerateSpanId();

    return span;
}

void Tracer::FinishSpan(
    const TraceSpan& span,
    const std::string& kind,
    const std::string& path,
    const std::string& upstream,
    int status,
    const std::map<std::string, std::string>& attributes
) {
    if (!span.sampled || span.trace_id.empty() || span.span_id.empty()) {
        return;
    }

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - span.started_at
    ).count();

    FinishedSpan finished;
    finished.trace_id = span.trace_id;
    finished.span_id = span.span_id;
    finished.parent_span_id = span.parent_span_id;
    finished.name = span.name;
    finished.service_name = config_.service_name;
    finished.kind = kind;
    finished.path = path;
    finished.upstream = upstream;
    finished.status = status;
    finished.start_unix_ms = span.start_unix_ms;
    finished.duration_ms = static_cast<uint64_t>(duration_ms);
    finished.attributes = attributes;

    std::lock_guard<std::mutex> lock(mutex_);

    finished_spans_.push_back(std::move(finished));

    while (finished_spans_.size() > config_.max_finished_spans) {
        finished_spans_.pop_front();
    }

    if (config_.otlp_exporter_enabled) {
        TracingConfig export_config = config_;
        std::thread([export_config = std::move(export_config), finished]() {
            Tracer exporter(export_config);
            exporter.ExportSpan(finished);
        }).detach();
    }
}

std::string Tracer::RenderRecentSpansJson() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream out;

    out << "{";
    out << "\"count\":" << finished_spans_.size() << ",";
    out << "\"spans\":[";

    for (size_t i = 0; i < finished_spans_.size(); ++i) {
        if (i > 0) {
            out << ",";
        }

        const auto& span = finished_spans_[i];

        out << "{";
        out << JsonKV("trace_id", span.trace_id) << ",";
        out << JsonKV("span_id", span.span_id) << ",";
        out << JsonKV("parent_span_id", span.parent_span_id) << ",";
        out << JsonKV("name", span.name) << ",";
        out << JsonKV("service_name", span.service_name) << ",";
        out << JsonKV("kind", span.kind) << ",";
        out << JsonKV("path", span.path) << ",";
        out << JsonKV("upstream", span.upstream) << ",";
        out << "\"status\":" << span.status << ",";
        out << "\"start_unix_ms\":" << span.start_unix_ms << ",";
        out << "\"duration_ms\":" << span.duration_ms << ",";

        out << "\"attributes\":{";
        size_t j = 0;
        for (const auto& attr : span.attributes) {
            if (j++ > 0) {
                out << ",";
            }
            out << JsonKV(attr.first, attr.second);
        }
        out << "}";

        out << "}";
    }

    out << "]";
    out << "}";

    return out.str();
}

bool Tracer::ShouldSample() const {
    if (config_.sample_ratio >= 1.0) {
        return true;
    }

    if (config_.sample_ratio <= 0.0) {
        return false;
    }

    static thread_local std::mt19937 rng{
        static_cast<uint32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()
        )
    };

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) <= config_.sample_ratio;
}

std::string Tracer::GenerateTraceId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++id_counter_;

    std::ostringstream out;
    out << HexRandom(24)
        << std::hex
        << std::setw(8)
        << std::setfill('0')
        << (id_counter_ & 0xffffffff);

    auto result = out.str();

    if (result.size() < 32) {
        result.append(32 - result.size(), '0');
    }

    if (result.size() > 32) {
        result.resize(32);
    }

    return result;
}

std::string Tracer::GenerateSpanId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++id_counter_;

    std::ostringstream out;
    out << HexRandom(8)
        << std::hex
        << std::setw(8)
        << std::setfill('0')
        << (id_counter_ & 0xffffffff);

    auto result = out.str();

    if (result.size() < 16) {
        result.append(16 - result.size(), '0');
    }

    if (result.size() > 16) {
        result.resize(16);
    }

    return result;
}

std::string Tracer::ExtractTraceIdFromRequest(const HttpRequest& request) const {
    auto trace_id_it = request.headers.find("X-Trace-Id");
    if (trace_id_it != request.headers.end() && !trace_id_it->second.empty()) {
        return trace_id_it->second;
    }

    if (config_.accept_traceparent) {
        auto traceparent_it = request.headers.find("traceparent");
        if (traceparent_it != request.headers.end()) {
            return ExtractTraceIdFromTraceParent(traceparent_it->second);
        }
    }

    return "";
}

std::string Tracer::ExtractParentSpanIdFromRequest(const HttpRequest& request) const {
    auto span_id_it = request.headers.find("X-Span-Id");
    if (span_id_it != request.headers.end() && !span_id_it->second.empty()) {
        return span_id_it->second;
    }

    auto parent_span_id_it = request.headers.find("X-Parent-Span-Id");
    if (parent_span_id_it != request.headers.end() && !parent_span_id_it->second.empty()) {
        return parent_span_id_it->second;
    }

    if (config_.accept_traceparent) {
        auto traceparent_it = request.headers.find("traceparent");
        if (traceparent_it != request.headers.end()) {
            return ExtractSpanIdFromTraceParent(traceparent_it->second);
        }
    }

    return "";
}

std::string Tracer::ExtractTraceIdFromTraceParent(const std::string& traceparent) const {
    // W3C traceparent format:
    // version-traceid-spanid-flags
    // 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01
    if (traceparent.size() < 55) {
        return "";
    }

    if (traceparent[2] != '-' || traceparent[35] != '-' || traceparent[52] != '-') {
        return "";
    }

    return traceparent.substr(3, 32);
}

std::string Tracer::ExtractSpanIdFromTraceParent(const std::string& traceparent) const {
    if (traceparent.size() < 55) {
        return "";
    }

    if (traceparent[2] != '-' || traceparent[35] != '-' || traceparent[52] != '-') {
        return "";
    }

    return traceparent.substr(36, 16);
}

uint64_t Tracer::NowUnixMs() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

void Tracer::ExportSpan(const FinishedSpan& span) const {
    try {
        PostOtlpJson(BuildOtlpJson(span));
    } catch (const std::exception& e) {
        TGW_WARN(
            "otlp span export failed, trace_id={}, span_id={}, error={}",
            span.trace_id,
            span.span_id,
            e.what()
        );
    }
}

std::string Tracer::BuildOtlpJson(const FinishedSpan& span) const {
    uint64_t start_ns = span.start_unix_ms * 1000000ULL;
    uint64_t end_ns = (span.start_unix_ms + span.duration_ms) * 1000000ULL;
    int kind = span.kind == "server" ? 2 : 3;

    std::ostringstream out;
    out << "{";
    out << "\"resourceSpans\":[{";
    out << "\"resource\":{\"attributes\":[";
    out << "{\"key\":\"service.name\",\"value\":{\"stringValue\":\""
        << JsonEscape(span.service_name) << "\"}}";
    out << "]},";
    out << "\"scopeSpans\":[{";
    out << "\"scope\":{\"name\":\"tgw-gateway-platform\",\"version\":\"0.1.0\"},";
    out << "\"spans\":[{";
    out << "\"traceId\":\"" << JsonEscape(span.trace_id) << "\",";
    out << "\"spanId\":\"" << JsonEscape(span.span_id) << "\",";
    if (!span.parent_span_id.empty()) {
        out << "\"parentSpanId\":\"" << JsonEscape(span.parent_span_id) << "\",";
    }
    out << "\"name\":\"" << JsonEscape(span.name) << "\",";
    out << "\"kind\":" << kind << ",";
    out << "\"startTimeUnixNano\":\"" << start_ns << "\",";
    out << "\"endTimeUnixNano\":\"" << end_ns << "\",";
    out << "\"attributes\":[";

    size_t attr_count = 0;
    auto append_attr = [&](const std::string& key, const std::string& value) {
        if (attr_count++ > 0) {
            out << ",";
        }
        out << "{\"key\":\"" << JsonEscape(key) << "\",\"value\":{\"stringValue\":\""
            << JsonEscape(value) << "\"}}";
    };

    append_attr("http.route", span.path);
    append_attr("upstream", span.upstream);
    append_attr("http.status_code", std::to_string(span.status));
    append_attr("duration_ms", std::to_string(span.duration_ms));

    for (const auto& attr : span.attributes) {
        append_attr(attr.first, attr.second);
    }

    out << "],";
    out << "\"status\":{\"code\":" << (span.status >= 500 ? 2 : 1) << "}";
    out << "}]}]}]}";
    out << "}";
    return out.str();
}

void Tracer::PostOtlpJson(const std::string& payload) const {
    asio::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::socket socket(io_context);

    auto endpoints = resolver.resolve(
        config_.otlp_host,
        std::to_string(config_.otlp_http_port)
    );
    asio::connect(socket, endpoints);

    std::ostringstream request;
    request << "POST " << config_.otlp_path << " HTTP/1.1\r\n";
    request << "Host: " << config_.otlp_host << ":" << config_.otlp_http_port << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << payload.size() << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    request << payload;

    auto raw = request.str();
    asio::write(socket, asio::buffer(raw));

    boost::system::error_code ignored;
    socket.shutdown(tcp::socket::shutdown_both, ignored);
    socket.close(ignored);
}

HttpResponse BuildTraceDebugResponse(const TracerPtr& tracer) {
    HttpResponse rsp;
    rsp.status = 200;
    rsp.content_type = "application/json";

    if (!tracer) {
        rsp.body = ApiResponse::Success("{\"count\":0,\"spans\":[]}").ToJson();
        return rsp;
    }

    rsp.body = ApiResponse::Success(tracer->RenderRecentSpansJson()).ToJson();
    return rsp;
}

} // namespace tgw
