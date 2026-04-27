#pragma once

#include <memory>
#include <string>

#include "Logger.h"
#include "ThreadSafeQueue.h"

#define INVALID_INSTANCE_ID (-1)

class PipelineThread {
 public:
  PipelineThread();
  virtual ~PipelineThread() = default;

  virtual int Init() { return 0; }
  virtual int Process(int msg_id, std::shared_ptr<void> msg_data) = 0;

  int SelfInstanceId() { return m_instance_id_; }
  std::string& SelfInstanceName() { return m_instance_name_; }

  int BaseConfig(int instance_id, const std::string& thread_name);

 private:
  int m_instance_id_;
  std::string m_instance_name_;
  bool m_is_base_configed_ = false;
  bool m_is_exit_ = false;
};

struct PipelineThreadParam {
  PipelineThread* thread_inst = nullptr;
  std::string thread_inst_name = "";
  int thread_inst_id = INVALID_INSTANCE_ID;
  uint32_t queue_size = 256;
};