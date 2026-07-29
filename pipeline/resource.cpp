#include "resource.h"

namespace pipeline {

Resource::Resource()
    : is_released_(false), device_id_(0), config_path_("") {
}

Resource::Resource(int32_t device_id, const std::string& config_path)
    : is_released_(false),
      device_id_(device_id),
      config_path_(config_path) {
}

Resource::~Resource() {
    Release();
}

TaskError Resource::Init() {
    // resource init
    return kOk;
}

void Resource::Release() {
    if (is_released_) {
        return;
    }
    is_released_ = true;
}

} // namespace pipeline
