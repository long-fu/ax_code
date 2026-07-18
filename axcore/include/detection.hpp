/*
 * AXERA is pleased to support the open source community by making ax-samples available.
 *
 * Copyright (c) 2022, AXERA Semiconductor (Shanghai) Co., Ltd. All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

/*
 * Author: ls.wang
 */
#ifndef __detection__
#define __detection__
#pragma once

#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include "base.h"
namespace detection
{
    typedef struct
    {
        int grid0;
        int grid1;
        int stride;
    } GridAndStride;

    // typedef struct Object
    // {
    //     cv::Rect_<float> rect;
    //     int label;
    //     float prob;
    //     cv::Point2f landmark[5];
    //     /* for yolov5-seg */
    //     cv::Mat mask;
    //     std::vector<float> mask_feat;
    //     std::vector<float> kps_feat;
    //     /* for yolov8-obb */
    //     float angle;
    // } Object;
    typedef DetectionObject Object;

    /* for palm detection */
    typedef struct PalmObject
    {
        cv::Rect_<float> rect;
        float prob;
        cv::Point2f vertices[4];
        cv::Point2f landmarks[7];
        cv::Mat affine_trans_mat;
        cv::Mat affine_trans_mat_inv;
    } PalmObject;

    // static inline float sigmoid(float x);

    // static float softmax(const float *src, float *dst, int length);

    // template <typename T>
    // static inline float intersection_area(const T &a, const T &b);

    // template <typename T>
    // static void qsort_descent_inplace(std::vector<T> &faceobjects, int left, int right);

    // template <typename T>
    // static void qsort_descent_inplace(std::vector<T> &faceobjects);

    // template <typename T>
    // static void nms_sorted_bboxes(const std::vector<T> &faceobjects, std::vector<int> &picked, float nms_threshold);

    // void GenerateProposalsYolov5Face(int stride, const float *feat,
    //                                            float prob_threshold, std::vector<Object> &objects,
    //                                            int letterbox_cols, int letterbox_rows,
    //                                            const float *anchors, float prob_threshold_unsigmoid);

    void GenerateProposalsScrfd(int feat_stride, const float *score_blob,
                                         const float *bbox_blob, const float *kps_blob,
                                         float prob_threshold, std::vector<detection::Object> &faceobjects, int letterbox_cols, int letterbox_rows);

    void GenerateProposalsYolov5Face(const float *feat,
                                        float prob_threshold,
                                        std::vector<Object> &objects);

    void GenerateProposalsYolov5Face(int stride, int anchor_group,
                                        const float *feat, float prob_threshold, std::vector<Object> &objects,
                                        int letterbox_cols, int letterbox_rows,
                                        const float *anchors, const int anchor_num,
                                        float prob_threshold_unsigmoid);

    void GenerateProposalsYolov5(int stride, int anchor_group,
                                   const float *feat, float prob_threshold,
                                   std::vector<Object> &objects,
                                   int letterbox_cols, int letterbox_rows,
                                   const float *anchors, int anchor_num,
                                   float prob_threshold_unsigmoid,
                                   int cls_num = 80);

    // void generate_proposals_yolov8(int stride, const float *dfl_feat,
    //                                       const float *cls_feat, const float *cls_idx,
    //                                       float prob_threshold, std::vector<Object> &objects,
    //                                       int letterbox_cols, int letterbox_rows, int cls_num = 80);

    // void generate_proposals_yolov8_pose(int stride, const float *feat,
    //                                            float prob_threshold, std::vector<Object> &objects,
    //                                            int letterbox_cols, int letterbox_rows, const int num_point = 17);

    void GenerateProposalsYolov8Native(int stride, const float *feat, float prob_threshold,
                                          std::vector<Object> &objects,
                                          int letterbox_cols, int letterbox_rows, int cls_num = 80);

    void GenerateProposalsYolov8PoseNative(int stride, const float *feat,
                                               const float *feat_kps, float prob_threshold, std::vector<Object> &objects,
                                               int letterbox_cols, int letterbox_rows,
                                               const int num_point = 17, int cls_num = 1);

    void ReverseLetterbox(std::vector<Object> &proposal, std::vector<Object> &objects,
                           int letterbox_rows, int letterbox_cols, int src_rows, int src_cols);

    void GetOutBboxNoLetterbox(std::vector<Object> &proposals,
                                   std::vector<Object> &objects,
                                   const float nms_threshold,
                                   int letterbox_rows, int letterbox_cols,
                                   int src_rows, int src_cols);

    void GetOutBbox(std::vector<Object> &objects, int letterbox_rows,
                      int letterbox_cols, int src_rows, int src_cols);

    void GetOutBbox(std::vector<Object> &proposals, std::vector<Object> &objects,
                      const float nms_threshold, int letterbox_rows, int letterbox_cols,
                      int src_rows, int src_cols);

    void GetOutBboxMask(std::vector<Object> &proposals,
                           std::vector<Object> &objects, const float *mask_proto,
                           int mask_proto_dim, int mask_stride, const float nms_threshold,
                           int letterbox_rows, int letterbox_cols, int src_rows, int src_cols);

    void GetOutBboxKps(std::vector<Object> &proposals,
                          std::vector<Object> &objects, const float nms_threshold,
                          int letterbox_rows, int letterbox_cols, int src_rows, int src_cols);

    void GetOutBboxKpsNoLetterbox(std::vector<Object> &proposals, std::vector<Object> &objects,
                                       const float nms_threshold,
                                       int letterbox_rows, int letterbox_cols,
                                       int src_rows, int src_cols);
} // namespace detection
#endif