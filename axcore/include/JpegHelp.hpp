#pragma once

#include "ax_ivps_api.h"
#include "ax_venc_api.h"
#include "ax_vdec_api.h"
#include <cstdint>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ax_base_type.h"
#include "ax_global_type.h"
#include "ax_buffer_tool.h"
#include <vector>
class JpegHelp
{


public:
    JpegHelp(/* args */);
    ~JpegHelp();

    static int JpegDecode(AX_VIDEO_FRAME_INFO_T *dest, std::string const &src);

    static int JpegEncode(std::vector<uint8_t> &dest, AX_VIDEO_FRAME_INFO_T *src);    
};
