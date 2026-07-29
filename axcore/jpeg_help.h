#pragma once

#include <cstdint>
#include <stdlib.h>
#include "ax_global_type.h"
#include <vector>
#include <string>
class JpegHelp
{


public:
    JpegHelp(/* args */);
    ~JpegHelp();

    static int JpegDecode(AX_VIDEO_FRAME_INFO_T **dest, std::string const &src);

    static int JpegEncode(std::vector<uint8_t> &dest, AX_VIDEO_FRAME_INFO_T *src);    
};
