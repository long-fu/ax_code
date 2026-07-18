
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdint.h>
#include <string>
#include <stdio.h>
#include "drawing.h"
#include <tuple>
#include <opencv2/opencv.hpp>
#include "freetype_helper.h"
#include "logger.h"
using namespace cv;

#pragma GCC push_options
#pragma GCC optimize("O0")

void SetPixel(AX_VIDEO_FRAME_INFO_T *frame_info, int x, int y, const YUVColor &color)
{
    // (frame_info=0xffffd4001490, x=751, y=1084, color=...)
    // YUV 绘制
    if (frame_info == nullptr)
    {
        return;
    }

    int _x = 0;
    int _y = 0;

    _x = x < 0 ? 0 : x;

    _y = y < 0 ? 0 : y;

    _x = _x >= frame_info->stVFrame.u32Width ? (frame_info->stVFrame.u32Width - 1) : _x;

    _y = _y >= frame_info->stVFrame.u32Height ? (frame_info->stVFrame.u32Height - 1) : _y;

    AX_VOID *pLumaVirAddr = (AX_VOID *)((AX_ULONG)frame_info->stVFrame.u64VirAddr[0]);
    AX_VOID *pChromaVirAddr = (AX_VOID *)((AX_ULONG)frame_info->stVFrame.u64VirAddr[1]);

    int yStride = frame_info->stVFrame.u32PicStride[0];

    uint8_t *yPt = static_cast<uint8_t *>(pLumaVirAddr);
    uint8_t *uvPt = static_cast<uint8_t *>(pChromaVirAddr);

    int y_offset = _y * yStride + _x;
    uint8_t *lpt = yPt + y_offset;
    *lpt = color.y; // y

    int uv_offset = (_x / 2 * 2) + ((_y / 2) * yStride);
    uvPt += uv_offset;
    uvPt[0] = color.u; // u
    uvPt[1] = color.v; // v
}

void DrawText(AX_VIDEO_FRAME_INFO_T *frame_info, int x, int y, const std::string &text, const YUVColor &color)
{
    RenderText(frame_info, x, y, text, &color);
}

int DrawLine(AX_VIDEO_FRAME_INFO_T *frame_info, int stx, int sty, int edx, int edy, const YUVColor &color, int lineWidth)
{

    if (frame_info->stVFrame.u64VirAddr[0] == 0)
    {
        return -1;
    }

    if (lineWidth == 0)
        lineWidth = 1;

    int width = frame_info->stVFrame.u32Width;
    int height = frame_info->stVFrame.u32Height;

    for (int i = 0; i < lineWidth; i++)
    {

        uint32_t x0 = stx, y0 = sty;
        uint32_t x1 = edx, y1 = edy;

        x0 = (x0 >= width) ? (x0 - lineWidth) : x0;
        x1 = (x1 >= width) ? (x1 - lineWidth) : x1;
        y0 = (y0 >= height) ? (y0 - lineWidth) : y0;
        y1 = (y1 >= height) ? (y1 - lineWidth) : y1;

        int dx = (x0 > x1) ? (x0 - x1) : (x1 - x0);
        int dy = (y0 > y1) ? (y0 - y1) : (y1 - y0);

        if (dx <= dy)
        {
            x0 += i;
            x1 += i;
        }
        else
        {
            y0 += i;
            y1 += i;
        }

        int xstep = (x0 < x1) ? 1 : -1;
        int ystep = (y0 < y1) ? 1 : -1;
        int nstep = 0, eps = 0;

        // 布雷森汉姆算法画线
        if (dx > dy)
        {
            while (nstep <= dx)
            {
                SetPixel(frame_info, x0, y0, color);
                eps += dy;
                if ((eps << 1) >= dx)
                {
                    y0 += ystep;
                    eps -= dx;
                }
                x0 += xstep;
                nstep++;
            }
        }
        else
        {
            while (nstep <= dy)
            {
                SetPixel(frame_info, x0, y0, color);
                eps += dx;
                if ((eps << 1) >= dy)
                {
                    x0 += xstep;
                    eps -= dy;
                }
                y0 += ystep;
                nstep++;
            }
        }
    }
    return 0;
}

// polylines

void DrawClosedLines(AX_VIDEO_FRAME_INFO_T *frame_info, std::vector<std::tuple<int, int>> points, const YUVColor &color, int lineWidth)
{

    std::tuple<int, int> st;
    std::tuple<int, int> en;
    for (size_t i = 0; i < points.size(); i++)
    {
        st = points[i];
        if (i == points.size() - 1)
        {
            en = points[0];
        }
        else
        {
            en = points[i + 1];
        }

        DrawLine(frame_info, std::get<0>(st), std::get<1>(st), std::get<0>(en), std::get<1>(en), color, lineWidth);
    }
}

void DrawRect(AX_VIDEO_FRAME_INFO_T *frame_info, int x1, int y1, int x2, int y2, const YUVColor &color, int lineWidth)
{

    if (x1 > x2)
    {
        std::swap(x1, x2);
    }

    if (y1 > y2)
    {
        std::swap(y1, y2);
    }

    int i, j;
    int iBound, jBound;
    int iStart, jStart;
    int width = (int)frame_info->stVFrame.u32Width;
    int height = (int)frame_info->stVFrame.u32Height;

    jBound = std::min(height, y1 + lineWidth);
    iBound = std::min(width - 1, x2);

    for (j = y1; j < jBound; ++j)
    {
        for (i = x1; i <= iBound; ++i)
        {
            SetPixel(frame_info, i, j, color);
        }
    }

    jStart = std::max(0, y2 - lineWidth + 1);
    jBound = std::min(height - 1, y2);

    iStart = std::max(0, x1);
    iBound = std::min(width - 1, x2);

    for (j = jStart; j <= jBound; ++j)
    {
        for (i = iStart; i <= iBound; ++i)
        {
            SetPixel(frame_info, i, j, color);
        }
    }

    iBound = std::min(width, x1 + lineWidth);
    jBound = std::min(height - 1, y2);

    for (i = x1; i < iBound; ++i)
    {
        for (j = y1; j <= jBound; ++j)
        {
            SetPixel(frame_info, i, j, color);
        }
    }

    iStart = std::max(0, (x2 - lineWidth + 1));
    iBound = std::min(width - 1, x2);

    jStart = std::max(0, y1);
    jBound = std::min(height - 1, y2);

    for (i = iStart; i <= iBound; ++i)
    {
        for (j = jStart; j <= jBound; ++j)
        {
            SetPixel(frame_info, i, j, color);
        }
    }
}

void draw_horiz_line(AX_VIDEO_FRAME_INFO_T *frame_info, uint32_t x1, uint32_t x2, uint32_t y, YUVColor color)
{
    for (uint32_t x = x1; x <= x2; ++x)
        SetPixel(frame_info, x, y, color);
}

void DrawCircle(AX_VIDEO_FRAME_INFO_T *frame_info, int32_t xCenter, int32_t yCenter, int32_t radius, YUVColor color)
{

    if (xCenter < 0 || xCenter >= frame_info->stVFrame.u32Width)
    {
        return;
    }
    if (yCenter < 0 || yCenter >= frame_info->stVFrame.u32Height)
    {
        return;
    }
    if (radius <= 0 || radius >= 30)
    {
        return;
    }

    int32_t r2 = radius * radius;

    for (int y = -radius; y <= radius; y++)
    {
        int32_t x = (int)(sqrt(r2 - y * y) + 0.5);
        draw_horiz_line(frame_info, xCenter - x, xCenter + x, yCenter - y, color);
    }
}

#pragma GCC pop_options