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
    // Init 一旦被调用，宿主会在任何后续失败或正常退出路径调用 Shutdown，
    // 然后才销毁实例并关闭动态库。实现必须能清理部分初始化状态。
    virtual int Init(const plugin::ComponentConfig& config) = 0;
    virtual int Process(plugin::PostProcessContext& context) = 0;
    // 必须幂等，且不得假设 Init 已完整成功。
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
