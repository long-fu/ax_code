#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "ax_engine_api.h"

#include "detection.h"



class Engine {
 public:
  explicit Engine(const std::string& model_path);
  virtual ~Engine();

  virtual int Init();
  virtual int Destroy();

  int Process(const std::vector<uint8_t>& data);
  int Process(const uint8_t* data, size_t size);

  virtual int Postprocess(int pic_width, int pic_height,
                          std::vector<detection::Object>& objects) = 0;

  AX_ENGINE_IO_T GetOutput() const { return io_data_; }
  AX_ENGINE_IO_INFO_T* GetInfo() const { return io_info_; }
  std::string ModelPath() const { return model_path_; }

 protected:
  std::string model_path_;

 private:
  AX_ENGINE_HANDLE handle_ = nullptr;
  AX_ENGINE_IO_INFO_T* io_info_ = nullptr;
  AX_ENGINE_IO_T io_data_{};
  bool is_released_ = false;
  bool engine_inited_ = false;
  bool handle_valid_ = false;
  uint32_t model_width_ = 640;
  uint32_t model_height_ = 640;
};