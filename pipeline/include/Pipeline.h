#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Logger.h"
#include "PipelineThreadMgr.h"

namespace {
int g_main_thread_id = 0;
}  // namespace

using AclLiteMsgProcess = int (*)(uint32_t msg_id,
                                  std::shared_ptr<void> msg_data,
                                  void* user_data);

class Pipeline {
 public:
  Pipeline();
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;
  ~Pipeline();

  static Pipeline& GetInstance() {
    static Pipeline instance;
    return instance;
  }

  int Start(std::vector<PipelineThreadParam>& thread_param_tbl);
  void Wait();
  void Wait(AclLiteMsgProcess msg_process, void* param);
  int GetPipelineThreadIdByName(const std::string& thread_name);
  int SendMessage(int dest, int msg_id, std::shared_ptr<void> data);
  void WaitEnd() { is_wait_end_ = true; }
  void Exit();

 private:
  int Init();
  // int CreatePipelineThread(PipelineThread* th_inst,
  //                          const std::string& inst_name,
  //                          uint32_t msg_queue_size);
  int CreatePipelineThreadMgr(PipelineThread* th_inst,
                              const std::string& inst_name,
                              uint32_t msg_queue_size);
  bool CheckThreadAbnormal();
  bool CheckThreadNameUnique(const std::string& thread_name);
  void ReleaseThreads();

  bool is_released_ = false;
  bool is_wait_end_ = false;
  std::vector<PipelineThreadMgr*> thread_list_;
};

Pipeline& CreatePipelineInstance();
Pipeline& GetPipelineInstance();
int SendMessage(int dest, int msg_id, std::shared_ptr<void> data);
int GetPipelineThreadIdByName(const std::string& thread_name);