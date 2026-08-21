#pragma once

#include <opencv2/opencv.hpp>

#include "detection_types.h"
#include "image_data.h"
#include "ivps_helper.h"

namespace face_align {

// landmark order: left-eye, right-eye, nose, left-mouth, right-mouth.

struct FrontalConfig {
  float min_eye_dist = 16.f;   // pixels; too small => invalid
  float max_roll_deg = 35.f;   // abs(eye-line angle)
  float max_yaw_proxy = 0.55f; // |nose offset from eye mid| / eye_dist
  float min_sym = 0.40f;       // min/max of horizontal eye-nose distances
  // Pitch gate (mouth mid vs eye-line). Only applied when AX_FACE_FRONTAL_USE_PITCH=1.
  float min_pitch_proxy = 0.25f;
  float max_pitch_proxy = 1.40f;
};

struct FrontalMetrics {
  float eye_dist = 0.f;
  float roll_deg = 0.f;
  float yaw_proxy = 0.f;
  float pitch_proxy = 0.f;  // mouth mid vertical offset / eye_dist (below eyes > 0)
  float sym = 0.f;
  bool valid = false;  // landmarks usable (eye_dist ok)
};

// Enable extreme pitch filter for local A/B tests:
//   -DAX_FACE_FRONTAL_USE_PITCH=1
#ifndef AX_FACE_FRONTAL_USE_PITCH
#define AX_FACE_FRONTAL_USE_PITCH 0
#endif


// Fill metrics from 5 landmarks. Returns false if landmarks unusable.
bool ComputeFrontalMetrics(const cv::Point2f landmark[5],
                            FrontalMetrics& out);

float EstimateYawProxy(const cv::Point2f landmark[5]);

bool IsFrontalFace(const cv::Point2f landmark[5],
                   const FrontalConfig& cfg = {},
                   FrontalMetrics* metrics = nullptr);

bool IsFrontalFace(const detection::Object& face,
                   const FrontalConfig& cfg = {},
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
                      ImageData &face_img,
                      float expand_ratio = 1.5f, int image_size = 112);

}  // namespace face_align
