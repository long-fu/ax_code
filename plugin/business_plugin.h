#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "detection_types.h"
#include "image_data.h"

class HostServices;

struct PluginConfig {
    std::string name;
    std::string library_path;
    std::string config_dir;
    std::string params_yaml;
    uint32_t api_version = 0;
};

inline constexpr uint32_t kBusinessPluginApiVersion = 1;

// 业务插件：管线只把「帧 + 检测结果」交给插件。
// OnFrame 在管线线程调用；异步工作必须走 HostServices::SubmitAsync。
class BusinessPlugin {
public:
    virtual ~BusinessPlugin() = default;

    virtual const char* Name() const = 0;

    virtual uint32_t ApiVersion() const = 0;

    virtual int Init(HostServices* host, const PluginConfig& cfg) = 0;

    // objects 为 const：多插件共享同一帧时互不污染。需要 track_id 时自行拷贝。
    virtual int OnFrame(const ImageData& frame,
                        const std::vector<detection::Object>& objects) = 0;

    virtual void Shutdown() = 0;
};

// 动态库导出的 C ABI 工厂。插件与宿主须用同一交叉工具链编译。
extern "C" {
typedef BusinessPlugin* (*CreatePluginFn)();
typedef void (*DestroyPluginFn)(BusinessPlugin*);
}

inline constexpr const char* kPluginCreateSymbol = "CreatePlugin";
inline constexpr const char* kPluginDestroySymbol = "DestroyPlugin";
