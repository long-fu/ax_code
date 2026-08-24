#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace detection {



typedef struct {
  int grid0;
  int grid1;
  int stride;
} GridAndStride;

typedef struct Object {
  cv::Rect_<float> rect;
  int label;
  float prob;
  cv::Point2f landmark[5];
  /* for yolov5-seg */
  cv::Mat mask;
  std::vector<float> mask_feat;
  std::vector<float> kps_feat;
  /* for yolov8-obb */
  float angle;
  /* detector / model provenance for multi-model pipelines */
  std::string model_name;
  /* ByteTrack 轨迹 ID：由 BusProcess 在跟踪后回写；-1 表示尚未关联到激活轨 */
  int track_id = -1;
} Object;

/* for palm detection */
typedef struct PalmObject {
  cv::Rect_<float> rect;
  float prob;
  cv::Point2f vertices[4];
  cv::Point2f landmarks[7];
  cv::Mat affine_trans_mat;
  cv::Mat affine_trans_mat_inv;
} PalmObject;

}  // namespace detection
