#pragma once

#include "ImageData.hpp"
#include "FrameData.hpp"
#include "detection.hpp"

namespace {

    const int MSG_APP_START = 1;
    const int MSG_VDEC_DATA = 2;
    const int MSG_PREPROC_DATA = 3;
    const int MSG_INFPROC_DATA = 4;
    const int MSG_BUSPROC_DATA = 5;
    
    const int MSG_APP_EXIT = 10;


    const int SEND_MSG_RE_MAX = 20;
}

struct PreData
{
    ImageData image;
    std::vector<uint8_t> data;
};

struct InfData
{
    ImageData image;
    std::vector<detection::Object> objects;
};

struct BusData
{
    ImageData image;
};
