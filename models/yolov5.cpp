#include "yolov5.h"

#include <cmath>
#include <vector>

#include "detection.h"
#include "label_names.h"
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
  // inputs 是 NCHW：inputs[2]=H(rows)、inputs[3]=W(cols)。原实现把两者对调，
  // 640x640 时恰好相等看不出来，非正方形输入就会算错 feat 尺寸。
  int letterbox_rows = config_.inputs[2];
  int letterbox_cols = config_.inputs[3];

  int cls_num = labels.size();
  float prob_threshold_u_sigmoid =
      -1.0f * static_cast<float>(std::log((1.0f / prob_threshold) - 1.0f));

  if (GetInfo()->nOutputSize != strides.size()) {
    LOG_ERROR("Output size mismatch: {} != {}", GetInfo()->nOutputSize, strides.size());
    return -1;
  }

  // 下游 generate_proposals_yolov5 用 stride 反推 anchor_group，且只处理
  // 8/16/32；其余取值会让 anchor_group 保持未初始化并越界读 anchors。
  // 它同时把每层 anchor 数硬编码为 3，故配置不符时必须报错而不是读垃圾。
  for (size_t i = 0; i < strides.size(); ++i) {
    if (strides[i] != 8 && strides[i] != 16 && strides[i] != 32) {
      LOG_ERROR("Unsupported stride {} at level {}, only 8/16/32 supported",
                strides[i], i);
      return -1;
    }
    if (i < config_.num_anchors.size() && config_.num_anchors[i] != 3) {
      LOG_ERROR("Unsupported num_anchors {} at level {}, only 3 supported",
                config_.num_anchors[i], i);
      return -1;
    }
  }

  std::vector<detection::Object> proposals;

  for (uint32_t i = 0; i < GetInfo()->nOutputSize; ++i) {

    auto& output = GetOutput().pOutputs[i];
    
    auto ptr = static_cast<float*>(output.pVirAddr);

    // auto& info = GetInfo()->pOutputs[i];
    auto out_size = output.nSize;

    // 用配置里的 stride 而非 (1<<i)*8：后者只是碰巧等于默认的 {8,16,32}，
    // 改了 YAML 就会与实际输出层错位。
    int32_t stride = strides[i];

    // 校验必须与 generate_proposals_yolov5 的实际读取量一致：
    // 它按 feat_h=rows/stride、feat_w=cols/stride 遍历，每格 3 个 anchor、
    // 每 anchor (cls_num+5) 个 float。原实现两次用了 cols，非正方形输入时
    // 校验会误判，而该函数内部没有边界保护，等于放过静默越界读。
    size_t countSize =
        static_cast<size_t>(letterbox_cols / stride) *
            static_cast<size_t>(letterbox_rows / stride) *
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
  std::string error;
  if (!models::AssignLabelNames(labels, objects, error)) {
    LOG_ERROR("{} Postprocess: {}", config_.model_type, error);
    return -1;
  }
  // detection::GetOutBbox(proposals, objects, nms_threshold, letterbox_rows,
  //                         letterbox_cols, pic_height, pic_width);
  return 0;
}
