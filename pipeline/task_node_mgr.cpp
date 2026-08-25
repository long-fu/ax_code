#include "task_node_mgr.h"
#include "error.h"
#include <chrono>
#include <thread>

namespace pipeline {
namespace {
const uint32_t kMsgQueueSize = 256;
const auto kPollInterval = std::chrono::microseconds(10000);
const auto kInitPollInterval = std::chrono::microseconds(1000);
// 丢帧汇总日志的最小间隔
const auto kDropLogInterval = std::chrono::seconds(5);
}

TaskNodeMgr::TaskNodeMgr(TaskNode* user_instance, const std::string& node_name)
    : is_exit_(false),
      status_(kReady),
      user_instance_(user_instance),
      name_(node_name),
      msg_queue_(kMsgQueueSize) {
}

TaskNodeMgr::~TaskNodeMgr() {
    Join();
    // 退出时汇报总丢弃量，免得只能靠翻实时日志估算。
    const uint64_t dropped = dropped_total_.load(std::memory_order_relaxed);
    if (dropped > 0) {
        PIPELINE_LOG_WARNING("Node {} 生命周期内共丢弃 {} 条消息(队列满)",
                             name_, dropped);
    }
    user_instance_ = nullptr;
    while (!msg_queue_.Empty()) {
        msg_queue_.Pop();
    }
}

void TaskNodeMgr::CreateThread() {
    if (worker_.joinable()) {
        PIPELINE_LOG_ERROR("TaskNodeMgr {} already has a running thread", name_);
        return;
    }
    worker_ = std::thread(&TaskNodeMgr::ThreadEntry, static_cast<void*>(this));
}

void TaskNodeMgr::Join() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void TaskNodeMgr::ThreadEntry(void* arg) {
    auto* mgr = static_cast<TaskNodeMgr*>(arg);
    TaskNode* user_instance = mgr->UserInstance();
    if (user_instance == nullptr) {
        PIPELINE_LOG_ERROR("TaskNode thread exit for user thread instance is null");
        return;
    }

    std::string& inst_name = user_instance->InstanceName();

    int ret = user_instance->Init();
    if (ret) {
        PIPELINE_LOG_ERROR("Thread {} init error {}, thread exit",
                           inst_name, ret);
        mgr->SetStatus(kError);
        return;
    }

    mgr->SetStatus(kRunning);
    while (kRunning == mgr->Status()) {
        std::shared_ptr<TaskMessage> msg = mgr->PopMessage();
        if (msg == nullptr) {
            std::this_thread::sleep_for(kPollInterval);
            continue;
        }
        ret = user_instance->Process(msg->msg_id, msg->data);
        msg->data = nullptr;
        if (ret) {
            // Hot-path / backpressure errors must not kill the node.
            PIPELINE_LOG_ERROR("Thread {} process function return error {}, "
                               "drop and continue", inst_name, ret);
            continue;
        }
    }
    mgr->SetStatus(kExited);
}

TaskError TaskNodeMgr::WaitThreadInitEnd() {
    while (true) {
        if (status_ == kRunning) {
            break;
        } else if (status_ > kRunning) {
            std::string& inst_name = user_instance_->InstanceName();
            PIPELINE_LOG_ERROR("Thread instance {} status change to {}, "
                               "app start failed", inst_name,
                               static_cast<int>(status_.load()));
            return kStartThread;
        } else {
            std::this_thread::sleep_for(kInitPollInterval);
        }
    }
    return kOk;
}

TaskError TaskNodeMgr::PushMessage(std::shared_ptr<TaskMessage>& message) {
    if (status_ != kRunning) {
        PIPELINE_LOG_ERROR("Thread instance {} status({}) is invalid, "
                           "can not receive message", name_,
                           static_cast<int>(status_.load()));
        return kThreadAbnormal;
    }
    if (msg_queue_.Push(message)) {
        return kOk;
    }
    // 各发送点历来忽略本函数返回值，丢弃在此统一记账：
    // 这是全部 SendMessage 的唯一收敞口，只在这里记就能全覆盖。
    ReportDroppedMessage(message->msg_id);
    return kEnqueue;
}

void TaskNodeMgr::ReportDroppedMessage(int msg_id) {
    // 队列满意味着本节点处理速度跟不上上游。丢帧对上层表现为跟踪 ID 断裂或
    // 漏检，若不记日志就无法把"算法效果差"和"正在丢帧"区分开。
    const uint64_t total =
        dropped_total_.fetch_add(1, std::memory_order_relaxed) + 1;

    std::lock_guard<std::mutex> lock(drop_log_mutex_);
    ++dropped_in_window_;

    const auto now = std::chrono::steady_clock::now();
    if (!first_drop_reported_) {
        // 首次丢弃立即打印，便于定位开始掉帧的时刻
        PIPELINE_LOG_WARNING("Node {} 队列满，开始丢弃消息 msg_id={} 容量={}",
                             name_, msg_id, kMsgQueueSize);
        first_drop_reported_ = true;
        last_drop_log_ = now;
        dropped_in_window_ = 0;
        return;
    }
    if (now - last_drop_log_ >= kDropLogInterval) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - last_drop_log_);
        PIPELINE_LOG_WARNING("Node {} 队列满丢弃 {} 条/{}秒 (累计 {})",
                             name_, dropped_in_window_, elapsed.count(), total);
        last_drop_log_ = now;
        dropped_in_window_ = 0;
    }
}

} // namespace pipeline
