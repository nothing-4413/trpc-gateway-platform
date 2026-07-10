#pragma once

#include "tgw/common/deadline.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace tgw {

// RPC 调用上下文。
// 它不是业务参数，而是一次 RPC 调用的元信息。
struct RpcContext {
    std::string request_id;
    std::string trace_id;
    std::string caller;
    std::string user_id;

    int timeout_ms = 1000;
    DeadlineContextPtr deadline;

    std::map<std::string, std::string> headers;

    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

    int64_t ElapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    }

    bool DeadlineExceeded() const {
        return deadline && deadline->IsDone();
    }

    void CancelDeadline(const std::string& reason) const {
        if (deadline) {
            deadline->Cancel(reason);
        }
    }

    std::string DeadlineReason() const {
        if (!deadline) {
            return "";
        }
        return deadline->Reason();
    }
};

} // namespace tgw
