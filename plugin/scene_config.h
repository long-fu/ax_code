#pragma once

#include <string>
#include <vector>

#include "component_config.h"

namespace plugin
{

    struct SceneConfig
    {
        ComponentConfig scene;
        std::vector<ComponentConfig> postprocessors;
    };

    std::string DefaultAppConfigPath();

    bool LoadSceneConfigPath(const std::string& app_config_path,
                             std::string& scene_config_path,
                             std::string& error);

    bool LoadSceneConfig(const std::string& scene_config_path,
                         SceneConfig& config,
                         std::string& error);

} // namespace plugin
