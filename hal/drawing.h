#pragma once

#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>
#include <ax_global_type.h>

// DrawText backend: 1 = bitmap OSD (hal/osd), 0 = FreeType.
#ifndef AX_USE_OSD_TEXT
#define AX_USE_OSD_TEXT 1
#endif

struct YUVColor {
    constexpr YUVColor() = default;
    constexpr YUVColor(uint8_t y, uint8_t u, uint8_t v) : y(y), u(u), v(v) {}
    constexpr YUVColor(uint8_t value0, uint8_t value1,
                       uint8_t value2, uint8_t value3) {
        val[0] = value0; val[1] = value1;
        val[2] = value2; val[3] = value3;
    }

    uint8_t y = 0;
    uint8_t u = 0;
    uint8_t v = 0;
    int val[4] = {0};
};

// RGB → YUV 转换（BT.601 full range）
inline YUVColor Rgb2Yuv(uint8_t r, uint8_t g, uint8_t b) {
    return {
        static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b),
        static_cast<uint8_t>(-0.169 * r - 0.331 * g + 0.500 * b + 128),
        static_cast<uint8_t>(0.500 * r - 0.419 * g - 0.081 * b + 128),
    };
}

// 预定义颜色（BT.601 full range）
namespace YUVColors {

inline constexpr YUVColor kWhite  {255, 128, 128};
inline constexpr YUVColor kBlack  {0,   128, 128};
inline constexpr YUVColor kRed    {76,  85,  255};
inline constexpr YUVColor kGreen  {150, 44,  21};
inline constexpr YUVColor kBlue   {29,  255, 107};
inline constexpr YUVColor kYellow {226, 0,   149};
inline constexpr YUVColor kCyan   {179, 171, 0};
inline constexpr YUVColor kMagenta{105, 212, 234};
inline constexpr YUVColor kOrange {165, 34,  239};
inline constexpr YUVColor kGray   {128, 128, 128};
inline constexpr YUVColor kPink   {163, 137, 196};

}  // namespace YUVColors

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
