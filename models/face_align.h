#pragma once

#include <opencv2/opencv.hpp>

#include "detection_types.h"
#include "image_data.h"
#include "ivps_helper.h"

namespace face_align {

// Estimate 2x3 similarity transform from 5 landmarks to ArcFace template.
// landmark order: left-eye, right-eye, nose, left-mouth, right-mouth.
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
