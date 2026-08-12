#pragma once

#include <iostream>
#include <string>

#include "engine.h"

struct Yolov5Config : public EngineConfig {
  std::string config_path;
  std::string model_file =
      "/home/workspace/deepsort/weights/ALL_GEN_fire.axmodel";
  std::string model_type = "yolov5";
  std::vector<int> inputs = {1, 3, 640, 640};

  float prob_threshold = 0.20;
  float nms_threshold = 0.45;

  std::vector<int> num_anchors = {3, 3, 3};
  std::vector<std::vector<float>> anchors = {{10, 13},  {16, 30},   {33, 23},
                                             {30, 61},  {62, 45},   {59, 119},
                                             {116, 90}, {156, 198}, {373, 326}};
  std::vector<int> strides = {8, 16, 32};

  std::vector<std::string> labels = {"fire", "smoke", "other", "warning"};
  std::string ModelFile() const override { return model_file; }
};

class Yolov5 : public Engine {
public:
  explicit Yolov5(const Yolov5Config &model_config)
      : Engine(model_config), config_(model_config) {}
  ~Yolov5() override;

  int Postprocess(int pic_width, int pic_height,
                  std::vector<detection::Object> &objects) override;

  Yolov5(const Yolov5 &) = delete;
  Yolov5 &operator=(const Yolov5 &) = delete;

private:
  Yolov5Config config_;
};