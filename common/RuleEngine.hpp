// common/RuleEngine.hpp
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include "BoxRule.hpp"

struct RuleConfig {
    std::string name;       // Plugin name matching registered rule name
    std::string so_path;    // Path to .so file
    std::string params;     // YAML/JSON parameter string
};

class RuleEngine {
public:
    static RuleEngine& instance();

    // Load configuration from YAML string and initialize all plugins
    int load(const std::string& yaml_config);

    // Process detection boxes, return hit results (OR of all rules)
    std::vector<bool> processBoxes(
        const std::vector<detection::Object>& objects) const;

    // Release all plugin resources
    void unload();

private:
    struct LoadedRule {
        void* handle = nullptr;      // dlopen handle
        BoxRule* instance = nullptr;  // Plugin instance
        RuleConfig config;
    };

    mutable std::mutex mutex_;
    std::vector<LoadedRule> rules_;
};
