#pragma once

#include <cstdint>

struct BusFrameDispatchPlan
{
    bool run_runtime = false;
    uint8_t forward_count = 0;
};

constexpr BusFrameDispatchPlan MakeBusFrameDispatchPlan(
    bool has_valid_input, bool pixels_accessible)
{
    if (!has_valid_input)
    {
        return {};
    }
    return {pixels_accessible, 1};
}
