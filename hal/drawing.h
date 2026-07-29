#pragma once

#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>
#include <ax_global_type.h>

struct YUVColor {
    YUVColor() = default;
    YUVColor(uint8_t y, uint8_t u, uint8_t v) : y(y), u(u), v(v) {}
    YUVColor(uint8_t value0, uint8_t value1, uint8_t value2, uint8_t value3) {
        val[0] = value0;
        val[1] = value1;
        val[2] = value2;
        val[3] = value3;
    }
    uint8_t y = 0;
    uint8_t u = 0;
    uint8_t v = 0;
    int val[4] = {0};
};

void SetPixel(AX_VIDEO_FRAME_INFO_T* frame, int x, int y, const YUVColor& color);

void DrawText(AX_VIDEO_FRAME_INFO_T* frame, int x, int y,
              const std::string& text, const YUVColor& color);

int DrawLine(AX_VIDEO_FRAME_INFO_T* frame, int x1, int y1, int x2, int y2,
             const YUVColor& color, int lineWidth);

void DrawClosedLines(AX_VIDEO_FRAME_INFO_T* frame,
                     std::vector<std::tuple<int, int>> points,
                     const YUVColor& color, int lineWidth);

void DrawRect(AX_VIDEO_FRAME_INFO_T* frame, int x1, int y1, int x2, int y2,
              const YUVColor& color, int lineWidth);

void DrawCircle(AX_VIDEO_FRAME_INFO_T* frame, int cx, int cy, int radius,
                YUVColor color);
