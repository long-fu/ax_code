#pragma once

#include <cstdint>
#include <vector>

#include "detection_types.h"
#include "host_services.h"
#include "image_data.h"
#include "plugin_loader.h"
#include "postprocessor_chain.h"
#include "scene_config.h"

namespace plugin
{

enum class SceneProcessStage
{
    kOk,
    kPostProcessor,
    kBusinessPlugin,
};

struct SceneProcessResult
{
    SceneProcessStage stage = SceneProcessStage::kOk;
    int code = 0;
    bool Ok() const { return code == 0; }
};

class SceneRuntime
{
public:
    SceneRuntime() = default;
    ~SceneRuntime();

    SceneRuntime(const SceneRuntime&) = delete;
    SceneRuntime& operator=(const SceneRuntime&) = delete;

    int Init(HostServices* host, const SceneConfig& config);
    SceneProcessResult Process(const ImageData& frame,
                               std::vector<detection::Object>& objects,
                               uint64_t frame_seq);
    void Shutdown();

private:
    PostProcessorChain postprocessors_;
    PluginManager business_;
};

} // namespace plugin
