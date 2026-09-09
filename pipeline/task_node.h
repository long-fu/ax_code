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

    // 停止本节点自行创建的外部线程（解码回调线程等）。
    //
    // 由 ExitPipeline 在 TaskScheduler::Exit() **之前**同步调用：Exit() 会销毁
    // TaskNodeMgr 并把 thread_list_ 元素置空（size 不变），此后外部线程再调
    // SendMessage 会通过边界检查后解引用空指针。
    //
    // 实现要求：同步返回（返回后相关线程必须已 join），且幂等（析构会再调一次）。
    //
    // 只有管线**头部**（外部数据来源）需要覆写。尾部节点（如 EncProcess）不应在
    // 此停止编码器，否则 Exit() 期间上游仍在排空队列，会往已停止的编码器投数据。
    virtual void StopSources() {}

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
