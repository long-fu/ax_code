// common/RuleEngine.hpp
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include "box_rule.h"

struct RuleConfig {
    std::string name;       // Plugin name matching registered rule name
    std::string so_path;    // Path to .so file
    std::string params;     // YAML/JSON parameter string
};

class RuleEngine {
public:
    static RuleEngine& Instance();

    // Load configuration from YAML string and initialize all plugins
    // Returns 0 on success, error code if no rules could be loaded
    int Load(const std::string& yaml_config);

    // Process detection boxes, return hit results (OR of all rules)
    // Returns false if no rules are loaded or an internal error occurred
    bool ProcessBoxes(
        const std::vector<detection::Object>& objects,
        std::vector<bool>& results) const;

    // Release all plugin resources
    void Unload();

private:
    struct LoadedRule {
        void* handle = nullptr;      // dlopen handle
        BoxRule* instance = nullptr;  // Plugin instance
        RuleConfig config;
    };

    mutable std::mutex mutex_;
    std::vector<LoadedRule> rules_;
    bool loaded_ = false;
};
