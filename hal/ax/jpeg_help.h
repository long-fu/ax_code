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

    /*
        只能支持解码成YUV数据
    */
    static int JpegDecode(AX_VIDEO_FRAME_INFO_T **dest, std::string const &src);

    /*
        只能支持YUV数据编码
    */
    static int JpegEncode(std::vector<uint8_t> &dest, AX_VIDEO_FRAME_INFO_T *src);    
};
