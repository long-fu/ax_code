#include "pipeline_thread.h"

#include <string>

#include "logger.h"

PipelineThread::PipelineThread()
    : instance_id_(INVALID_INSTANCE_ID),
      instance_name_(""),
      is_base_configed_(false) {}

int PipelineThread::BaseConfig(int instance_id, const std::string& thread_name) {
  if (is_base_configed_) {
    return -1;
  }

  instance_id_ = instance_id;
  instance_name_.assign(thread_name.c_str());

  is_base_configed_ = true;

  return 0;
}