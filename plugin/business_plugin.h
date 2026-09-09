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

// 单个业务插件只读消费后处理完成的检测对象及外部生成的 track_id。
// OnFrame 在管线线程调用；异步工作必须使用 Name() 作为
// HostServices::SubmitAsync 的 plugin_name。宿主在 Init 前保证 Name() 非空且
// 与配置名称一致，并在 Init 被调用后的所有退出路径先等待异步任务归零，再调用
// Shutdown。即使 Init 返回失败，插件也必须允许 Shutdown 清理部分初始化状态；
// Shutdown 必须幂等且不得假设 Init 已完整成功。
class BusinessPlugin {
public:
    virtual ~BusinessPlugin() = default;

    virtual const char* Name() const = 0;

    virtual uint32_t ApiVersion() const = 0;

    virtual int Init(HostServices* host, const PluginConfig& cfg) = 0;

    // objects 为只读的后处理最终结果，track_id 由外部后处理器提供。
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
