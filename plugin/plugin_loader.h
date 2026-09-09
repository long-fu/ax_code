#pragma once

#include <string>
#include <vector>

#include "business_plugin.h"
#include "component_config.h"
#include "detection_types.h"
#include "host_services.h"
#include "image_data.h"

namespace plugin {

class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    int Load(HostServices* host, const ComponentConfig& config);

    int OnFrame(const ImageData& frame,
                const std::vector<detection::Object>& objects);

    // 停止投喂 → WaitQuiesce → Shutdown → Destroy → dlclose
    void Unload();
    bool Loaded() const;

private:
    HostServices* host_ = nullptr;
    std::string name_;
    BusinessPlugin* plugin_ = nullptr;
    DestroyPluginFn destroy_ = nullptr;
    void* so_handle_ = nullptr;
};

}  // namespace plugin
