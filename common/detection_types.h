#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace detection {

    struct CenterKey
    {
        int h, w, stride;
        bool operator==(const CenterKey &o) const
        {
            return h == o.h && w == o.w && stride == o.stride;
        }
    };
    struct CenterKeyHash
    {
        size_t operator()(const CenterKey &k) const
        {
            return (static_cast<size_t>(k.h) * 1315423911u) ^ (static_cast<size_t>(k.w) << 1) ^
                   static_cast<size_t>(k.stride);
        }
    };

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
