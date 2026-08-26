#include "plugin_loader.h"

#include <cstdlib>
#include <dlfcn.h>
#include <linux/limits.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

#include "logger.h"
#include "plugin_registry.h"

namespace plugin {
namespace {

std::string Trim(std::string s)
{
    auto not_space = [](unsigned char c) { return c != ' ' && c != '\t'; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string ExeDir()
{
    char buf[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        return ".";
    }
    buf[n] = '\0';
    std::string path(buf);
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos)
    {
        return ".";
    }
    return path.substr(0, slash);
}

}  // namespace

PluginManager::~PluginManager()
{
    Unload();
}

std::vector<std::string> ParsePluginList(const char* spec)
{
    if (spec == nullptr || spec[0] == '\0')
    {
        return {"face_recognition"};
    }
    std::vector<std::string> names;
    std::stringstream ss(spec);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        item = Trim(std::move(item));
        if (!item.empty())
        {
            names.push_back(std::move(item));
        }
    }
    if (names.empty())
    {
        names.push_back("face_recognition");
    }
    return names;
}

std::string DefaultPluginDir()
{
    if (const char* env = std::getenv("AX_PLUGIN_DIR"))
    {
        if (env[0] != '\0')
        {
            return env;
        }
    }
    return ExeDir() + "/plugins";
}

int PluginManager::Load(HostServices* host, const std::vector<std::string>& names,
                        const std::string& dir)
{
    Unload();
    host_ = host;
    if (host_ == nullptr)
    {
        LOG_ERROR("PluginManager: host is null");
        return -1;
    }

    for (const auto& name : names)
    {
        Loaded loaded;
        loaded.name = name;

        const std::string so_path = dir + "/lib" + name + ".so";
        void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        const char* dlopen_err = handle ? nullptr : dlerror();
        if (handle != nullptr)
        {
            auto create = reinterpret_cast<CreatePluginFn>(
                dlsym(handle, kPluginCreateSymbol));
            auto destroy = reinterpret_cast<DestroyPluginFn>(
                dlsym(handle, kPluginDestroySymbol));
            if (create == nullptr || destroy == nullptr)
            {
                LOG_ERROR("插件 {} 缺少 CreatePlugin/DestroyPlugin: {}", name,
                          dlerror());
                dlclose(handle);
                Unload();
                return -1;
            }
            loaded.plugin = create();
            loaded.destroy = destroy;
            loaded.so_handle = handle;
            loaded.from_static = false;
        }
        else if (HasRegisteredPlugin(name))
        {
            auto plugin = CreateRegisteredPlugin(name);
            if (!plugin)
            {
                LOG_ERROR("静态注册插件 {} 创建失败", name);
                Unload();
                return -1;
            }
            loaded.plugin = plugin.release();
            loaded.destroy = nullptr;
            loaded.so_handle = nullptr;
            loaded.from_static = true;
            LOG_INFO("插件 {} 使用静态注册（未找到 {}）", name, so_path);
        }
        else
        {
            LOG_ERROR("无法加载插件 {}: dlopen({}) 失败: {}", name, so_path,
                      dlopen_err ? dlopen_err : "unknown");
            Unload();
            return -1;
        }

        if (loaded.plugin == nullptr)
        {
            LOG_ERROR("插件 {} 工厂返回空指针", name);
            if (loaded.so_handle)
            {
                dlclose(loaded.so_handle);
            }
            Unload();
            return -1;
        }

        PluginConfig cfg;
        cfg.name = name;
        cfg.plugin_dir = dir;
        const int ret = loaded.plugin->Init(host_, cfg);
        if (ret != 0)
        {
            LOG_ERROR("插件 {} Init 失败, ret={}", name, ret);
            if (loaded.destroy)
            {
                loaded.destroy(loaded.plugin);
            }
            else
            {
                delete loaded.plugin;
            }
            if (loaded.so_handle)
            {
                dlclose(loaded.so_handle);
            }
            Unload();
            return ret;
        }

        LOG_INFO("已加载插件 {} ({})", name,
                 loaded.from_static ? "static" : so_path.c_str());
        loaded_.push_back(loaded);
    }
    return 0;
}

int PluginManager::OnFrame(const ImageData& frame,
                           const std::vector<detection::Object>& objects)
{
    int last_err = 0;
    for (auto& item : loaded_)
    {
        if (item.plugin == nullptr)
        {
            continue;
        }
        const int ret = item.plugin->OnFrame(frame, objects);
        if (ret != 0)
        {
            LOG_ERROR("插件 {} OnFrame 失败, ret={}", item.name, ret);
            last_err = ret;
        }
    }
    return last_err;
}

void PluginManager::Unload()
{
    // 逆序：停止投喂（不再 OnFrame）→ WaitQuiesce → Shutdown → Destroy → dlclose
    for (auto it = loaded_.rbegin(); it != loaded_.rend(); ++it)
    {
        if (host_ != nullptr)
        {
            host_->WaitQuiesce(it->name, -1);
        }
        if (it->plugin != nullptr)
        {
            it->plugin->Shutdown();
            if (it->destroy)
            {
                it->destroy(it->plugin);
            }
            else
            {
                delete it->plugin;
            }
            it->plugin = nullptr;
        }
        if (it->so_handle != nullptr)
        {
            dlclose(it->so_handle);
            it->so_handle = nullptr;
        }
    }
    loaded_.clear();
}

}  // namespace plugin
