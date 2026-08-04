#pragma once

#include <string>
#include "drawing.h"

void RenderText(AX_VIDEO_FRAME_INFO_T* image, int x, int y,
                const std::string& text, const YUVColor* color);
