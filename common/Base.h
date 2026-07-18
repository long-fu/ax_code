#ifndef AX_BASE_H
#define AX_BASE_H

#include <iostream>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <queue>
struct DetectionObject
{
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

    std::string model;
};

class BoxMgr
{
    std::queue<DetectionObject> boxQueue;
};

class Base
{
private:
    /* data */
public:
    Base(const std::string &configPath, cv::Size imgSize);
    virtual int Init() = 0;
    virtual int Proccess(std::unordered_map<std::string, DetectionObject> boxs) = 0;
    virtual int Destroy() = 0;
    ~Base();
};

#endif // AX_BASE_H
