#include "business_plugin.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace
{

char g_lifecycle_file[1024] = {};
bool g_log_frames = false;

std::string ValueFor(const std::string& yaml, const std::string& key)
{
    const std::string prefix = key + ":";
    const size_t begin = yaml.find(prefix);
    if (begin == std::string::npos)
    {
        return {};
    }
    const size_t value_begin = begin + prefix.size();
    const size_t end = yaml.find('\n', value_begin);
    std::string value = yaml.substr(value_begin, end - value_begin);
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                             [](unsigned char c) {
                                                 return !std::isspace(c);
                                             }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char c) {
                                 return !std::isspace(c);
                             }).base(),
                value.end());
    return value;
}

void AppendLifecycle(const char* phase)
{
    if (g_lifecycle_file[0] == '\0')
    {
        return;
    }
    std::ofstream output(g_lifecycle_file, std::ios::app);
    output << phase << '\n';
}

class OkBusinessPlugin final : public BusinessPlugin
{
public:
    const char* Name() const override
    {
        return "scene";
    }

    uint32_t ApiVersion() const override
    {
        return kBusinessPluginApiVersion;
    }

    int Init(HostServices*, const PluginConfig& config) override
    {
        if (config.name.empty() || config.library_path.empty() ||
            config.config_dir != "/scene/config" ||
            config.api_version != kBusinessPluginApiVersion)
        {
            return -67;
        }
        const std::string lifecycle_file =
            ValueFor(config.params_yaml, "lifecycle_file");
        std::snprintf(g_lifecycle_file, sizeof(g_lifecycle_file), "%s",
                      lifecycle_file.c_str());
        g_log_frames =
            ValueFor(config.params_yaml, "log_frames") == "true";
        AppendLifecycle("init");

        std::string normalized = config.params_yaml;
        normalized.erase(
            std::remove_if(normalized.begin(), normalized.end(),
                           [](unsigned char c) { return std::isspace(c); }),
            normalized.end());
        return normalized.find("fail_init:true") != std::string::npos ? -66
                                                                       : 0;
    }

    int OnFrame(const ImageData&,
                const std::vector<detection::Object>& objects) override
    {
        if (g_log_frames)
        {
            AppendLifecycle("frame");
        }
        return !objects.empty() && objects.front().track_id == 42 ? 0 : -42;
    }

    void Shutdown() override
    {
        AppendLifecycle("shutdown");
    }
};

} // namespace

extern "C" __attribute__((visibility("default")))
BusinessPlugin* CreatePlugin()
{
    return new OkBusinessPlugin();
}

extern "C" __attribute__((visibility("default")))
void DestroyPlugin(BusinessPlugin* plugin)
{
    AppendLifecycle("destroy");
    delete plugin;
}

__attribute__((destructor)) static void OnLibraryUnload()
{
    AppendLifecycle("dlclose");
}
