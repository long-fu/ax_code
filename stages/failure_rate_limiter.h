#pragma once

#include <chrono>
#include <cstdint>

enum class FailureReportEvent
{
    kNone,
    kFirst,
    kSummary,
    kRecovery,
};

struct FailureReport
{
    FailureReportEvent event = FailureReportEvent::kNone;
    uint64_t episode_failures = 0;
    uint64_t code_changes = 0;
    int latest_code = 0;
    int64_t duration_seconds = 0;
};

class FailureRateLimiter
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit constexpr FailureRateLimiter(std::chrono::seconds interval)
        : interval_(interval)
    {
    }

    FailureReport OnFailure(int code, TimePoint now)
    {
        if (episode_failures_ == 0)
        {
            started_ = now;
            last_report_ = now;
            latest_code_ = code;
            episode_failures_ = 1;
            return Snapshot(FailureReportEvent::kFirst, now);
        }

        ++episode_failures_;
        if (latest_code_ != code)
        {
            latest_code_ = code;
            ++code_changes_;
        }
        if (now - last_report_ >= interval_)
        {
            last_report_ = now;
            return Snapshot(FailureReportEvent::kSummary, now);
        }
        return Snapshot(FailureReportEvent::kNone, now);
    }

    FailureReport OnRecovery(TimePoint now)
    {
        if (episode_failures_ == 0)
        {
            return {};
        }
        const FailureReport report =
            Snapshot(FailureReportEvent::kRecovery, now);
        episode_failures_ = 0;
        code_changes_ = 0;
        latest_code_ = 0;
        return report;
    }

private:
    FailureReport Snapshot(FailureReportEvent event, TimePoint now) const
    {
        return {event,
                episode_failures_,
                code_changes_,
                latest_code_,
                std::chrono::duration_cast<std::chrono::seconds>(now -
                                                                  started_)
                    .count()};
    }

    std::chrono::seconds interval_;
    TimePoint started_{};
    TimePoint last_report_{};
    uint64_t episode_failures_ = 0;
    uint64_t code_changes_ = 0;
    int latest_code_ = 0;
};
