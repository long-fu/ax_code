#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "component_config.h"
#include "detection_types.h"
#include "image_data.h"

inline constexpr uint32_t kPostProcessorApiVersion = 1;

namespace plugin
{

struct PostProcessContext
{
    const ImageData& frame;
    std::vector<detection::Object>& objects;
    uint64_t frame_seq;
    std::chrono::steady_clock::time_point timestamp;
};

} // namespace plugin

class PostProcessor
{
public:
    virtual ~PostProcessor() = default;
    virtual const char* Name() const = 0;
    virtual uint32_t ApiVersion() const = 0;
    virtual int Init(const plugin::ComponentConfig& config) = 0;
    virtual int Process(plugin::PostProcessContext& context) = 0;
    virtual void Shutdown() = 0;
};

extern "C"
{
    typedef PostProcessor* (*CreatePostProcessorFn)();
    typedef void (*DestroyPostProcessorFn)(PostProcessor*);
}

inline constexpr const char* kPostProcessorCreateSymbol =
    "CreatePostProcessor";
inline constexpr const char* kPostProcessorDestroySymbol =
    "DestroyPostProcessor";
