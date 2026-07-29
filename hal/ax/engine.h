#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "ax_engine_api.h"

#include "detection.h"

struct EngineConfig {
  std::string model_file = "/home/workspace/deepsort/weights/ALL_GEN_fire.axmodel";
  std::string model_type = "yolov5";
  std::vector<int> inputs = {1, 3, 640, 640};

  float prob_threshold = 0.20;
  float nms_threshold = 0.45;

  std::vector<int> num_anchors = {3, 3, 3};
  std::vector<std::vector<float>> anchors = {
      {10, 13}, {16, 30},   {33, 23},   {30, 61},   {62, 45},
      {59, 119}, {116, 90}, {156, 198}, {373, 326}};
  std::vector<int> strides = {8, 16, 32};

  std::vector<std::string> labels = {"fire", "smoke", "other", "warning"};

  std::string config_path;
  explicit EngineConfig(const std::string& path) : config_path(path) {}
  int Init() { return 0; }
};

class Engine {
 public:
  explicit Engine(const std::string& model_config);
  virtual ~Engine();

  virtual int Init();
  virtual int Destroy();

  int Process(const std::vector<uint8_t>& data);
  int Process(const uint8_t* data, size_t size);

  virtual int Postprocess(int pic_width, int pic_height,
                          std::vector<detection::Object>& objects) = 0;

  AX_ENGINE_IO_T GetOutput() const { return io_data_; }
  AX_ENGINE_IO_INFO_T* GetInfo() const { return io_info_; }
  EngineConfig GetConfig() const { return config_; }

 private:
  AX_ENGINE_HANDLE handle_;
  AX_ENGINE_IO_INFO_T* io_info_;
  AX_ENGINE_IO_T io_data_;
  bool is_released_ = false;
  uint32_t model_width_ = 640;
  uint32_t model_height_ = 640;
  EngineConfig config_;
};