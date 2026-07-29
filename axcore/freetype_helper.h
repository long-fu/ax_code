
#pragma once

#include <unistd.h>
#include <string>
#include <memory>
#include "drawing.h"

// void SetPixel(AX_VIDEO_FRAME_INFO_T *image, int x, int y, const YUVColor &color);


void RenderText(AX_VIDEO_FRAME_INFO_T *image, int x, int y, const std::string &text, const YUVColor *color);
