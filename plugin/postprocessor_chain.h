#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "component_config.h"
#include "postprocessor.h"

namespace plugin
{

class PostProcessorChain
{
public:
    PostProcessorChain() = default;
    ~PostProcessorChain();

    PostProcessorChain(const PostProcessorChain&) = delete;
    PostProcessorChain& operator=(const PostProcessorChain&) = delete;

    int Load(const std::vector<ComponentConfig>& configs);
    int Process(PostProcessContext& context);
    void Unload();
    size_t Size() const;
    const std::string& LastError() const;

private:
    struct Loaded
    {
        std::string name;
        std::string library_path;
        void* so_handle = nullptr;
        PostProcessor* processor = nullptr;
        DestroyPostProcessorFn destroy = nullptr;
    };

    void SetStartupFailure(const ComponentConfig& config, const char* stage,
                           const std::string& detail);

    std::vector<Loaded> loaded_;
    std::string last_error_;
};

} // namespace plugin
