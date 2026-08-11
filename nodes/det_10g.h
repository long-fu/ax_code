#pragma once

#include <string>
#include <vector>

#include "engine.h"

// void Det10g::ApplyDefaults() {
//   config_.model_file = "model/det_10g.axmodel";
//   config_.model_type = "det_10g";
//   config_.inputs = {1, 3, 640, 640};
//   config_.prob_threshold = 0.5f;
//   config_.nms_threshold = 0.4f;
//   config_.strides = {8, 16, 32};
//   config_.labels = {"face"};
//   config_.num_anchors = {2, 2, 2};
//   config_.anchors.clear();
// }


struct ScrfdConfig {
  std::string model_file = "model/det_10g.axmodel";
  std::string model_type = "scrfd";

  std::vector<int> inputs = {1, 3, 640, 640};

  float prob_threshold = 0.5f;
  float nms_threshold = 0.4f;

  std::vector<int> num_anchors = {2, 2, 2};
  std::vector<int> strides = {8, 16, 32};

  std::vector<std::string> labels = {"face"};

  std::string config_path;
  explicit ScrfdConfig(const std::string& path = "") : config_path(path) {
    if (!path.empty()) {
      // Allow passing either a config path or a model path string.
      model_file = path;
    }
  }
  int Init() { return 0; }
  int LoadConfig() { return 0; }
};

class Det10g : public Engine {
 public:
  explicit Det10g(const ScrfdConfig& config):Engine(config.model_file), config_(config)  {


  };
  ~Det10g() override;

  int Postprocess(int pic_width, int pic_height,
                  std::vector<detection::Object>& objects) override;

  Det10g(const Det10g&) = delete;
  Det10g& operator=(const Det10g&) = delete;

 private:
  ScrfdConfig config_;
};
