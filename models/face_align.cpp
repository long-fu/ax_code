#include "face_align.h"

#include <algorithm>
#include <cmath>

#include "image_data.h"
#include "logger.h"

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + ((align)-1)) & ~((align)-1))
#endif

namespace face_align {
namespace {

// InsightFace arcface_dst for 112x112.
const cv::Point2f kArcfaceDst112[5] = {
    {38.2946f, 51.6963f}, {73.5318f, 51.5014f}, {56.0252f, 71.7366f},
    {41.5493f, 92.3655f}, {70.7299f, 92.2041f},
};

// Similarity (scale+rot+trans) from src→dst using Umeyama (2D).
// Returns 2x3 CV_64F matrix, or empty on failure.
cv::Mat EstimateSimilarity(const cv::Point2f* src, const cv::Point2f* dst,
                           int n) {
  if (src == nullptr || dst == nullptr || n < 2) {
    return {};
  }

  cv::Point2d mean_s(0, 0), mean_d(0, 0);
  for (int i = 0; i < n; ++i) {
    mean_s.x += src[i].x;
    mean_s.y += src[i].y;
    mean_d.x += dst[i].x;
    mean_d.y += dst[i].y;
  }
  mean_s.x /= n;
  mean_s.y /= n;
  mean_d.x /= n;
  mean_d.y /= n;

  double var_s = 0.0;
  double cov00 = 0.0, cov01 = 0.0, cov10 = 0.0, cov11 = 0.0;
  for (int i = 0; i < n; ++i) {
    const double sx = src[i].x - mean_s.x;
    const double sy = src[i].y - mean_s.y;
    const double dx = dst[i].x - mean_d.x;
    const double dy = dst[i].y - mean_d.y;
    var_s += sx * sx + sy * sy;
    cov00 += sx * dx;
    cov01 += sx * dy;
    cov10 += sy * dx;
    cov11 += sy * dy;
  }
  var_s /= n;
  cov00 /= n;
  cov01 /= n;
  cov10 /= n;
  cov11 /= n;

  cv::Mat cov = (cv::Mat_<double>(2, 2) << cov00, cov01, cov10, cov11);
  cv::Mat w, u, vt;
  cv::SVDecomp(cov, w, u, vt, cv::SVD::FULL_UV);

  cv::Mat r = vt.t() * u.t();
  if (cv::determinant(r) < 0) {
    vt.at<double>(1, 0) *= -1;
    vt.at<double>(1, 1) *= -1;
    r = vt.t() * u.t();
  }

  double scale = 1.0;
  if (var_s > 1e-12) {
    scale = (w.at<double>(0) + w.at<double>(1)) / var_s;
  }

  const double a = scale * r.at<double>(0, 0);
  const double b = scale * r.at<double>(0, 1);
  const double c = scale * r.at<double>(1, 0);
  const double d = scale * r.at<double>(1, 1);
  const double tx = mean_d.x - (a * mean_s.x + b * mean_s.y);
  const double ty = mean_d.y - (c * mean_s.x + d * mean_s.y);

  return (cv::Mat_<double>(2, 3) << a, b, tx, c, d, ty);
}

// Match IvpsHelper::CropAndCSC even-alignment, then clamp inside frame.
cv::Rect AlignRoiForIvps(cv::Rect roi, int frame_w, int frame_h) {
  int x = static_cast<int>(ALIGN_UP(std::max(0, roi.x), 2));
  int y = static_cast<int>(ALIGN_UP(std::max(0, roi.y), 2));
  int w = static_cast<int>(ALIGN_UP(std::max(2, roi.width), 2));
  int h = static_cast<int>(ALIGN_UP(std::max(2, roi.height), 2));

  if (x + w > frame_w) {
    w = frame_w - x;
    w = w & ~1;
  }
  if (y + h > frame_h) {
    h = frame_h - y;
    h = h & ~1;
  }
  if (w < 2 || h < 2) {
    return {};
  }
  return cv::Rect(x, y, w, h);
}

}  // namespace

cv::Mat EstimateNorm(const cv::Point2f landmark[5], int image_size) {
  if (landmark == nullptr || image_size <= 0) {
    return {};
  }

  cv::Point2f dst[5];
  const float ratio = static_cast<float>(image_size) / 112.f;
  for (int i = 0; i < 5; ++i) {
    dst[i] = kArcfaceDst112[i] * ratio;
  }

  cv::Mat M = EstimateSimilarity(landmark, dst, 5);
  if (M.empty()) {
    LOG_ERROR("face_align::EstimateNorm failed");
  }
  return M;
}

cv::Mat NormCrop(const cv::Mat& image, const cv::Point2f landmark[5],
                 int image_size) {
  cv::Mat aligned;
  if (image.empty() || landmark == nullptr || image_size <= 0) {
    return aligned;
  }

  cv::Mat M = EstimateNorm(landmark, image_size);
  if (M.empty()) {
    return aligned;
  }

  cv::warpAffine(image, aligned, M, cv::Size(image_size, image_size),
                 cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
  return aligned;
}

cv::Mat NormCrop(const cv::Mat& image, const detection::Object& face,
                 int image_size) {
  return NormCrop(image, face.landmark, image_size);
}

cv::Rect ComputeExpandedRoi(const cv::Rect_<float>& box, float expand_ratio,
                            int frame_w, int frame_h, int min_side) {
  if (frame_w <= 0 || frame_h <= 0 || expand_ratio <= 0.f) {
    return {};
  }
  if (box.width <= 0.f || box.height <= 0.f) {
    return {};
  }

  const float cx = box.x + box.width * 0.5f;
  const float cy = box.y + box.height * 0.5f;
  float nw = box.width * expand_ratio;
  float nh = box.height * expand_ratio;

  // Small faces: keep enough source pixels for NormCrop upscale to image_size.
  if (min_side > 0) {
    const float need_w = static_cast<float>(std::min(min_side, frame_w));
    const float need_h = static_cast<float>(std::min(min_side, frame_h));
    nw = std::max(nw, need_w);
    nh = std::max(nh, need_h);
  }

  float x0 = cx - nw * 0.5f;
  float y0 = cy - nh * 0.5f;
  float x1 = cx + nw * 0.5f;
  float y1 = cy + nh * 0.5f;

  // If min-size push goes past borders, shift ROI back into frame.
  if (x0 < 0.f) {
    x1 -= x0;
    x0 = 0.f;
  }
  if (y0 < 0.f) {
    y1 -= y0;
    y0 = 0.f;
  }
  if (x1 > static_cast<float>(frame_w)) {
    const float d = x1 - static_cast<float>(frame_w);
    x0 -= d;
    x1 = static_cast<float>(frame_w);
  }
  if (y1 > static_cast<float>(frame_h)) {
    const float d = y1 - static_cast<float>(frame_h);
    y0 -= d;
    y1 = static_cast<float>(frame_h);
  }
  x0 = std::max(0.f, x0);
  y0 = std::max(0.f, y0);
  x1 = std::min(static_cast<float>(frame_w), x1);
  y1 = std::min(static_cast<float>(frame_h), y1);

  int ix = static_cast<int>(std::floor(x0));
  int iy = static_cast<int>(std::floor(y0));
  int iw = static_cast<int>(std::ceil(x1) - ix);
  int ih = static_cast<int>(std::ceil(y1) - iy);
  if (iw < 1 || ih < 1) {
    return {};
  }
  if (ix + iw > frame_w) {
    iw = frame_w - ix;
  }
  if (iy + ih > frame_h) {
    ih = frame_h - iy;
  }
  return cv::Rect(ix, iy, iw, ih);
}

void RemapLandmarksToRoi(const cv::Point2f src[5], cv::Point2f dst[5],
                         float roi_x, float roi_y) {
  if (src == nullptr || dst == nullptr) {
    return;
  }
  for (int i = 0; i < 5; ++i) {
    dst[i].x = src[i].x - roi_x;
    dst[i].y = src[i].y - roi_y;
  }
}

cv::Mat HwRoiNormCrop(IvpsHelper& ivps, const ImageData& frame,
                      const detection::Object& face,
                      ImageData &face_img, 
                      float expand_ratio,int image_size) {
  cv::Mat aligned;
  if (frame.data == nullptr || frame.data->FrameInfo() == nullptr ||
      image_size <= 0) {
    LOG_ERROR("HwRoiNormCrop: invalid frame");
    return aligned;
  }

  const int frame_w = static_cast<int>(frame.width > 0
                                           ? frame.width
                                           : frame.data->FrameInfo()
                                                 ->stVFrame.u32Width);
  const int frame_h = static_cast<int>(frame.height > 0
                                           ? frame.height
                                           : frame.data->FrameInfo()
                                                 ->stVFrame.u32Height);
  if (frame_w <= 0 || frame_h <= 0) {
    LOG_ERROR("HwRoiNormCrop: invalid frame size");
    return aligned;
  }

  cv::Rect roi = ComputeExpandedRoi(face.rect, expand_ratio, frame_w, frame_h,
                                    image_size);
  roi = AlignRoiForIvps(roi, frame_w, frame_h);
  if (roi.empty()) {
    LOG_ERROR("HwRoiNormCrop: empty ROI after expand/align");
    return aligned;
  }

  AX_S32 ret = ivps.CropAndCSC(AX_FORMAT_RGB888,
                               static_cast<AX_U16>(roi.x),
                               static_cast<AX_U16>(roi.y),
                               static_cast<AX_U16>(roi.width),
                               static_cast<AX_U16>(roi.height));
  
  if (ret != 0) {
    LOG_ERROR("HwRoiNormCrop: CropAndCSC failed, ret={}", ret);
    return aligned;
  }

  ImageData roi_frame;
  ret = ivps.Process(roi_frame, frame);
  if (ret != 0) {
    LOG_ERROR("HwRoiNormCrop: IVPS Process failed, ret={}", ret);
    return aligned;
  }

  face_img = roi_frame;
  
  cv::Mat roi_bgr;
  if (Copy2Mat(roi_bgr, roi_frame) != 0 || roi_bgr.empty()) {
    LOG_ERROR("HwRoiNormCrop: Copy2Mat failed");
    return aligned;
  }
  // cv::imwrite("roi_bgr.jpg", roi_bgr);
  
  cv::Point2f lm_roi[5];
  RemapLandmarksToRoi(face.landmark, lm_roi, static_cast<float>(roi.x),
                      static_cast<float>(roi.y));

  aligned = NormCrop(roi_bgr, lm_roi, image_size);

  // cv::imwrite("aligned.jpg", aligned);
  
  if (aligned.empty()) {
    LOG_ERROR("HwRoiNormCrop: NormCrop failed");
  }
  return aligned;
}

}  // namespace face_align
