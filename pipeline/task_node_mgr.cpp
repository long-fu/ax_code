#include "task_node_mgr.h"
#include "error.h"
#include <chrono>
#include <thread>

namespace pipeline {
namespace {
const uint32_t kMsgQueueSize = 256;
const auto kPollInterval = std::chrono::microseconds(10000);
const auto kInitPollInterval = std::chrono::microseconds(1000);
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
    return msg_queue_.Push(message) ? kOk : kEnqueue;
}

} // namespace pipeline
