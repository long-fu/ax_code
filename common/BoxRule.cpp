// common/BoxRule.cpp
#include "BoxRule.hpp"

RuleFactory& RuleFactory::instance() {
    static RuleFactory inst;
    return inst;
}

void RuleFactory::registerRule(const std::string& name, RuleCreateFunc create) {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_[name] = create;
}

BoxRule* RuleFactory::createRule(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = registry_.find(name);
    if (it != registry_.end()) {
        return it->second();
    }
    return nullptr;
}
