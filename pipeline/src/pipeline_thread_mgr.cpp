#include "pipeline_thread_mgr.h"

#include <memory>
#include <string>

#include "logger.h"

namespace {
const uint32_t kWait10Milliseconds = 10000;
const uint32_t kWaitThreadStart = 1000;
}  // namespace

PipelineThreadMgr::PipelineThreadMgr(PipelineThread* user_thread_instance,
                                     const std::string& thread_name,
                                     uint32_t msg_queue_size)
    : user_instance_(user_thread_instance),
      name_(thread_name),
      msg_queue_(msg_queue_size) {}

PipelineThreadMgr::~PipelineThreadMgr() {
  user_instance_ = nullptr;
  while (!msg_queue_.Empty()) {
    msg_queue_.Pop();
  }
}

void PipelineThreadMgr::CreateThread() {
  std::thread engine(&PipelineThreadMgr::ThreadEntry, this);
  engine.detach();
}

void PipelineThreadMgr::ThreadEntry(void* arg) {
  auto* th_mgr = static_cast<PipelineThreadMgr*>(arg);
  PipelineThread* user_instance = th_mgr->GetUserInstance();
  if (user_instance == nullptr) {
    LOG_ERROR("Pipeline thread exit for user thread instance is null");
    return;
  }

  std::string& inst_name = user_instance->SelfInstanceName();

  int ret = user_instance->Init();
  if (ret) {
    LOG_ERROR("Thread {} init error {}, thread exit", inst_name, ret);
    th_mgr->SetStatus(THREAD_ERROR);
    return;
  }

  th_mgr->SetStatus(THREAD_RUNNING);
  while (THREAD_RUNNING == th_mgr->GetStatus()) {
    auto msg = th_mgr->PopMsgFromQueue();
    if (msg == nullptr) {
      usleep(kWait10Milliseconds);
      continue;
    }
    ret = user_instance->Process(msg->msg_id, msg->data);
    msg->data = nullptr;
    if (ret) {
      LOG_ERROR("Thread {} process function return error {}, thread exit",
                inst_name, ret);
      th_mgr->SetStatus(THREAD_ERROR);
      return;
    }
    usleep(1);
  }

  th_mgr->SetStatus(THREAD_EXITED);
}

int PipelineThreadMgr::WaitThreadInitEnd() {
  while (true) {
    if (status_ == THREAD_RUNNING) {
      break;
    } else if (status_ > THREAD_RUNNING) {
      std::string& inst_name = user_instance_->SelfInstanceName();
      LOG_ERROR("Thread instance {} status change to {}, app start failed",
                inst_name, static_cast<int>(status_));
      return -1;
    } else {
      usleep(kWaitThreadStart);
    }
  }
  return 0;
}

int PipelineThreadMgr::PushMsgToQueue(
    std::shared_ptr<PipelineMessage>& message) {
  if (status_ != THREAD_RUNNING) {
    LOG_ERROR("Thread instance {} status({}) is invalid, can not receive message",
              name_, static_cast<int>(status_));
    return -1;
  }
  bool pushed = msg_queue_.Push(message);
  if (!pushed) {
    LOG_WARN("Thread instance {} message queue full, dropping message", name_);
    return -1;
  }
  return 0;
}