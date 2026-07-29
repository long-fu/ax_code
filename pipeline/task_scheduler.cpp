#include "task_scheduler.h"
#include "error.h"
#include <chrono>
#include <thread>

namespace pipeline {
namespace {
const auto kWaitInterval = std::chrono::microseconds(10000);
const uint32_t kThreadExitRetry = 3;
}

TaskScheduler::TaskScheduler()
    : is_released_(false), wait_end_(false) {
    Init();
}

TaskScheduler::~TaskScheduler() {
    ReleaseThreads();
}

TaskError TaskScheduler::Init() {
    auto* mgr = new TaskNodeMgr(nullptr, "main");
    thread_list_.push_back(mgr);
    mgr->SetStatus(kRunning);
    return kOk;
}

int TaskScheduler::CreateTaskNode(TaskNode* node, const std::string& node_name) {
    int inst_id = CreateTaskNodeMgr(node, node_name);
    if (inst_id == kInvalidInstanceId) {
        PIPELINE_LOG_ERROR("Add node instance %s failed", node_name.c_str());
        return kInvalidInstanceId;
    }
    thread_list_[inst_id]->CreateThread();
    TaskError ret = thread_list_[inst_id]->WaitThreadInitEnd();
    if (ret != kOk) {
        PIPELINE_LOG_ERROR("Create node failed, error %d", ret);
        return kInvalidInstanceId;
    }
    return inst_id;
}

int TaskScheduler::CreateTaskNodeMgr(TaskNode* node,
                                     const std::string& node_name) {
    if (!IsNodeNameUnique(node_name)) {
        PIPELINE_LOG_ERROR("The node instance name is not unique");
        return kInvalidInstanceId;
    }
    int inst_id = static_cast<int>(thread_list_.size());
    TaskError ret = node->BaseConfig(inst_id, node_name);
    if (ret != kOk) {
        PIPELINE_LOG_ERROR("Create node instance failed for error %d", ret);
        return kInvalidInstanceId;
    }
    auto* mgr = new TaskNodeMgr(node, node_name);
    thread_list_.push_back(mgr);
    return inst_id;
}

bool TaskScheduler::IsNodeNameUnique(const std::string& node_name) {
    if (node_name.empty()) {
        return true;
    }
    for (size_t i = 0; i < thread_list_.size(); i++) {
        if (node_name == thread_list_[i]->NodeName()) {
            return false;
        }
    }
    return true;
}

int TaskScheduler::Start(std::vector<TaskNodeParam>& node_params) {
    for (size_t i = 0; i < node_params.size(); i++) {
        int inst_id = CreateTaskNodeMgr(node_params[i].node,
                                        node_params[i].node_name);
        if (inst_id == kInvalidInstanceId) {
            PIPELINE_LOG_ERROR("Create node instance failed");
            return kGeneralError;
        }
        node_params[i].node_id = inst_id;
    }
    // Note: instance id must be generated before creating threads,
    // so nodes can look up each other's instance ids in Init function
    for (size_t i = 0; i < node_params.size(); i++) {
        thread_list_[node_params[i].node_id]->CreateThread();
    }
    for (size_t i = 0; i < node_params.size(); i++) {
        int inst_id = node_params[i].node_id;
        TaskError ret = thread_list_[inst_id]->WaitThreadInitEnd();
        if (ret != kOk) {
            PIPELINE_LOG_ERROR("Create node %s failed, error %d",
                               node_params[i].node_name.c_str(), ret);
            return ret;
        }
    }
    return kOk;
}

int TaskScheduler::TaskNodeIdByName(const std::string& node_name) {
    if (node_name.empty()) {
        PIPELINE_LOG_ERROR("search name is empty");
        return kInvalidInstanceId;
    }
    for (size_t i = 0; i < thread_list_.size(); i++) {
        if (thread_list_[i]->NodeName() == node_name) {
            return static_cast<int>(i);
        }
    }
    return kInvalidInstanceId;
}

TaskError TaskScheduler::SendMessage(int dest, int msg_id,
                                     std::shared_ptr<void> data) {
    if (static_cast<uint32_t>(dest) >= thread_list_.size()) {
        PIPELINE_LOG_ERROR("Send message to %d failed for node not exist", dest);
        return kDestInvalid;
    }
    auto message = std::make_shared<TaskMessage>();
    message->dest = dest;
    message->msg_id = msg_id;
    message->data = data;
    return thread_list_[dest]->PushMessage(message);
}

void TaskScheduler::Wait() {
    while (true) {
        std::this_thread::sleep_for(kWaitInterval);
        if (wait_end_) break;
    }
    thread_list_[kMainThreadId]->SetStatus(kExited);
}

bool TaskScheduler::IsThreadAbnormal() {
    for (size_t i = 0; i < thread_list_.size(); i++) {
        if (thread_list_[i]->Status() == kError) {
            return true;
        }
    }
    return false;
}

void TaskScheduler::Wait(TaskMsgProcess msg_process, void* param) {
    TaskNodeMgr* main_mgr = thread_list_[0];
    if (main_mgr == nullptr) {
        PIPELINE_LOG_ERROR(
            "TaskScheduler wait exit for message process function is nullptr");
        return;
    }
    while (true) {
        if (wait_end_) break;
        std::shared_ptr<TaskMessage> msg = main_mgr->PopMessage();
        if (msg == nullptr) {
            std::this_thread::sleep_for(kWaitInterval);
            continue;
        }
        int ret = msg_process(msg->msg_id, msg->data, param);
        if (ret) {
            PIPELINE_LOG_ERROR(
                "TaskScheduler exit for message %d process error:%d",
                msg->msg_id, ret);
            break;
        }
    }
    thread_list_[kMainThreadId]->SetStatus(kExited);
}

void TaskScheduler::Exit() {
    ReleaseThreads();
}

void TaskScheduler::ReleaseThreads() {
    if (is_released_) return;
    thread_list_[kMainThreadId]->SetStatus(kExited);

    for (size_t i = 1; i < thread_list_.size(); i++) {
        if ((thread_list_[i] != nullptr) &&
            (thread_list_[i]->Status() == kRunning)) {
            thread_list_[i]->SetStatus(kExiting);
        }
    }

    int retry = kThreadExitRetry;
    while (retry >= 0) {
        bool exit_finish = true;
        for (size_t i = 0; i < thread_list_.size(); i++) {
            if (thread_list_[i] == nullptr) continue;
            if (thread_list_[i]->Status() > kExiting) {
                delete thread_list_[i];
                thread_list_[i] = nullptr;
                PIPELINE_LOG_INFO("TaskNode thread %zu released", i);
            } else {
                exit_finish = false;
            }
        }
        if (exit_finish) break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        retry--;
    }
    is_released_ = true;
}

TaskScheduler& CreateTaskSchedulerInstance() {
    return TaskScheduler::Instance();
}

TaskScheduler& GetTaskSchedulerInstance() {
    return TaskScheduler::Instance();
}

TaskError SendMessage(int dest, int msg_id, std::shared_ptr<void> data) {
    return TaskScheduler::Instance().SendMessage(dest, msg_id, data);
}

int TaskNodeIdByName(const std::string& node_name) {
    return TaskScheduler::Instance().TaskNodeIdByName(node_name);
}

} // namespace pipeline
