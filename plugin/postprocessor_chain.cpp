#include "postprocessor_chain.h"

#include <dlfcn.h>

#include <iostream>
#include <string>
#include <utility>

namespace plugin
{
namespace
{

std::string FailureMessage(const std::string& name,
                           const std::string& library_path,
                           const char* stage, const std::string& detail)
{
    std::string message = "PostProcessorChain: component='" + name +
                          "' path='" + library_path + "' stage='" + stage +
                          "' failed";
    if (!detail.empty())
    {
        message += ": " + detail;
    }
    return message;
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

void PostProcessorChain::SetStartupFailure(const ComponentConfig& config,
                                           const char* stage,
                                           const std::string& detail)
{
    last_error_ =
        FailureMessage(config.name, config.library_path, stage, detail);
    std::cerr << last_error_ << '\n';
}

int PostProcessorChain::Load(const std::vector<ComponentConfig>& configs)
{
    Unload();
    last_error_.clear();

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
            SetStartupFailure(config, "dlopen",
                              error == nullptr ? "unknown" : error);
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
            SetStartupFailure(config, "dlsym", detail);
            dlclose(handle);
            Unload();
            return -1;
        }

        PostProcessor* processor = create();
        if (processor == nullptr)
        {
            SetStartupFailure(config, "create", "factory returned null");
            dlclose(handle);
            Unload();
            return -1;
        }

        if (config.api_version != kPostProcessorApiVersion)
        {
            SetStartupFailure(config, "config-api-version",
                              "expected " +
                                  std::to_string(kPostProcessorApiVersion) +
                                  ", got " +
                                  std::to_string(config.api_version));
            DestroyUnloaded(processor, destroy, handle);
            Unload();
            return -1;
        }

        const uint32_t instance_version = processor->ApiVersion();
        if (instance_version != kPostProcessorApiVersion)
        {
            SetStartupFailure(config, "instance-api-version",
                              "expected " +
                                  std::to_string(kPostProcessorApiVersion) +
                                  ", got " +
                                  std::to_string(instance_version));
            DestroyUnloaded(processor, destroy, handle);
            Unload();
            return -1;
        }

        const int init_result = processor->Init(config);
        if (init_result != 0)
        {
            SetStartupFailure(config, "init",
                              "status " + std::to_string(init_result));
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
    last_error_.clear();
    for (auto& loaded : loaded_)
    {
        const int result = loaded.processor->Process(context);
        if (result != 0)
        {
            last_error_ = FailureMessage(loaded.name, loaded.library_path,
                                         "process",
                                         "status " + std::to_string(result));
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

const std::string& PostProcessorChain::LastError() const
{
    return last_error_;
}

} // namespace plugin
