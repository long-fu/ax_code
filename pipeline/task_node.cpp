#include "task_node.h"

namespace pipeline {

TaskNode::TaskNode()
    : instance_id_(kInvalidInstanceId),
      instance_name_(""),
      base_configured_(false) {
}

TaskError TaskNode::BaseConfig(int instance_id, const std::string& node_name) {
    if (base_configured_) {
        return kInitedAlready;
    }
    instance_id_ = instance_id;
    instance_name_.assign(node_name.c_str());
    base_configured_ = true;
    return kOk;
}

} // namespace pipeline
