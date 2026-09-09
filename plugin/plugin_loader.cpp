#include "plugin_loader.h"

#include <dlfcn.h>

#include <string>

#include "logger.h"

namespace plugin {
namespace {

void LogLoadFailure(const ComponentConfig& config, const char* stage,
                    const std::string& detail)
{
    LOG_ERROR("BusinessPlugin: scene='{}' library='{}' stage='{}' failed: {}",
              config.name, config.library_path, stage, detail);
}

void DestroyUnloaded(BusinessPlugin* plugin, DestroyPluginFn destroy,
                     void* handle, HostServices* host = nullptr,
                     const std::string& canonical_name = {},
                     bool init_invoked = false)
{
    if (plugin != nullptr && destroy != nullptr)
    {
        if (init_invoked)
        {
            if (host != nullptr)
            {
                host->WaitQuiesce(canonical_name, -1);
            }
            plugin->Shutdown();
        }
        destroy(plugin);
    }
    if (handle != nullptr)
    {
        dlclose(handle);
    }
}

}  // namespace

PluginManager::~PluginManager()
{
    Unload();
}

int PluginManager::Load(HostServices* host, const ComponentConfig& config)
{
    Unload();
    if (host == nullptr)
    {
        LogLoadFailure(config, "validate", "host is null");
        return -1;
    }

    void* handle = dlopen(config.library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
        const char* error = dlerror();
        LogLoadFailure(config, "dlopen", error == nullptr ? "unknown" : error);
        return -1;
    }

    dlerror();
    auto create =
        reinterpret_cast<CreatePluginFn>(dlsym(handle, kPluginCreateSymbol));
    const char* create_error_text = dlerror();
    const std::string create_error =
        create_error_text == nullptr ? "" : create_error_text;
    dlerror();
    auto destroy = reinterpret_cast<DestroyPluginFn>(
        dlsym(handle, kPluginDestroySymbol));
    const char* destroy_error_text = dlerror();
    const std::string destroy_error =
        destroy_error_text == nullptr ? "" : destroy_error_text;
    if (create == nullptr || destroy == nullptr || !create_error.empty() ||
        !destroy_error.empty())
    {
        const std::string detail = !create_error.empty()
                                       ? create_error
                                       : (!destroy_error.empty()
                                              ? destroy_error
                                              : "factory symbol resolved to null");
        LogLoadFailure(config, "dlsym", detail);
        dlclose(handle);
        return -1;
    }

    BusinessPlugin* plugin = create();
    if (plugin == nullptr)
    {
        LogLoadFailure(config, "create", "factory returned null");
        dlclose(handle);
        return -1;
    }

    const char* actual_name_text = plugin->Name();
    const std::string actual_name =
        actual_name_text == nullptr ? "" : actual_name_text;
    if (actual_name.empty() || actual_name != config.name)
    {
        const std::string display_name =
            actual_name_text == nullptr
                ? "<null>"
                : (actual_name.empty() ? "<empty>" : actual_name);
        LogLoadFailure(config, "name",
                       "plugin name '" + display_name +
                           "' does not match configured scene name");
        DestroyUnloaded(plugin, destroy, handle);
        return -1;
    }

    if (config.api_version != kBusinessPluginApiVersion)
    {
        LogLoadFailure(config, "config-api-version",
                       "expected " +
                           std::to_string(kBusinessPluginApiVersion) +
                           ", got " + std::to_string(config.api_version));
        DestroyUnloaded(plugin, destroy, handle);
        return -1;
    }

    const uint32_t instance_version = plugin->ApiVersion();
    if (instance_version != kBusinessPluginApiVersion)
    {
        LogLoadFailure(config, "instance-api-version",
                       "expected " +
                           std::to_string(kBusinessPluginApiVersion) +
                           ", got " + std::to_string(instance_version));
        DestroyUnloaded(plugin, destroy, handle);
        return -1;
    }

    PluginConfig plugin_config;
    plugin_config.name = config.name;
    plugin_config.library_path = config.library_path;
    plugin_config.config_dir = config.config_dir;
    plugin_config.params_yaml = config.params_yaml;
    plugin_config.api_version = config.api_version;
    const int init_result = plugin->Init(host, plugin_config);
    if (init_result != 0)
    {
        LogLoadFailure(config, "init",
                       "status " + std::to_string(init_result));
        DestroyUnloaded(plugin, destroy, handle, host, config.name, true);
        return init_result;
    }

    host_ = host;
    name_ = config.name;
    plugin_ = plugin;
    destroy_ = destroy;
    so_handle_ = handle;
    LOG_INFO("BusinessPlugin: loaded scene='{}' library='{}'", config.name,
             config.library_path);
    return 0;
}

int PluginManager::OnFrame(
    const ImageData& frame, const std::vector<detection::Object>& objects)
{
    if (!Loaded())
    {
        return -1;
    }
    return plugin_->OnFrame(frame, objects);
}

void PluginManager::Unload()
{
    if (plugin_ != nullptr)
    {
        if (host_ != nullptr)
        {
            host_->WaitQuiesce(name_, -1);
        }
        plugin_->Shutdown();
        destroy_(plugin_);
        plugin_ = nullptr;
    }
    if (so_handle_ != nullptr)
    {
        dlclose(so_handle_);
        so_handle_ = nullptr;
    }
    destroy_ = nullptr;
    host_ = nullptr;
    name_.clear();
}

bool PluginManager::Loaded() const
{
    return plugin_ != nullptr;
}

}  // namespace plugin
