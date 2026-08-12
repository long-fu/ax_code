#pragma once

#include <string>

#include "drawing.h"

// Bitmap-font OSD text (hal/osd FontEn16 / FontZh16).
// `y` is the text baseline (matches FreeType RenderText / DrawText).
// `font_size` maps to integer scale: nScale = (font_size + 15) / 16 (min 1).
void RenderOsdText(AX_VIDEO_FRAME_INFO_T* frame, int x, int y,
                   const std::string& text, const YUVColor& color,
                   int font_size = 16);
