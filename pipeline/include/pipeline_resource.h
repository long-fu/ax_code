#pragma once

#include <cstdint>
#include <string>

#include "logger.h"

class PipelineResource {
 public:
  PipelineResource();
  explicit PipelineResource(int32_t channel);
  ~PipelineResource();

  int Init();
  void Release();

  int32_t GetChannelId() { return channel_id_; }

 private:
  bool is_released_ = false;
  int32_t channel_id_ = 0;
};