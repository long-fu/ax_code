#pragma once

#include <memory>
#include <string>
#include <vector>

#include "business_plugin.h"
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

    // names: 逗号分隔的插件名列表已拆好。dir 为 .so 搜索目录。
    // 优先静态注册表，未命中再 dlopen lib{name}.so。
    int Load(HostServices* host, const std::vector<std::string>& names,
             const std::string& dir);

    int OnFrame(const ImageData& frame,
                const std::vector<detection::Object>& objects);

    // 停止投喂 → WaitQuiesce → Shutdown → Destroy → dlclose
    void Unload();

private:
    struct Loaded {
        std::string name;
        BusinessPlugin* plugin = nullptr;
        DestroyPluginFn destroy = nullptr;
        void* so_handle = nullptr;
        bool from_static = false;
    };

    HostServices* host_ = nullptr;
    std::vector<Loaded> loaded_;
};

std::vector<std::string> ParsePluginList(const char* spec);
std::string DefaultPluginDir();

}  // namespace plugin
