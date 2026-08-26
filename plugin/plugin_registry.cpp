#include "plugin_registry.h"

#include <mutex>
#include <unordered_map>

namespace plugin {
namespace {

std::mutex g_mu;
std::unordered_map<std::string, PluginFactory> g_factories;

}  // namespace

void RegisterPlugin(const std::string& name, PluginFactory factory)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_factories[name] = std::move(factory);
}

std::unique_ptr<BusinessPlugin> CreateRegisteredPlugin(const std::string& name)
{
    PluginFactory factory;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto it = g_factories.find(name);
        if (it == g_factories.end())
        {
            return nullptr;
        }
        factory = it->second;
    }
    return factory ? factory() : nullptr;
}

bool HasRegisteredPlugin(const std::string& name)
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_factories.find(name) != g_factories.end();
}

}  // namespace plugin
