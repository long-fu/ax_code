#pragma once

#include <opencv2/opencv.hpp>


struct BoundingBox
{
    float score = 0.0f;
    int class_id = -1;  // 类别ID
    int bbox_id = -1;  // bbox id in current frame
    cv::Rect_<float> rect;
};


struct TrackingBox
{
    int frame_id = 0;
    int track_id = -1;
    int class_id = -1;
    float obj_conf = 0.0f;  // 是否为前景的置信度
    cv::Rect_<float> box;

    // 构造函数
    TrackingBox(){}  
    // 重载构造函数
    TrackingBox(BoundingBox obj){
        box = obj.rect;
        obj_conf = obj.score;
        class_id = obj.class_id;
        track_id = -1;  // 初始化为-1
    }
};