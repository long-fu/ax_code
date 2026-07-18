#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "frame_data.hpp"
#include <opencv2/opencv.hpp>

struct ImageData {
  AX_U32 width;
  AX_U32 height;
  AX_IMG_FORMAT_E img_format;
  AX_BOOL end_of_stream;
  std::shared_ptr<FrameData> data;

  std::chrono::steady_clock::time_point time_point;
};

int Clone(ImageData& dest, const ImageData& src);
int JpegEncode(std::vector<uint8_t>& dest, const ImageData& src);
int JpegDecode(ImageData &dest, std::string const &jpegFile);
int Copy2Host(std::vector<uint8_t>& dest, const ImageData& src);
int Copy2Mat(cv::Mat& dest, const ImageData& src);
int Map(ImageData& img);
int Unmap(ImageData& img);

