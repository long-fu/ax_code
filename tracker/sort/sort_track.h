#pragma once

#include <set>
#include "hungarian.h"
#include "kalman_tracker.h"
#include "datatrans.h"

#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"



class SortTracker
{
public:
    // int total_frames = 0;  // 记录总帧数
    double total_time = 0.0;  // 记录总耗时
    static const int max_num = 100;  // max num of people per frame
    cv::Scalar_<int> rand_color[max_num];  // 颜色数组

    int frame_count = 0;         // 记录处理了多少帧数据。由于刚调用update函数就会加一，所以实际是从1开始计数
    int max_lost_time = 3;       // 连续预测的最大次数，即目标未被检测到的帧数，超过之后会被删
    int lower_max_lost_time = 2; // 对新生tracker和稳定tracker的容忍度不同，如果稳定tracker连续丢失3次就被移除，那新生tracker则是2次
    int min_hits = 3;            // 目标命中的最小次数，小于该次数时getReport函数不返回该目标的KalmanTracker卡尔曼滤波对象
    double iou_threshold = 0.3;
    std::vector<KalmanTracker> trackers; // 维护所有的跟踪序列，列表元素是KalmanTracker的对象

    // variables used in the for-loop
    std::vector<cv::Rect_<float>> predicted_boxes;
    std::vector<std::vector<double>> iou_matrix;
    std::vector<int> assignment;
    std::set<int> unmatched_detections;
    std::set<int> unmatched_trajectories;
    std::set<int> all_items;
    std::set<int> matched_items;
    std::vector<cv::Point> matched_pairs;
    std::vector<TrackingBox> frame_tracking_result;  // 用于保存最新的对外输出结果
    unsigned int trk_num = 0;
    unsigned int det_num = 0;

    double cycle_time = 0.0;
    int64_t start_time = 0;

    SortTracker()
    {
        KalmanTracker::kkf_count_ = 0; // tracking id relies on this, so we have to reset it in each seq.
        cv::RNG rng(0xFFFFFFFF);
        for (int i = 0; i < max_num; i++)
            rng.fill(rand_color[i], cv::RNG::UNIFORM, 0, 256);
    }

    // Computes IOU between two bounding boxes
    double GetIOU(cv::Rect_<float> bb_test, cv::Rect_<float> bb_gt);
    // 更新全局跟踪器
    void Update(const std::vector<TrackingBox> &detFrameData);
    // 对外输出跟踪结果
    std::vector<TrackingBox> GetReport();
};
