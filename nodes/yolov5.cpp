#include "yolov5.h"

#include <cmath>
#include <vector>

#include "detection.h"
#include "logger.h"

Yolov5::~Yolov5() {}

int Yolov5::Postprocess(int pic_width, int pic_height,
                        std::vector<detection::Object>& objects) {
  // EngineConfig config = GetConfig();
  std::vector<float> anchors;
  for (size_t i = 0; i < config_.anchors.size(); i++) {
    for (size_t j = 0; j < config_.anchors[i].size(); j++) {
      anchors.push_back(config_.anchors[i][j]);
    }
  }

  std::vector<int> strides = config_.strides;
  std::vector<std::string> labels = config_.labels;

  float prob_threshold = config_.prob_threshold;
  float nms_threshold = config_.nms_threshold;
  int letterbox_cols = config_.inputs[2];
  int letterbox_rows = config_.inputs[3];

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

    // auto& info = GetInfo()->pOutputs[i];
    auto out_size = output.nSize;
    
    // int32_t stride = strides[i];
    int32_t stride = (1 << i) * 8;

    size_t countSize =
        (letterbox_cols / stride) * (letterbox_cols / stride) *
            (labels.size() + 5) * 3 * sizeof(float);

    if (countSize != out_size) {
      LOG_ERROR("Output buffer size mismatch: {} != {}", countSize, out_size);
      return -2;
    }

    detection::generate_proposals_yolov5(stride, ptr, prob_threshold, proposals, letterbox_cols, letterbox_rows, anchors.data(), prob_threshold_u_sigmoid,cls_num);
    // detection::GenerateProposalsYolov5(
    //     stride, i + 1, ptr, prob_threshold, proposals, letterbox_cols,
    //     letterbox_rows, anchors.data(), 3, prob_threshold_u_sigmoid, cls_num);
  }
  detection::get_out_bbox(proposals, objects, nms_threshold, letterbox_rows, letterbox_cols, pic_height, pic_width);
  // detection::GetOutBbox(proposals, objects, nms_threshold, letterbox_rows,
  //                         letterbox_cols, pic_height, pic_width);
  return 0;
}