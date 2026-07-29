#ifndef PIPELINE_TASK_NODE_MGR_H
#define PIPELINE_TASK_NODE_MGR_H
#pragma once

#include <memory>
#include <string>
#include "thread_safe_queue.h"
#include "task_node.h"

namespace pipeline {

struct TaskMessage {
    int dest;
    int msg_id;
    std::shared_ptr<void> data = nullptr;
};

enum TaskNodeStatus {
    kReady = 0,
    kRunning = 1,
    kExiting = 2,
    kExited = 3,
    kError = 4,
};

class TaskNodeMgr {
public:
    TaskNodeMgr(TaskNode* user_instance, const std::string& node_name);
    ~TaskNodeMgr();

    static void ThreadEntry(void* data);

    TaskNode* UserInstance() {
        return user_instance_;
    }

    const std::string& NodeName() const {
        return name_;
    }

    TaskError PushMessage(std::shared_ptr<TaskMessage>& message);

    std::shared_ptr<TaskMessage> PopMessage() {
        return msg_queue_.Pop();
    }

    void CreateThread();

    void SetStatus(TaskNodeStatus status) {
        status_ = status;
    }

    TaskNodeStatus Status() const {
        return status_;
    }

    TaskError WaitThreadInitEnd();

private:
    bool is_exit_;
    TaskNodeStatus status_;
    TaskNode* user_instance_;
    std::string name_;
    ThreadSafeQueue<std::shared_ptr<TaskMessage>> msg_queue_;
};

} // namespace pipeline
#endif  // PIPELINE_TASK_NODE_MGR_H
