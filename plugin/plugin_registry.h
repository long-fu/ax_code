#pragma once

#include <functional>
#include <memory>
#include <string>

#include "business_plugin.h"

namespace plugin {

using PluginFactory = std::function<std::unique_ptr<BusinessPlugin>()>;

void RegisterPlugin(const std::string& name, PluginFactory factory);
std::unique_ptr<BusinessPlugin> CreateRegisteredPlugin(const std::string& name);
bool HasRegisteredPlugin(const std::string& name);

}  // namespace plugin
