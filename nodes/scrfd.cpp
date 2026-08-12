#include "scrfd.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "logger.h"

namespace {

struct ScrfdLevel {
  int stride = 0;
  const float *score = nullptr;
  const float *bbox = nullptr;
  const float *kps = nullptr;
  size_t n = 0; // H*W*2
};

// InsightFace packed layout: row = spatial_index * 2 + anchor.
// Convert to CHW expected by GenerateProposalsScrfd:
//   score[q * feat + i], bbox[(q*4+c)*feat + i], kps[(q*10+c)*feat + i]
void PackedToChw(const float *score_n1, const float *bbox_n4,
                 const float *kps_n10, size_t n, int feat_size,
                 std::vector<float> &score_chw, std::vector<float> &bbox_chw,
                 std::vector<float> &kps_chw) {
  constexpr int kNumAnchors = 2;
  score_chw.assign(static_cast<size_t>(kNumAnchors) * feat_size, 0.f);
  bbox_chw.assign(static_cast<size_t>(kNumAnchors) * 4 * feat_size, 0.f);
  kps_chw.assign(static_cast<size_t>(kNumAnchors) * 10 * feat_size, 0.f);

  for (int q = 0; q < kNumAnchors; ++q) {
    for (int index = 0; index < feat_size; ++index) {
      const size_t row = static_cast<size_t>(index) * kNumAnchors + q;
      if (row >= n) {
        continue;
      }
      score_chw[static_cast<size_t>(q) * feat_size + index] = score_n1[row];
      for (int c = 0; c < 4; ++c) {
        bbox_chw[static_cast<size_t>(q * 4 + c) * feat_size + index] =
            bbox_n4[row * 4 + c];
      }
      for (int c = 0; c < 10; ++c) {
        kps_chw[static_cast<size_t>(q * 10 + c) * feat_size + index] =
            kps_n10[row * 10 + c];
      }
    }
  }
}

int StrideFromN(size_t n, int letterbox) {
  if (n == 0 || n % 2 != 0) {
    return 0;
  }
  const size_t hw = n / 2;
  for (int stride : {8, 16, 32}) {
    const int feat = letterbox / stride;
    if (static_cast<size_t>(feat) * static_cast<size_t>(feat) == hw) {
      return stride;
    }
  }
  return 0;
}

} // namespace

Scrfd::~Scrfd() = default;

int Scrfd::Postprocess(int pic_width, int pic_height,
                        std::vector<detection::Object> &objects) {
  objects.clear();

  // inputs = {N, C, H, W}
  const int lb_h = config_.inputs.size() >= 4 ? config_.inputs[2] : 640;
  const int lb_w = config_.inputs.size() >= 4 ? config_.inputs[3] : 640;

  auto *info = GetInfo();
  if (info == nullptr) {
    LOG_ERROR("Det10g Postprocess: io info is null");
    return -1;
  }
  if (info->nOutputSize != 9) {
    LOG_ERROR("Det10g Postprocess: expected 9 outputs, got {}",
              info->nOutputSize);
    return -1;
  }

  std::unordered_map<int, ScrfdLevel> levels;
  for (uint32_t i = 0; i < info->nOutputSize; ++i) {
    auto &out = GetOutput().pOutputs[i];
    const auto *ptr = static_cast<const float *>(out.pVirAddr);
    if (ptr == nullptr || out.nSize == 0) {
      LOG_ERROR("Det10g Postprocess: empty output {}", i);
      return -2;
    }
    const size_t floats = out.nSize / sizeof(float);
    size_t n = 0;
    int kind = 0; // 1=score, 4=bbox, 10=kps
    if (floats == 12800 || floats == 3200 || floats == 800) {
      n = floats;
      kind = 1;
    } else if (floats == 12800ull * 4 || floats == 3200ull * 4 ||
               floats == 800ull * 4) {
      n = floats / 4;
      kind = 4;
    } else if (floats == 12800ull * 10 || floats == 3200ull * 10 ||
               floats == 800ull * 10) {
      n = floats / 10;
      kind = 10;
    } else {
      LOG_ERROR("Det10g Postprocess: unrecognized output size {} floats",
                floats);
      return -2;
    }

    const int stride = StrideFromN(n, lb_w);
    if (stride == 0) {
      LOG_ERROR("Det10g Postprocess: cannot map N={} to stride", n);
      return -2;
    }
    auto &level = levels[stride];
    level.stride = stride;
    level.n = n;
    if (kind == 1) {
      level.score = ptr;
    } else if (kind == 4) {
      level.bbox = ptr;
    } else {
      level.kps = ptr;
    }
  }

  if (levels.size() != 3) {
    LOG_ERROR("Det10g Postprocess: expected 3 stride levels, got {}",
              levels.size());
    return -2;
  }

  std::vector<detection::Object> proposals;
  for (int stride : {8, 16, 32}) {
    auto it = levels.find(stride);
    if (it == levels.end()) {
      LOG_ERROR("Det10g Postprocess: missing stride {}", stride);
      return -2;
    }
    const ScrfdLevel &lv = it->second;
    if (lv.score == nullptr || lv.bbox == nullptr || lv.kps == nullptr) {
      LOG_ERROR("Det10g Postprocess: incomplete tensors for stride {}", stride);
      return -2;
    }

    const int feat_w = lb_w / stride;
    const int feat_h = lb_h / stride;
    const int feat_size = feat_w * feat_h;
    if (static_cast<size_t>(feat_size) * 2 != lv.n) {
      LOG_ERROR("Det10g Postprocess: N mismatch stride {} feat {} N {}", stride,
                feat_size, lv.n);
      return -2;
    }

    std::vector<float> score_chw;
    std::vector<float> bbox_chw;
    std::vector<float> kps_chw;
    PackedToChw(lv.score, lv.bbox, lv.kps, lv.n, feat_size, score_chw, bbox_chw,
                kps_chw);

    // detection::GenerateProposalsScrfd(stride, score_chw.data(),
    // bbox_chw.data(),
    //                                   kps_chw.data(), cfg.prob_threshold,
    //                                   proposals, lb_w, lb_h);
    detection::generate_proposals_scrfd(
        stride, score_chw.data(), bbox_chw.data(), kps_chw.data(),
        config_.prob_threshold, proposals, lb_w, lb_h);
  }
  detection::get_out_bbox(proposals, objects, config_.nms_threshold, lb_h, lb_w,
                          pic_height, pic_width);
  return 0;
}
