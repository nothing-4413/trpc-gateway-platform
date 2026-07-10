#include "tgw/governance/governance_upstream_client.h"

#include "tgw/common/logger.h"
#include "tgw/common/status.h"

#include <chrono>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace tgw {

namespace {

int HeaderToInt(
    const std::map<std::string, std::string>& headers,
    const std::string& key,
    int default_value
) {
    auto it = headers.find(key);
    if (it == headers.end()) {
        return default_value;
    }

    try {
        return std::stoi(it->second);
    } catch (...) {
        return default_value;
    }
}

HttpResponse BuildErrorResponse(
    int http_status,
    ErrorCode code,
    const std::string& message,
    const std::string& request_id
) {
    HttpResponse rsp;
    rsp.status = http_status;
    rsp.content_type = "application/json";
    rsp.body = ApiResponse::Error(code, message).ToJson();
    rsp.headers["X-Request-Id"] = request_id;
    return rsp;
}

} // namespace

GovernanceUpstreamClient::GovernanceUpstreamClient(
    UpstreamClientPtr inner,
    GovernanceConfig config
)
    : inner_(std::move(inner)),
      config_(std::move(config)) {}

HttpResponse GovernanceUpstreamClient::Forward(const ForwardContext& ctx) {
    if (!config_.enabled) {
        return inner_->Forward(ctx);
    }

    if (!AllowRequest(ctx.upstream)) {
        TGW_WARN(
            "circuit breaker rejected request, request_id={}, upstream={}",
            ctx.request_id,
            ctx.upstream
        );

        return CircuitOpenResponse(ctx);
    }

    int max_attempts = config_.retry.enabled ? config_.retry.max_attempts : 1;
    if (max_attempts <= 0) {
        max_attempts = 1;
    }

    HttpResponse last_response;
    bool has_response = false;

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        TGW_INFO(
            "governance call attempt, request_id={}, upstream={}, attempt={}/{}",
            ctx.request_id,
            ctx.upstream,
            attempt,
            max_attempts
        );

        auto started_at = std::chrono::steady_clock::now();

        HttpResponse response = CallOnce(ctx);

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at
        ).count();

        if (elapsed_ms > ctx.timeout_ms) {
            TGW_WARN(
                "upstream timeout detected, request_id={}, upstream={}, elapsed_ms={}, timeout_ms={}",
                ctx.request_id,
                ctx.upstream,
                elapsed_ms,
                ctx.timeout_ms
            );

            response = TimeoutResponse(ctx, attempt);
        }

        response.headers["X-Governance-Attempt"] = std::to_string(attempt);
        response.headers["X-Governance-Elapsed-Ms"] = std::to_string(elapsed_ms);

        last_response = response;
        has_response = true;

        bool failure = IsFailureResponse(response);
        RecordResult(ctx.upstream, !failure);

        if (!failure) {
            return response;
        }

        bool should_retry = config_.retry.enabled &&
                            attempt < max_attempts &&
                            CanRetryMethod(ctx);

        if (!should_retry) {
            break;
        }

        if (config_.retry.backoff_ms > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.retry.backoff_ms)
            );
        }
    }

    if (!has_response) {
        return BuildErrorResponse(
            500,
            ErrorCode::INTERNAL_ERROR,
            "governance call produced no response",
            ctx.request_id
        );
    }

    if (IsFailureResponse(last_response) && config_.fallback.enabled) {
        return FallbackResponse(ctx, "all retry attempts failed");
    }

    return last_response;
}

bool GovernanceUpstreamClient::AllowRequest(const std::string& upstream) {
    if (!config_.circuit_breaker.enabled) {
        return true;
    }

    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = circuit_states_[upstream];

    if (state.state == CircuitState::OPEN) {
        if (now >= state.open_until) {
            state.state = CircuitState::HALF_OPEN;
            state.consecutive_successes = 0;

            TGW_WARN(
                "circuit breaker enters half-open, upstream={}",
                upstream
            );

            return true;
        }

        return false;
    }

    return true;
}

void GovernanceUpstreamClient::RecordResult(const std::string& upstream, bool success) {
    if (!config_.circuit_breaker.enabled) {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = circuit_states_[upstream];

    if (success) {
        state.consecutive_failures = 0;

        if (state.state == CircuitState::HALF_OPEN) {
            ++state.consecutive_successes;

            if (state.consecutive_successes >=
                config_.circuit_breaker.half_open_success_threshold) {
                TGW_WARN(
                    "circuit breaker closed after half-open success, upstream={}",
                    upstream
                );

                state.state = CircuitState::CLOSED;
                state.consecutive_successes = 0;
            }
        }

        return;
    }

    state.consecutive_successes = 0;
    ++state.consecutive_failures;

    if (state.state == CircuitState::HALF_OPEN ||
        state.consecutive_failures >= config_.circuit_breaker.failure_threshold) {
        state.state = CircuitState::OPEN;
        state.open_until = now + std::chrono::seconds(
            config_.circuit_breaker.open_seconds
        );

        TGW_ERROR(
            "circuit breaker opened, upstream={}, failures={}, open_seconds={}",
            upstream,
            state.consecutive_failures,
            config_.circuit_breaker.open_seconds
        );
    }
}

bool GovernanceUpstreamClient::CanRetryMethod(const ForwardContext& ctx) const {
    if (IsIdempotentMethod(ctx.method)) {
        return true;
    }

    return config_.retry.retry_non_idempotent;
}

bool GovernanceUpstreamClient::IsFailureResponse(const HttpResponse& response) const {
    return response.status == 504 || response.status >= 500;
}

bool GovernanceUpstreamClient::IsIdempotentMethod(const std::string& method) const {
    return method == "GET" || method == "HEAD" || method == "OPTIONS";
}

HttpResponse GovernanceUpstreamClient::CallOnce(const ForwardContext& ctx) {
    int debug_sleep_ms = HeaderToInt(ctx.headers, "X-Debug-Sleep-Ms", 0);
    if (debug_sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(debug_sleep_ms));
    }

    int debug_force_status = HeaderToInt(ctx.headers, "X-Debug-Force-Status", 0);
    if (debug_force_status >= 500) {
        HttpResponse rsp = BuildErrorResponse(
            debug_force_status,
            ErrorCode::RPC_FAILED,
            "debug forced upstream failure",
            ctx.request_id
        );

        rsp.headers["X-Debug-Forced"] = "true";
        return rsp;
    }

    return inner_->Forward(ctx);
}

HttpResponse GovernanceUpstreamClient::TimeoutResponse(
    const ForwardContext& ctx,
    int attempt
) const {
    HttpResponse rsp = BuildErrorResponse(
        504,
        ErrorCode::RPC_TIMEOUT,
        "upstream timeout",
        ctx.request_id
    );

    rsp.headers["X-Governance-Timeout"] = "true";
    rsp.headers["X-Governance-Attempt"] = std::to_string(attempt);

    return rsp;
}

HttpResponse GovernanceUpstreamClient::CircuitOpenResponse(const ForwardContext& ctx) const {
    if (config_.fallback.enabled) {
        return FallbackResponse(ctx, "circuit breaker open");
    }

    HttpResponse rsp = BuildErrorResponse(
        503,
        ErrorCode::RPC_FAILED,
        "circuit breaker open",
        ctx.request_id
    );

    rsp.headers["X-Circuit-State"] = "open";
    return rsp;
}

HttpResponse GovernanceUpstreamClient::FallbackResponse(
    const ForwardContext& ctx,
    const std::string& reason
) const {
    std::ostringstream data;

    data << "{";
    data << "\"degraded\":true,";
    data << "\"upstream\":\"" << JsonEscape(ctx.upstream) << "\",";
    data << "\"reason\":\"" << JsonEscape(reason) << "\",";
    data << "\"message\":\"" << JsonEscape(config_.fallback.message) << "\"";
    data << "}";

    HttpResponse rsp;
    rsp.status = 200;
    rsp.content_type = "application/json";
    rsp.body = ApiResponse::Success(data.str()).ToJson();
    rsp.headers["X-Request-Id"] = ctx.request_id;
    rsp.headers["X-Governance-Fallback"] = "true";
    rsp.headers["X-Circuit-State"] = "open";

    return rsp;
}

} // namespace tgw