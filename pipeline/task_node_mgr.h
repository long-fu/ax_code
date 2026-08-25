#ifndef PIPELINE_TASK_NODE_MGR_H
#define PIPELINE_TASK_NODE_MGR_H
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
    void Join();

    void SetStatus(TaskNodeStatus status) {
        status_ = status;
    }

    TaskNodeStatus Status() const {
        return status_;
    }

    TaskError WaitThreadInitEnd();

    uint64_t DroppedTotal() const {
        return dropped_total_.load(std::memory_order_relaxed);
    }

private:
    // 队列满导致消息被丢弃时记账并按时间窗汇总打印。
    // 满载时丢弃可达每秒数十次，逐条打印会把日志刷爆。
    void ReportDroppedMessage(int msg_id);

    bool is_exit_;
    std::atomic<TaskNodeStatus> status_;
    TaskNode* user_instance_;
    std::string name_;
    ThreadSafeQueue<std::shared_ptr<TaskMessage>> msg_queue_;
    std::thread worker_;

    // 丢弃统计。PushMessage 可被多线程调用（VDEC 回调线程 + 各 worker），
    // 故计数用原子；窗口状态只在丢弃分支里加锁，正常路径无额外开销。
    std::atomic<uint64_t> dropped_total_{0};
    std::mutex drop_log_mutex_;
    uint64_t dropped_in_window_ = 0;
    bool first_drop_reported_ = false;
    std::chrono::steady_clock::time_point last_drop_log_;
};

} // namespace pipeline
#endif  // PIPELINE_TASK_NODE_MGR_H
