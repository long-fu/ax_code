#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "FrameData.hpp"
#include <opencv2/opencv.hpp>

struct ImageData {
  AX_U32 u32Width;
  AX_U32 u32Height;
  AX_IMG_FORMAT_E enImgFormat;
  AX_BOOL bEndOfStream;
  std::shared_ptr<FrameData> data;

  std::chrono::steady_clock::time_point timePoint;
};

int Clone(ImageData& dest, const ImageData& src);
int JpegEncode(std::vector<uint8_t>& dest, const ImageData& src);
int Copy2Host(std::vector<uint8_t>& dest, const ImageData& src);
int Copy2Mat(cv::Mat& dest, const ImageData& src);
