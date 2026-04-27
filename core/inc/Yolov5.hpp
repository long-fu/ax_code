#pragma once

#include <iostream>
#include <string>

#include "Engine.hpp"

class Yolov5 : public Engine {
 public:
  explicit Yolov5(const std::string& model_config)
      : Engine(model_config) {}
  ~Yolov5() override;

  void Postprocess(int pic_width, int pic_height,
                   std::vector<detection::Object>& objects);

  Yolov5(const Yolov5&) = delete;
  Yolov5& operator=(const Yolov5&) = delete;

 private:
};