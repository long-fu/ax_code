#ifndef PIPELINE_TASK_NODE_H
#define PIPELINE_TASK_NODE_H
#pragma once

#include <memory>
#include <string>
#include "error.h"

namespace pipeline {

constexpr int kInvalidInstanceId = -1;

class TaskNode {
public:
    TaskNode();
    virtual ~TaskNode() {}

    virtual int Init() {
        return kOk;
    }

    virtual int Process(int msg_id, std::shared_ptr<void> msg_data) = 0;

    int InstanceId() const {
        return instance_id_;
    }

    std::string& InstanceName() {
        return instance_name_;
    }

    TaskError BaseConfig(int instance_id, const std::string& node_name);

private:
    int instance_id_;
    std::string instance_name_;
    bool base_configured_;
};

struct TaskNodeParam {
    TaskNode* node = nullptr;
    std::string node_name;
    int node_id = kInvalidInstanceId;
};

} // namespace pipeline
#endif  // PIPELINE_TASK_NODE_H
