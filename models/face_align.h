#pragma once

#include <opencv2/opencv.hpp>

#include "detection_types.h"
#include "image_data.h"
#include "ivps_helper.h"

namespace face_align {

// 五点顺序：左眼、右眼、鼻尖、左嘴角、右嘴角（与 Scrfd / InsightFace 一致）。
//
// 正脸判定用途：在提特征前排除
//   1) 完全侧脸  2) 明显抬头/低头（需打开 pitch 宏）  3) 脸太小、五官点不可用
// 不要求「绝对正视」，推荐档在可识别与少误杀之间折中。

// ---------------------------------------------------------------------------
// 门限配置：调参方向见各字段注释（「更严」= 更容易判为非正脸）
// 结构体默认值 = 推荐档 FrontalRecommended()
// ---------------------------------------------------------------------------
struct FrontalConfig {
  // 两眼像素距离下限。过小表示人脸太远/太小、特征不足。调大更严。
  float min_eye_dist = 16.f;
  // 双眼连线相对水平的最大倾角（度）。超过视为歪头过大。调小更严。
  float max_roll_deg = 30.f;
  // |yaw_proxy| 上限。鼻尖相对双眼中点沿眼轴的偏移/眼距；越大越侧脸。调小更严。
  float max_yaw_proxy = 0.50f;
  // 左右对称度下限（鼻到左/右眼在眼轴投影距离的 min/max）。调大更严。
  float min_sym = 0.40f;
  // 俯仰门限（嘴中点相对眼轴的「下方」偏移/眼距）。仅当
  // AX_FACE_FRONTAL_USE_PITCH=1 时参与判定；调窄区间更严。
  float min_pitch_proxy = 0.25f;  // 过小多对应抬头过度
  float max_pitch_proxy = 1.40f;  // 过大多对应低头过度
};

// ---------------------------------------------------------------------------
// 由五点算出的几何量（与是否「通过正脸门限」无关；valid 只表示算数成功）
// ---------------------------------------------------------------------------
struct FrontalMetrics {
  float eye_dist = 0.f;     // 两眼距离（像素）
  float roll_deg = 0.f;     // 双眼连线倾角（度），0 接近水平
  float yaw_proxy = 0.f;    // 左右转代理量，约 0 更正脸，|值|大更侧
  float pitch_proxy = 0.f;  // 俯仰代理量：眼水平时嘴在眼下方通常 > 0
  float sym = 0.f;          // 左右对称，越接近 1 越对称
  bool valid = false;       // landmark 是否算出可用几何（眼距过小则为 false）
};

// 是否启用俯仰过滤。默认关闭；要挡抬头/低头时在编译选项加：
//   -DAX_FACE_FRONTAL_USE_PITCH=1
#ifndef AX_FACE_FRONTAL_USE_PITCH
#define AX_FACE_FRONTAL_USE_PITCH 0
#endif

// ---- 三套预设：排除全侧脸 / 俯仰(需宏) / 特征过少 ----

// 宽松：少误杀，仍挡全侧脸与过小脸。
inline FrontalConfig FrontalLoose() {
  FrontalConfig c;
  c.min_eye_dist = 12.f;
  c.max_roll_deg = 40.f;
  c.max_yaw_proxy = 0.70f;
  c.min_sym = 0.30f;
  c.min_pitch_proxy = 0.15f;
  c.max_pitch_proxy = 1.60f;
  return c;
}

// 推荐：门禁/抓拍提特征前默认建议使用本套。
inline FrontalConfig FrontalRecommended() {
  FrontalConfig c;
  c.min_eye_dist = 16.f;
  c.max_roll_deg = 30.f;
  c.max_yaw_proxy = 0.50f;
  c.min_sym = 0.40f;
  c.min_pitch_proxy = 0.25f;
  c.max_pitch_proxy = 1.40f;
  return c;
}

// 严格：更接近正脸才过，侧脸与大俯仰更容易被拒，特征质量更好。
inline FrontalConfig FrontalStrict() {
  FrontalConfig c;
  c.min_eye_dist = 20.f;
  c.max_roll_deg = 20.f;
  c.max_yaw_proxy = 0.35f;
  c.min_sym = 0.55f;
  c.min_pitch_proxy = 0.35f;
  c.max_pitch_proxy = 1.20f;
  return c;
}

// 从五点填充 FrontalMetrics；landmark 不可用时返回 false。
bool ComputeFrontalMetrics(const cv::Point2f landmark[5],
                           FrontalMetrics& out);

float EstimateYawProxy(const cv::Point2f landmark[5]);

// 先算 metrics，再与 cfg 逐项比较；全部通过才视为正脸。
// cfg 默认 {} 即结构体默认值（推荐档）。业务建议显式传 FrontalRecommended()。
bool IsFrontalFace(const cv::Point2f landmark[5],
                   const FrontalConfig& cfg = {},
                   FrontalMetrics* metrics = nullptr);

bool IsFrontalFace(const detection::Object& face,
                   const FrontalConfig& cfg = {},
                   FrontalMetrics* metrics = nullptr);

// ---------------------------------------------------------------------------
// 加权正脸分数（与上面硬门限并存）
// 各几何量先归一成约 0~1「越好越高」，再加权求和，返回总分。
// 是否达到业务门槛、同轨取最高分等，由调用方自行记录与比较；本模块不设 score_thresh。
// 硬门限 IsFrontalFace 逻辑不变。
// ---------------------------------------------------------------------------
struct FrontalScoreConfig {
  // 归一化参考：|指标|/ref 用于压到 [0,1] 的「差」再取 1-差
  float yaw_ref = 0.70f;   // |yaw_proxy| / yaw_ref
  float roll_ref = 40.f;   // |roll_deg| / roll_ref
  float eye_ref = 24.f;    // eye_dist / eye_ref，饱和到 1
  float pitch_lo = 0.25f;  // 理想俯仰下界
  float pitch_hi = 1.40f;  // 理想俯仰上界
  // 权重：默认 yaw+roll+sym+eye = 1.0；w_pitch 默认 0（不计入）
  float w_yaw = 0.35f;
  float w_roll = 0.15f;
  float w_sym = 0.25f;
  float w_eye = 0.25f;
  float w_pitch = 0.f;
};

// 宽松：侧脸权重略低，眼距权重略高，少误杀。
inline FrontalScoreConfig FrontalScoreLoose() {
  FrontalScoreConfig c;
  c.w_yaw = 0.30f;
  c.w_roll = 0.15f;
  c.w_sym = 0.25f;
  c.w_eye = 0.30f;
  c.w_pitch = 0.f;
  return c;
}

// 推荐：默认权重（结构体默认值一致）。
inline FrontalScoreConfig FrontalScoreRecommended() {
  return FrontalScoreConfig{};
}

// 严格：更看重侧脸/对称。
inline FrontalScoreConfig FrontalScoreStrict() {
  FrontalScoreConfig c;
  c.w_yaw = 0.40f;
  c.w_roll = 0.10f;
  c.w_sym = 0.30f;
  c.w_eye = 0.20f;
  c.w_pitch = 0.f;
  return c;
}

// 返回约 [0,1]；metrics 不可用时返回 0。可选写出 FrontalMetrics。
float ComputeFrontalScore(const cv::Point2f landmark[5],
                          const FrontalScoreConfig& cfg = {},
                          FrontalMetrics* metrics = nullptr);

float ComputeFrontalScore(const detection::Object& face,
                          const FrontalScoreConfig& cfg = {},
                          FrontalMetrics* metrics = nullptr);

// Estimate 2x3 similarity transform from 5 landmarks to ArcFace template.
// Returns empty Mat on failure.
cv::Mat EstimateNorm(const cv::Point2f landmark[5], int image_size = 112);

// Warp image to aligned square crop (InsightFace NormCrop).
cv::Mat NormCrop(const cv::Mat& image, const cv::Point2f landmark[5],
                 int image_size = 112);

cv::Mat NormCrop(const cv::Mat& image, const detection::Object& face,
                 int image_size = 112);

// Expand box about center by expand_ratio, clamp to frame.
// If min_side > 0, grow ROI about center to at least min_side×min_side when
// the frame is large enough (handles faces smaller than NormCrop output).
cv::Rect ComputeExpandedRoi(const cv::Rect_<float>& box, float expand_ratio,
                            int frame_w, int frame_h, int min_side = 0);

// Remap full-image landmarks into ROI-local coordinates.
void RemapLandmarksToRoi(const cv::Point2f src[5], cv::Point2f dst[5],
                         float roi_x, float roi_y);

// IVPS crops expanded bbox to RGB888 ROI, remaps landmarks, then NormCrop.
// Returns empty Mat on failure.
cv::Mat HwRoiNormCrop(IvpsHelper& ivps, const ImageData& frame,
                      const detection::Object& face,
                      ImageData& face_img,
                      float expand_ratio = 1.5f, int image_size = 112);

}  // namespace face_align
