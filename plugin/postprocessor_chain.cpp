#include "postprocessor_chain.h"

#include <dlfcn.h>

#include <iostream>
#include <string>
#include <utility>

namespace plugin
{
namespace
{

void LogFailure(const ComponentConfig& config, const char* stage,
                const std::string& detail)
{
    std::cerr << "PostProcessorChain: component='" << config.name
              << "' path='" << config.library_path << "' stage='" << stage
              << "' failed";
    if (!detail.empty())
    {
        std::cerr << ": " << detail;
    }
    std::cerr << '\n';
}

void DestroyUnloaded(PostProcessor* processor, DestroyPostProcessorFn destroy,
                     void* handle)
{
    if (processor != nullptr && destroy != nullptr)
    {
        destroy(processor);
    }
    if (handle != nullptr)
    {
        dlclose(handle);
    }
}

} // namespace

PostProcessorChain::~PostProcessorChain()
{
    Unload();
}

int PostProcessorChain::Load(const std::vector<ComponentConfig>& configs)
{
    Unload();

    for (const auto& config : configs)
    {
        if (!config.enabled)
        {
            continue;
        }

        void* handle =
            dlopen(config.library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr)
        {
            const char* error = dlerror();
            LogFailure(config, "dlopen", error == nullptr ? "unknown" : error);
            Unload();
            return -1;
        }

        dlerror();
        auto create = reinterpret_cast<CreatePostProcessorFn>(
            dlsym(handle, kPostProcessorCreateSymbol));
        const char* create_error_text = dlerror();
        const std::string create_error =
            create_error_text == nullptr ? "" : create_error_text;
        dlerror();
        auto destroy = reinterpret_cast<DestroyPostProcessorFn>(
            dlsym(handle, kPostProcessorDestroySymbol));
        const char* destroy_error_text = dlerror();
        const std::string destroy_error =
            destroy_error_text == nullptr ? "" : destroy_error_text;
        if (!create_error.empty() || !destroy_error.empty() ||
            create == nullptr || destroy == nullptr)
        {
            std::string detail;
            if (!create_error.empty())
            {
                detail = create_error;
            }
            else if (!destroy_error.empty())
            {
                detail = destroy_error;
            }
            else
            {
                detail = "factory symbol resolved to null";
            }
            LogFailure(config, "dlsym", detail);
            dlclose(handle);
            Unload();
            return -1;
        }

        PostProcessor* processor = create();
        if (processor == nullptr)
        {
            LogFailure(config, "create", "factory returned null");
            dlclose(handle);
            Unload();
            return -1;
        }

        if (config.api_version != kPostProcessorApiVersion)
        {
            LogFailure(config, "config-api-version",
                       "expected " +
                           std::to_string(kPostProcessorApiVersion) +
                           ", got " + std::to_string(config.api_version));
            DestroyUnloaded(processor, destroy, handle);
            Unload();
            return -1;
        }

        const uint32_t instance_version = processor->ApiVersion();
        if (instance_version != kPostProcessorApiVersion)
        {
            LogFailure(config, "instance-api-version",
                       "expected " +
                           std::to_string(kPostProcessorApiVersion) +
                           ", got " + std::to_string(instance_version));
            DestroyUnloaded(processor, destroy, handle);
            Unload();
            return -1;
        }

        const int init_result = processor->Init(config);
        if (init_result != 0)
        {
            LogFailure(config, "init", "status " +
                                           std::to_string(init_result));
            DestroyUnloaded(processor, destroy, handle);
            Unload();
            return init_result;
        }

        Loaded loaded;
        loaded.name = config.name;
        loaded.library_path = config.library_path;
        loaded.so_handle = handle;
        loaded.processor = processor;
        loaded.destroy = destroy;
        loaded_.push_back(std::move(loaded));
    }

    return 0;
}

int PostProcessorChain::Process(PostProcessContext& context)
{
    for (auto& loaded : loaded_)
    {
        const int result = loaded.processor->Process(context);
        if (result != 0)
        {
            std::cerr << "PostProcessorChain: component='" << loaded.name
                      << "' path='" << loaded.library_path
                      << "' stage='process' failed: status " << result << '\n';
            return result;
        }
    }
    return 0;
}

void PostProcessorChain::Unload()
{
    for (auto it = loaded_.rbegin(); it != loaded_.rend(); ++it)
    {
        if (it->processor != nullptr)
        {
            it->processor->Shutdown();
            it->destroy(it->processor);
            it->processor = nullptr;
        }
        if (it->so_handle != nullptr)
        {
            dlclose(it->so_handle);
            it->so_handle = nullptr;
        }
    }
    loaded_.clear();
}

size_t PostProcessorChain::Size() const
{
    return loaded_.size();
}

} // namespace plugin
