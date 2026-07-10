#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace tgw {

class DeadlineContext {
public:
    using Clock = std::chrono::steady_clock;

    static std::shared_ptr<DeadlineContext> Start(int timeout_ms) {
        if (timeout_ms <= 0) {
            timeout_ms = 1;
        }
        return std::shared_ptr<DeadlineContext>(new DeadlineContext(timeout_ms));
    }

    bool IsExpired() const {
        return Clock::now() >= deadline_;
    }

    bool IsCancelled() const {
        return cancelled_.load();
    }

    bool IsDone() const {
        return IsCancelled() || IsExpired();
    }

    void Cancel(const std::string& reason) {
        bool expected = false;
        if (!cancelled_.compare_exchange_strong(expected, true)) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        cancel_reason_ = reason.empty() ? "cancelled" : reason;
    }

    std::string Reason() const {
        if (IsExpired()) {
            return "deadline exceeded";
        }

        std::lock_guard<std::mutex> lock(mutex_);
        return cancel_reason_;
    }

    int64_t ElapsedMs() const {
        auto now = Clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

    int64_t RemainingMs() const {
        auto now = Clock::now();
        if (now >= deadline_) {
            return 0;
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now).count();
    }

private:
    explicit DeadlineContext(int timeout_ms)
        : start_(Clock::now()),
          deadline_(start_ + std::chrono::milliseconds(timeout_ms)) {}

private:
    Clock::time_point start_;
    Clock::time_point deadline_;
    std::atomic<bool> cancelled_{false};

    mutable std::mutex mutex_;
    std::string cancel_reason_;
};

using DeadlineContextPtr = std::shared_ptr<DeadlineContext>;

} // namespace tgw
