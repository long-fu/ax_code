#include "Pipeline.h"

#include <memory>
#include <string>
#include <vector>

#include "Logger.h"
#include "PipelineThreadMgr.h"

namespace {
const uint32_t kWaitInterval = 10000;
const uint32_t kThreadExitRetry = 3;
}  // namespace

Pipeline::Pipeline() : is_released_(false), is_wait_end_(false) { Init(); }

Pipeline::~Pipeline() {
  LOG_INFO("调用释放 ~~Pipeline");
  ReleaseThreads();
}

int Pipeline::Init() {
  const uint32_t msg_queue_size = 256;
  auto* th_mgr = new PipelineThreadMgr(nullptr, "main", msg_queue_size);
  thread_list_.push_back(th_mgr);
  th_mgr->SetStatus(THREAD_RUNNING);
  return 0;
}

int Pipeline::CreatePipelineThread(PipelineThread* th_inst,
                                   const std::string& inst_name,
                                   uint32_t msg_queue_size) {
  int inst_id = CreatePipelineThreadMgr(th_inst, inst_name, msg_queue_size);
  if (inst_id == INVALID_INSTANCE_ID) {
    LOG_ERROR_LOC("Add thread instance {} failed", inst_name);
    return INVALID_INSTANCE_ID;
  }

  thread_list_[inst_id]->CreateThread();
  int ret = thread_list_[inst_id]->WaitThreadInitEnd();
  if (ret != 0) {
    LOG_ERROR_LOC("Create thread failed, error {}", ret);
    return INVALID_INSTANCE_ID;
  }

  return inst_id;
}

int Pipeline::CreatePipelineThreadMgr(PipelineThread* th_inst,
                                      const std::string& inst_name,
                                      uint32_t msg_queue_size) {
  if (!CheckThreadNameUnique(inst_name)) {
    LOG_ERROR_LOC("The thread instance name is not unique");
    return INVALID_INSTANCE_ID;
  }

  int inst_id = thread_list_.size();
  int ret = th_inst->BaseConfig(inst_id, inst_name);
  if (ret != 0) {
    LOG_ERROR_LOC("Create thread instance failed for error {}", ret);
    return INVALID_INSTANCE_ID;
  }

  auto* th_mgr = new PipelineThreadMgr(th_inst, inst_name, msg_queue_size);
  thread_list_.push_back(th_mgr);

  return inst_id;
}

bool Pipeline::CheckThreadNameUnique(const std::string& thread_name) {
  if (thread_name.size() == 0) {
    return true;
  }

  for (size_t i = 0; i < thread_list_.size(); i++) {
    if (thread_name == thread_list_[i]->GetThreadName()) {
      return false;
    }
  }

  return true;
}

int Pipeline::Start(std::vector<PipelineThreadParam>& thread_param_tbl) {
  for (size_t i = 0; i < thread_param_tbl.size(); i++) {
    int inst_id = CreatePipelineThreadMgr(thread_param_tbl[i].thread_inst,
                                          thread_param_tbl[i].thread_inst_name,
                                          thread_param_tbl[i].queue_size);
    if (inst_id == INVALID_INSTANCE_ID) {
      LOG_ERROR_LOC("Create thread instance failed");
      return -1;
    }
    thread_param_tbl[i].thread_inst_id = inst_id;
  }

  for (size_t i = 0; i < thread_param_tbl.size(); i++) {
    thread_list_[thread_param_tbl[i].thread_inst_id]->CreateThread();
  }

  for (size_t i = 0; i < thread_param_tbl.size(); i++) {
    int inst_id = thread_param_tbl[i].thread_inst_id;
    int ret = thread_list_[inst_id]->WaitThreadInitEnd();
    if (ret != 0) {
      LOG_ERROR_LOC("Create thread {} failed, error {}",
                thread_param_tbl[i].thread_inst_name, ret);
      return ret;
    }
  }
  return 0;
}

int Pipeline::GetPipelineThreadIdByName(const std::string& thread_name) {
  if (thread_name.empty()) {
    LOG_ERROR_LOC("search name is empty");
    return INVALID_INSTANCE_ID;
  }

  for (uint32_t i = 0; i < thread_list_.size(); i++) {
    if (thread_list_[i]->GetThreadName() == thread_name) {
      return i;
    }
  }

  return INVALID_INSTANCE_ID;
}

int Pipeline::SendMessage(int dest, int msg_id,
                          std::shared_ptr<void> data) {
  if (static_cast<uint32_t>(dest) > thread_list_.size()) {
    LOG_ERROR_LOC("Send message to {} failed for thread not exist", dest);
    return -1;
  }

  auto message = std::make_shared<PipelineMessage>();
  message->dest = dest;
  message->msg_id = msg_id;
  message->data = data;

  return thread_list_[dest]->PushMsgToQueue(message);
}

void Pipeline::Wait() {
  while (true) {
    usleep(kWaitInterval);
    if (is_wait_end_) break;
  }
  thread_list_[g_main_thread_id]->SetStatus(THREAD_EXITED);
}

bool Pipeline::CheckThreadAbnormal() {
  for (size_t i = 0; i < thread_list_.size(); i++) {
    if (thread_list_[i]->GetStatus() == THREAD_ERROR) {
      return true;
    }
  }
  return false;
}

void Pipeline::Wait(AclLiteMsgProcess msg_process, void* param) {
  PipelineThreadMgr* main_mgr = thread_list_[0];

  if (main_mgr == nullptr) {
    LOG_ERROR_LOC(
        "AclLite app wait exit for message process function is nullptr");
    return;
  }

  while (true) {
    if (is_wait_end_) break;

    auto msg = main_mgr->PopMsgFromQueue();
    if (msg == nullptr) {
      usleep(kWaitInterval);
      continue;
    }
    int ret = msg_process(msg->msg_id, msg->data, param);
    if (ret) {
      LOG_ERROR_LOC("AclLite app exit for message {} process error:{}",
                msg->msg_id, ret);
      break;
    }
  }
  thread_list_[g_main_thread_id]->SetStatus(THREAD_EXITED);
}

void Pipeline::Exit() { ReleaseThreads(); }

void Pipeline::ReleaseThreads() {
  if (is_released_) return;
  thread_list_[g_main_thread_id]->SetStatus(THREAD_EXITED);

  for (uint32_t i = 1; i < thread_list_.size(); i++) {
    if ((thread_list_[i] != nullptr) &&
        (thread_list_[i]->GetStatus() == THREAD_RUNNING)) {
      thread_list_[i]->SetStatus(THREAD_EXITING);
    }
  }

  int retry = kThreadExitRetry;
  while (retry >= 0) {
    bool exit_finish = true;
    for (uint32_t i = 0; i < thread_list_.size(); i++) {
      if (thread_list_[i] == nullptr) continue;
      if (thread_list_[i]->GetStatus() > THREAD_EXITING) {
        delete thread_list_[i];
        thread_list_[i] = nullptr;
        LOG_INFO("AclLite thread {} released", i);
      } else {
        thread_list_[i]->SetStatus(THREAD_EXITING);
        exit_finish = false;
      }
    }

    if (exit_finish) break;

    sleep(1);
    retry--;
  }
  is_released_ = true;
}

Pipeline& CreatePipelineInstance() { return Pipeline::GetInstance(); }

Pipeline& GetPipelineInstance() { return Pipeline::GetInstance(); }

int SendMessage(int dest, int msg_id, std::shared_ptr<void> data) {
  Pipeline& app = Pipeline::GetInstance();
  return app.SendMessage(dest, msg_id, data);
}

int GetPipelineThreadIdByName(const std::string& thread_name) {
  Pipeline& app = Pipeline::GetInstance();
  return app.GetPipelineThreadIdByName(thread_name);
}