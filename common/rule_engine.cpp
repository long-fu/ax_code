// common/RuleEngine.cpp
#include "rule_engine.h"
#include "logger.h"
#include <dlfcn.h>
#include <sstream>
#include <algorithm>

RuleEngine& RuleEngine::Instance() {
    static RuleEngine inst;
    return inst;
}

int RuleEngine::Load(const std::string& yaml_config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Guard against double-load
    if (loaded_) {
        LOG_WARN("RuleEngine already loaded, ignoring duplicate load call");
        return -1;
    }

    // Parse YAML config format:
    // - name: sample_rule
    //   path: /path/to/libsample_rule.so
    //   params: |
    //     region: [[100,100],[300,100],[300,300],[100,300]]
    std::vector<RuleConfig> configs;
    std::istringstream stream(yaml_config);
    std::string line;
    RuleConfig* current = nullptr;
    std::stringstream param_buf;
    bool in_params = false;

    while (std::getline(stream, line)) {
        if (line.empty() || line.find("#") == 0) continue;

        if (line.find("name:") != std::string::npos) {
            if (current && !current->name.empty()) configs.push_back(*current);
            RuleConfig cfg{};
            current = &cfg;
            size_t pos = line.find(":");
            if (pos != std::string::npos)
                current->name = line.substr(pos + 1);
            current->name.erase(0, current->name.find_first_not_of(" \t"));
            current->name.erase(current->name.find_last_not_of(" \t\r\n") + 1);
            in_params = false;
        } else if (line.find("path:") != std::string::npos && current) {
            size_t pos = line.find(":");
            if (pos != std::string::npos)
                current->so_path = line.substr(pos + 1);
            current->so_path.erase(0, current->so_path.find_first_not_of(" \t"));
            current->so_path.erase(current->so_path.find_last_not_of(" \t\r\n") + 1);
            in_params = false;
        } else if (line.find("params:") != std::string::npos) {
            in_params = true;
            param_buf.str("");
            size_t pos = line.find(":");
            if (pos != std::string::npos && pos + 2 < line.size()) {
                std::string val = line.substr(pos + 1);
                val.erase(0, val.find_first_not_of(" \t"));
                if (!val.empty() && val[0] != '|') {
                    param_buf << val << "\n";
                    in_params = false;
                }
            }
        } else if (in_params && current) {
            param_buf << line << "\n";
        }
    }
    if (current && !current->name.empty()) configs.push_back(*current);

    // Load each plugin
    for (auto& cfg : configs) {
        LoadedRule loaded;
        loaded.config = cfg;

        // Restrict plugin paths: no empty, no "..", must end with .so, prefer rules/ prefix.
        if (cfg.so_path.empty() ||
            cfg.so_path.find("..") != std::string::npos ||
            cfg.so_path.size() < 3 ||
            cfg.so_path.substr(cfg.so_path.size() - 3) != ".so") {
            LOG_ERROR("Reject unsafe rule plugin path: {}", cfg.so_path);
            continue;
        }
        const bool allowed =
            cfg.so_path.rfind("./rules/", 0) == 0 ||
            cfg.so_path.rfind("rules/", 0) == 0 ||
            cfg.so_path.rfind("/home/", 0) == 0;  // allow absolute under home for device deploy
        if (!allowed) {
            LOG_ERROR("Rule plugin path not in whitelist: {}", cfg.so_path);
            continue;
        }

        loaded.handle = dlopen(cfg.so_path.c_str(), RTLD_NOW);
        if (!loaded.handle) {
            LOG_ERROR("dlopen failed: {}, error: {}", cfg.so_path, dlerror());
            continue;
        }

        std::string create_name = "create_" + cfg.name;
        auto create_fn = (RuleCreateFunc)dlsym(loaded.handle, create_name.c_str());
        if (!create_fn) {
            LOG_ERROR("dlsym {} failed: {}", create_name, dlerror());
            dlclose(loaded.handle);
            loaded.handle = nullptr;
            continue;
        }

        loaded.instance = create_fn();
        if (!loaded.instance) {
            LOG_ERROR("Failed to create rule instance: {}", cfg.name);
            dlclose(loaded.handle);
            loaded.handle = nullptr;
            continue;
        }

        if (loaded.instance->Init(cfg.params) != 0) {
            LOG_ERROR("Rule Init failed: {}", cfg.name);
            loaded.instance->Destroy();
            delete loaded.instance;
            dlclose(loaded.handle);
            loaded.handle = nullptr;
            continue;
        }

        rules_.push_back(std::move(loaded));
    }

    loaded_ = !rules_.empty();
    LOG_INFO("RuleEngine loaded {} rules", rules_.size());
    return loaded_ ? 0 : -1;
}

bool RuleEngine::ProcessBoxes(
    const std::vector<DetectionObject>& objects,
    std::vector<bool>& results) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!loaded_ || rules_.empty()) {
        results.clear();
        return false;
    }

    results.assign(objects.size(), false);

    // Copy results from each rule into a local buffer first, then combine
    // under the lock. This keeps plugin execution time minimal while locked.
    std::vector<std::vector<bool>> all_results;
    all_results.reserve(rules_.size());

    for (const auto& rule : rules_) {
        if (!rule.instance) continue;

        std::vector<bool> rule_results(objects.size(), false);
        int ret = rule.instance->Process(objects, rule_results);
        if (ret != 0) {
            LOG_WARN("Rule '{}' Process returned error code {}", rule.config.name, ret);
            continue;
        }

        // Validate result size matches
        if (rule_results.size() != objects.size()) {
            LOG_ERROR("Rule '{}' returned {} results for {} objects, skipping",
                      rule.config.name, rule_results.size(), objects.size());
            continue;
        }

        all_results.push_back(std::move(rule_results));
    }

    // OR-combine all rule results
    for (size_t i = 0; i < objects.size(); ++i) {
        for (const auto& rr : all_results) {
            if (i < rr.size() && rr[i]) {
                results[i] = true;
                break;
            }
        }
    }

    return true;
}

void RuleEngine::Unload() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& rule : rules_) {
        if (rule.instance) {
            rule.instance->Destroy();
            delete rule.instance;
            rule.instance = nullptr;
        }
        if (rule.handle) {
            dlclose(rule.handle);
            rule.handle = nullptr;
        }
    }
    rules_.clear();
    loaded_ = false;
}
