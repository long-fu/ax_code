#pragma once

#include <cstdint>
#include <vector>

#include "image_data.hpp"
#include "frame_data.hpp"
#include "detection.hpp"

namespace {

const int kMsgAppStart = 1;
const int kMsgVdecData = 2;
const int kMsgPreprocData = 3;
const int kMsgInfprocData = 4;
const int kMsgBusprocData = 5;
const int kMsgAppExit = 10;
const int kSendMsgReMax = 20;

}  // namespace

struct PreData {
  ImageData image;
  std::vector<uint8_t> data;
};

struct InfData {
  ImageData image;
  std::vector<detection::Object> objects;
};

struct BusData {
  ImageData image;
};
