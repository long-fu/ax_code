#include "failure_rate_limiter.h"

#include <chrono>
#include <iostream>

namespace
{

void Expect(bool condition, const char* message, int& failures)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    using namespace std::chrono_literals;
    int failures = 0;
    FailureRateLimiter limiter(5s);
    const FailureRateLimiter::TimePoint start{};

    auto report = limiter.OnFailure(-1, start);
    Expect(report.event == FailureReportEvent::kFirst,
           "first failure reports immediately", failures);

    for (int frame = 1; frame < 100; ++frame)
    {
        const int code = frame % 2 == 0 ? -1 : -2;
        report = limiter.OnFailure(code, start + frame * 10ms);
        Expect(report.event == FailureReportEvent::kNone,
               "alternating high-frequency codes do not report per frame",
               failures);
    }

    report = limiter.OnFailure(-1, start + 5s);
    Expect(report.event == FailureReportEvent::kSummary,
           "five seconds emits one sustained-failure summary", failures);
    Expect(report.episode_failures == 101,
           "summary counts the entire failure episode", failures);
    Expect(report.latest_code == -1, "summary reports the latest code",
           failures);
    Expect(report.code_changes == 100,
           "summary reports every code transition", failures);
    Expect(report.duration_seconds == 5,
           "summary duration starts at the first failure", failures);

    report = limiter.OnRecovery(start + 6s);
    Expect(report.event == FailureReportEvent::kRecovery,
           "recovery emits one report", failures);
    Expect(report.episode_failures == 101 && report.duration_seconds == 6,
           "recovery retains full episode totals and duration", failures);
    Expect(report.latest_code == -1 && report.code_changes == 100,
           "recovery retains final code metadata", failures);
    Expect(limiter.OnRecovery(start + 7s).event == FailureReportEvent::kNone,
           "repeated success does not repeat recovery", failures);

    if (failures != 0)
    {
        std::cerr << "failure_rate_limiter_test: " << failures
                  << " failure(s)\n";
        return 1;
    }
    std::cout << "failure_rate_limiter_test: PASS\n";
    return 0;
}
