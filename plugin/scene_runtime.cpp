#include "scene_runtime.h"

#include <chrono>

namespace plugin
{

SceneRuntime::~SceneRuntime()
{
    Shutdown();
}

int SceneRuntime::Init(HostServices* host, const SceneConfig& config)
{
    Shutdown();
    const int processor_result = postprocessors_.Load(config.postprocessors);
    if (processor_result != 0)
    {
        Shutdown();
        return processor_result;
    }

    const int business_result = business_.Load(host, config.scene);
    if (business_result != 0)
    {
        Shutdown();
        return business_result;
    }
    return 0;
}

SceneProcessResult SceneRuntime::Process(
    const ImageData& frame, std::vector<detection::Object>& objects,
    uint64_t frame_seq)
{
    for (auto& object : objects)
    {
        object.track_id = -1;
    }

    PostProcessContext context{frame, objects, frame_seq,
                               std::chrono::steady_clock::now()};
    const int processor_result = postprocessors_.Process(context);
    if (processor_result != 0)
    {
        return {SceneProcessStage::kPostProcessor, processor_result};
    }

    const int business_result = business_.OnFrame(frame, objects);
    if (business_result != 0)
    {
        return {SceneProcessStage::kBusinessPlugin, business_result};
    }
    return {};
}

void SceneRuntime::Shutdown()
{
    business_.Unload();
    postprocessors_.Unload();
}

} // namespace plugin
