#include "PipelineThread.h"

#include <string>

#include "Logger.h"

PipelineThread::PipelineThread()
    : m_instance_id_(INVALID_INSTANCE_ID),
      m_instance_name_(""),
      m_is_base_configed_(false) {}

int PipelineThread::BaseConfig(int instance_id, const std::string& thread_name) {
  if (m_is_base_configed_) {
    return -1;
  }

  m_instance_id_ = instance_id;
  m_instance_name_.assign(thread_name.c_str());

  m_is_base_configed_ = true;

  return 0;
}