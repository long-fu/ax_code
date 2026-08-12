// common/BoxRule.hpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "base.h"

class BoxRule {
public:
    virtual ~BoxRule() = default;

    // Initialize with YAML config string
    virtual int Init(const std::string& config) = 0;

    // Process detection boxes, return judgment results
    // results: output for each object, index corresponds to objects
    virtual int Process(
        const std::vector<detection::Object>& objects,
        std::vector<bool>& results) = 0;

    // Destroy resources
    virtual int Destroy() = 0;
};

// Factory function signatures
using RuleCreateFunc = BoxRule* (*)();
using RuleDestroyFunc = void (*)(BoxRule*);

// Factory registry: auto-registers plugin types
class RuleFactory {
public:
    static RuleFactory& Instance();

    void RegisterRule(const std::string& name, RuleCreateFunc create);
    BoxRule* CreateRule(const std::string& name) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RuleCreateFunc> registry_;
};

#define REGISTER_RULE(Class) \
    extern "C" BoxRule* create_##Class() { return new Class(); } \
    struct RegisterHelper_##Class { \
        RegisterHelper_##Class() { \
            RuleFactory::Instance().RegisterRule(#Class, create_##Class); \
        } \
    }; \
    static RegisterHelper_##Class g_register_##Class;
