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
#include "detection_types.h"

class Base
{
private:
    /* data */
public:
    Base(const std::string &configPath, cv::Size imgSize);
    virtual int Init() = 0;
    virtual int Proccess(std::unordered_map<std::string, detection::Object> boxs) = 0;
    virtual int Destroy() = 0;
    ~Base();
};

#endif // AX_BASE_H
