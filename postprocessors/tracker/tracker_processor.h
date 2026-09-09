#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "BYTETracker.h"
#include "postprocessor.h"

class TrackerProcessor final : public PostProcessor
{
public:
    const char* Name() const override;
    uint32_t ApiVersion() const override;
    int Init(const plugin::ComponentConfig& config) override;
    int Process(plugin::PostProcessContext& context) override;
    void Shutdown() override;

private:
    std::unordered_set<std::string> track_labels_;
    std::unordered_map<std::string, std::unique_ptr<BYTETracker>> trackers_;
    int next_track_id_ = 1;
};
