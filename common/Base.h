#include <iostream>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <queue>
struct Object_
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
};


class BoxMgr {
    std::queue<Object_> boxQueue;

};

class Base
{
private:
    /* data */
public:
    Base(const std::string &configPath,cv::Size imgSize);
    virtual int Init() = 0;
    virtual int Proccess(std::vector<Object_> boxs) = 0;
    virtual int Destroy() = 0;
    ~Base();
};
