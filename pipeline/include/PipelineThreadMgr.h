#pragma once

#include <memory>
#include <string>
#include <thread>

#include "Logger.h"
#include "PipelineThread.h"
#include "ThreadSafeQueue.h"

enum PipelineThreadStatus {
  THREAD_READY = 0,
  THREAD_RUNNING = 1,
  THREAD_EXITING = 2,
  THREAD_EXITED = 3,
  THREAD_ERROR = 4,
};

struct PipelineMessage {
  int dest;
  int msg_id;
  std::shared_ptr<void> data = nullptr;
};

class PipelineThreadMgr {
 public:
  PipelineThreadMgr(PipelineThread* user_thread_instance,
                    const std::string& thread_name,
                    uint32_t msg_queue_size);
  ~PipelineThreadMgr();

  static void ThreadEntry(void* data);

  PipelineThread* GetUserInstance() { return user_instance_; }
  const std::string& GetThreadName() { return name_; }

  int PushMsgToQueue(std::shared_ptr<PipelineMessage>& message);
  std::shared_ptr<PipelineMessage> PopMsgFromQueue() {
    return msg_queue_.Pop();
  }

  void CreateThread();
  void SetStatus(PipelineThreadStatus status) { status_ = status; }
  PipelineThreadStatus GetStatus() { return status_; }
  int WaitThreadInitEnd();

 public:
  bool is_exit_ = false;
  PipelineThreadStatus status_ = THREAD_READY;
  PipelineThread* user_instance_ = nullptr;
  std::string name_;
  ThreadSafeQueue<std::shared_ptr<PipelineMessage>> msg_queue_;
};