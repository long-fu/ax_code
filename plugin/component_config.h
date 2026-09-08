#pragma once

#include <cstdint>
#include <string>

namespace plugin
{

    struct ComponentConfig
    {
        std::string name;
        std::string library_path;
        std::string config_dir;
        std::string params_yaml;
        uint32_t api_version = 0;
        bool enabled = true;
    };

} // namespace plugin
