#ifndef PIPELINE_TASK_SCHEDULER_H
#define PIPELINE_TASK_SCHEDULER_H
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "task_node_mgr.h"

namespace pipeline {

typedef int (*TaskMsgProcess)(uint32_t msg_id,
                               std::shared_ptr<void> msg_data,
                               void* user_data);

class TaskScheduler {
public:
    static TaskScheduler& Instance() {
        static TaskScheduler instance;
        return instance;
    }

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    int CreateTaskNode(TaskNode* node, const std::string& node_name);
    int Start(std::vector<TaskNodeParam>& node_params);
    void Wait();
    void Wait(TaskMsgProcess msg_process, void* param);
    int TaskNodeIdByName(const std::string& node_name);
    TaskError SendMessage(int dest, int msg_id, std::shared_ptr<void> data);

    void SignalWaitEnd() {
        wait_end_ = true;
    }

    void Exit();

private:
    TaskScheduler();
    ~TaskScheduler();

    TaskError Init();
    int CreateTaskNodeMgr(TaskNode* node, const std::string& node_name);
    bool IsThreadAbnormal();
    bool IsNodeNameUnique(const std::string& node_name);
    void ReleaseThreads();

    static constexpr int kMainThreadId = 0;

    bool is_released_;
    bool wait_end_;
    std::vector<TaskNodeMgr*> thread_list_;
};

TaskScheduler& CreateTaskSchedulerInstance();
TaskScheduler& GetTaskSchedulerInstance();
TaskError SendMessage(int dest, int msg_id, std::shared_ptr<void> data);
int TaskNodeIdByName(const std::string& node_name);

} // namespace pipeline
#endif  // PIPELINE_TASK_SCHEDULER_H
