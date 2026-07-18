#include "Yolov5.hpp"

#include <cmath>
#include <vector>

#include "Logger.h"

Yolov5::~Yolov5() {}

int Yolov5::Postprocess(int pic_width, int pic_height,
                        std::vector<detection::Object>& objects) {
  EngineConfig config = GetConfig();
  std::vector<float> anchors;
  for (size_t i = 0; i < config.anchors.size(); i++) {
    for (size_t j = 0; j < config.anchors[i].size(); j++) {
      anchors.push_back(config.anchors[i][j]);
    }
  }

  std::vector<int> strides = config.strides;
  std::vector<std::string> labels = config.labels;

  float prob_threshold = config.prob_threshold;
  float nms_threshold = config.nms_threshold;
  int letterbox_cols = config.inputs[2];
  int letterbox_rows = config.inputs[3];

  int cls_num = labels.size();
  float prob_threshold_u_sigmoid =
      -1.0f * static_cast<float>(std::log((1.0f / prob_threshold) - 1.0f));

  if (GetInfo()->nOutputSize != strides.size()) {
    LOG_ERROR("Output size mismatch: {} != {}", GetInfo()->nOutputSize, strides.size());
    return -1;
  }

  std::vector<detection::Object> proposals;

  for (uint32_t i = 0; i < GetInfo()->nOutputSize; ++i) {
    auto& output = GetOutput().pOutputs[i];
    auto ptr = static_cast<float*>(output.pVirAddr);
    auto out_size = output.nSize;
    int32_t stride = strides[i];

    size_t countSize =
        (letterbox_cols / stride) * (letterbox_cols / stride) *
            (labels.size() + 5) * 3 * sizeof(float);

    if (countSize != out_size) {
      LOG_ERROR("Output buffer size mismatch: {} != {}", countSize, out_size);
      return -2;
    }

    detection::GenerateProposalsYolov5(
        stride, i + 1, ptr, prob_threshold, proposals, letterbox_cols,
        letterbox_rows, anchors.data(), 3, prob_threshold_u_sigmoid, cls_num);
  }

  detection::GetOutBbox(proposals, objects, nms_threshold, letterbox_rows,
                          letterbox_cols, pic_height, pic_width);
  return 0;
}