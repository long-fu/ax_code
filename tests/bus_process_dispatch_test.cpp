#include "bus_process_dispatch.h"

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
    int failures = 0;

    const auto ready = MakeBusFrameDispatchPlan(true, true);
    Expect(ready.run_runtime, "accessible pixels run the scene runtime",
           failures);
    Expect(ready.forward_count == 1,
           "runtime outcome cannot suppress the single forward", failures);

    const auto map_failed = MakeBusFrameDispatchPlan(true, false);
    Expect(!map_failed.run_runtime,
           "mapping failure skips pixel-consuming scene code", failures);
    Expect(map_failed.forward_count == 1,
           "mapping failure still forwards exactly once", failures);

    const auto missing = MakeBusFrameDispatchPlan(false, false);
    Expect(!missing.run_runtime && missing.forward_count == 0,
           "missing input is neither processed nor forwarded", failures);

    if (failures != 0)
    {
        std::cerr << "bus_process_dispatch_test: " << failures
                  << " failure(s)\n";
        return 1;
    }
    std::cout << "bus_process_dispatch_test: PASS\n";
    return 0;
}
