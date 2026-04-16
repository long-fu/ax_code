

#ifndef __DRAWING__
#define __DRAWING__

#include <string>
#include <vector>
#include <tuple>
#include "ax_global_type.h"
struct YUVColor
{
    YUVColor(){}
    YUVColor(uint8_t value0, uint8_t value1, uint8_t value2,uint8_t value3) {
        // : val[0](value0), val[1](value1), val[1](value2),val[3](value3)
        val[0] = value0;
        val[1] = value1;
        val[2] = value2;
        val[3] = value3;
    }
    YUVColor(uint8_t y, uint8_t u, uint8_t v) : y(y), u(u), v(v) {}
    uint8_t y;
    uint8_t u;
    uint8_t v;
    int val[4]={0x0};
};

void SetPixel(AX_VIDEO_FRAME_INFO_T *frame_info, int x, int y, const YUVColor &color);

void DrawText(AX_VIDEO_FRAME_INFO_T *frame_info, int x, int y, const std::string &text, const YUVColor &color);

int DrawLine(AX_VIDEO_FRAME_INFO_T *frame_info, int x1, int y1, int x2, int y2, const YUVColor &color, int lineWidth);

void DrawClosedLines(AX_VIDEO_FRAME_INFO_T *frame_info, std::vector<std::tuple<int, int>> points, const YUVColor &color, int lineWidth);

void DrawRect(AX_VIDEO_FRAME_INFO_T *frame_info, int x1, int y1, int x2, int y2, const YUVColor &color, int lineWidth);

void DrawCircle(AX_VIDEO_FRAME_INFO_T *frame_info, int xCenter, int yCenter, int radius, YUVColor color);
#endif
