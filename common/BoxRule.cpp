// common/BoxRule.cpp
#include "BoxRule.hpp"

RuleFactory& RuleFactory::Instance() {
    static RuleFactory inst;
    return inst;
}

void RuleFactory::RegisterRule(const std::string& name, RuleCreateFunc create) {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_[name] = create;
}

BoxRule* RuleFactory::CreateRule(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = registry_.find(name);
    if (it != registry_.end()) {
        return it->second();
    }
    return nullptr;
}
