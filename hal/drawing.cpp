#include "drawing.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <stdint.h>
#include <opencv2/opencv.hpp>
#include "freetype_helper.h"

using namespace cv;

// ============================================================
//  Defensive validation
// ============================================================
namespace {

bool IsFrameValid(const AX_VIDEO_FRAME_INFO_T* f) {
    if (!f) return false;
    if (!f->stVFrame.u64VirAddr[0]) return false;
    if (f->stVFrame.u32Width == 0 || f->stVFrame.u32Height == 0) return false;
    if (f->stVFrame.u32PicStride[0] < f->stVFrame.u32Width) return false;
    return true;
}

// Debug-build assertion; release-build silent false
#define ASSERT_OR_RETURN(cond) \
    do { if (!(cond)) { assert(cond); return; } } while (0)
#define ASSERT_OR_RETURN_VAL(cond, val) \
    do { if (!(cond)) { assert(cond); return (val); } } while (0)

}

// ============================================================
//  Internal helpers
// ============================================================

namespace {

struct FrameBuf {
    uint8_t* y;
    uint8_t* uv;
    int stride;
    int width;
    int height;
};

FrameBuf GetFrameBuf(AX_VIDEO_FRAME_INFO_T* f) {
    return {
        static_cast<uint8_t*>((void*)f->stVFrame.u64VirAddr[0]),
        static_cast<uint8_t*>((void*)f->stVFrame.u64VirAddr[1]),
        static_cast<int>(f->stVFrame.u32PicStride[0]),
        static_cast<int>(f->stVFrame.u32Width),
        static_cast<int>(f->stVFrame.u32Height),
    };
}

// Pre-filled UV pattern for memcpy (128 bytes = 64 UV pairs)
inline const uint8_t* UvPattern128(const YUVColor& c) {
    alignas(64) static uint8_t kPat[128];
    static YUVColor kCached = {0, 0, 0};  // guard against first-use race (benign)
    if (kCached.y != c.y || kCached.u != c.u || kCached.v != c.v) {
        for (int i = 0; i < 128; i += 2) {
            kPat[i] = c.u;
            kPat[i + 1] = c.v;
        }
        kCached = c;
    }
    return kPat;
}

// Fast horizontal line — memset Y, memcpy UV
void DrawHorizLine(const FrameBuf& fb, int x1, int x2, int y, const YUVColor& c) {
    if (y < 0 || y >= fb.height) return;
    if (x1 > x2) std::swap(x1, x2);
    x1 = std::max(0, x1);
    x2 = std::min(fb.width - 1, x2);
    if (x1 > x2) return;

    // Y plane: one memset
    memset(fb.y + y * fb.stride + x1, c.y, x2 - x1 + 1);

    // UV plane: memcpy in 128-byte chunks, then tail loop
    int uvRow = (y / 2) * fb.stride;
    int uvStart = x1 & ~1;       // round down to even
    int uvBytes = x2 - uvStart + 2;
    uint8_t* dst = fb.uv + uvRow + uvStart;
    const uint8_t* pat = UvPattern128(c);

    while (uvBytes >= 128) {
        memcpy(dst, pat, 128);
        dst += 128;
        uvBytes -= 128;
    }
    while (uvBytes >= 2) {
        dst[0] = c.u;
        dst[1] = c.v;
        dst += 2;
        uvBytes -= 2;
    }
}

// Fast vertical line — stride loop for Y, stride loop for UV (skip even-row dupes)
void DrawVertLine(const FrameBuf& fb, int x, int y1, int y2, const YUVColor& c) {
    if (x < 0 || x >= fb.width) return;
    if (y1 > y2) std::swap(y1, y2);
    y1 = std::max(0, y1);
    y2 = std::min(fb.height - 1, y2);
    if (y1 > y2) return;

    int uvX = x & ~1;

    // Y: one write per row
    for (int y = y1; y <= y2; y++) {
        fb.y[y * fb.stride + x] = c.y;
    }

    // UV: each pair covers 2 Y rows — write only on odd y (or y1 if range < 2)
    int uvY1 = y1 | 1;  // round up to next odd
    if (uvY1 > y2) uvY1 = y1;
    for (int y = uvY1; y <= y2; y += 2) {
        int off = (y / 2) * fb.stride + uvX;
        fb.uv[off] = c.u;
        fb.uv[off + 1] = c.v;
    }
}

// Filled horizontal band: lineWidth rows, each using memset
void DrawHorizBand(const FrameBuf& fb, int x1, int x2, int y, int lineWidth,
                   const YUVColor& c) {
    for (int i = 0; i < lineWidth; i++) {
        DrawHorizLine(fb, x1, x2, y + i, c);
    }
}

// Filled vertical band: lineWidth columns, each using stride loop
void DrawVertBand(const FrameBuf& fb, int x, int y1, int y2, int lineWidth,
                  const YUVColor& c) {
    for (int i = 0; i < lineWidth; i++) {
        DrawVertLine(fb, x + i, y1, y2, c);
    }
}

}  // namespace

// ============================================================
//  Public API
// ============================================================

void SetPixel(AX_VIDEO_FRAME_INFO_T* frame, int x, int y, const YUVColor& color) {
    ASSERT_OR_RETURN(IsFrameValid(frame));

    int w = static_cast<int>(frame->stVFrame.u32Width);
    int h = static_cast<int>(frame->stVFrame.u32Height);
    if (x < 0 || x >= w || y < 0 || y >= h) return;

    auto fb = GetFrameBuf(frame);

    fb.y[y * fb.stride + x] = color.y;

    int uvOff = (y / 2) * fb.stride + (x & ~1);
    fb.uv[uvOff] = color.u;
    fb.uv[uvOff + 1] = color.v;
}

void DrawText(AX_VIDEO_FRAME_INFO_T* frame, int x, int y,
              const std::string& text, const YUVColor& color) {
    RenderText(frame, x, y, text, &color);
}

int DrawLine(AX_VIDEO_FRAME_INFO_T* frame, int x1, int y1, int x2, int y2,
             const YUVColor& color, int lineWidth) {
    ASSERT_OR_RETURN_VAL(IsFrameValid(frame), -1);
    if (lineWidth < 1) lineWidth = 1;

    int w = static_cast<int>(frame->stVFrame.u32Width);
    int h = static_cast<int>(frame->stVFrame.u32Height);

    // Special case: horizontal line → memset
    if (y1 == y2) {
        // Clamp to bounds
        if (y1 < 0 || y1 >= h) return 0;
        if (x1 > x2) std::swap(x1, x2);
        // Draw band for thickness
        for (int i = 0; i < lineWidth; i++) {
            int yy = y1 + i - lineWidth / 2;
            if (yy >= 0 && yy < h) {
                int sx = std::max(0, x1);
                int ex = std::min(w - 1, x2);
                if (sx <= ex) {
                    auto fb = GetFrameBuf(frame);
                    DrawHorizLine(fb, sx, ex, yy, color);
                }
            }
        }
        return 0;
    }

    // Special case: vertical line → stride loop
    if (x1 == x2) {
        if (x1 < 0 || x1 >= w) return 0;
        if (y1 > y2) std::swap(y1, y2);
        for (int i = 0; i < lineWidth; i++) {
            int xx = x1 + i - lineWidth / 2;
            if (xx >= 0 && xx < w) {
                int sy = std::max(0, y1);
                int ey = std::min(h - 1, y2);
                if (sy <= ey) {
                    auto fb = GetFrameBuf(frame);
                    DrawVertLine(fb, xx, sy, ey, color);
                }
            }
        }
        return 0;
    }

    // General case: Bresenham with centered thickness
    int halfW = lineWidth / 2;
    for (int i = 0; i < lineWidth; i++) {
        int off = i - halfW;
        int x0 = x1, y0 = y1, x1o = x2, y1o = y2;

        int dx = std::abs(x2 - x1);
        int dy = std::abs(y2 - y1);

        if (dx > dy) {
            y0 += off;
            y1o += off;
        } else {
            x0 += off;
            x1o += off;
        }

        int xstep = (x0 < x1o) ? 1 : -1;
        int ystep = (y0 < y1o) ? 1 : -1;
        int nstep = 0, eps = 0;

        if (dx > dy) {
            while (nstep <= dx) {
                SetPixel(frame, x0, y0, color);
                eps += dy;
                if ((eps << 1) >= dx) { y0 += ystep; eps -= dx; }
                x0 += xstep;
                nstep++;
            }
        } else {
            while (nstep <= dy) {
                SetPixel(frame, x0, y0, color);
                eps += dx;
                if ((eps << 1) >= dy) { x0 += xstep; eps -= dy; }
                y0 += ystep;
                nstep++;
            }
        }
    }
    return 0;
}

void DrawClosedLines(AX_VIDEO_FRAME_INFO_T* frame,
                     std::vector<std::tuple<int, int>> points,
                     const YUVColor& color, int lineWidth) {
    if (points.size() < 2) return;
    for (size_t i = 0; i < points.size(); i++) {
        auto [sx, sy] = points[i];
        auto [ex, ey] = points[(i + 1) % points.size()];
        DrawLine(frame, sx, sy, ex, ey, color, lineWidth);
    }
}

void DrawRect(AX_VIDEO_FRAME_INFO_T* frame, int x1, int y1, int x2, int y2,
              const YUVColor& color, int lineWidth) {
    ASSERT_OR_RETURN(IsFrameValid(frame));
    if (lineWidth < 1) lineWidth = 1;

    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    auto fb = GetFrameBuf(frame);

    // Top / bottom edges → horizontal bands
    DrawHorizBand(fb, x1, x2, y1, lineWidth, color);
    DrawHorizBand(fb, x1, x2, y2 - lineWidth + 1, lineWidth, color);

    // Left / right edges → vertical bands (skip overlap with top/bottom)
    DrawVertBand(fb, x1, y1 + lineWidth, y2 - lineWidth, lineWidth, color);
    DrawVertBand(fb, x2 - lineWidth + 1, y1 + lineWidth, y2 - lineWidth,
                 lineWidth, color);
}

void DrawCircle(AX_VIDEO_FRAME_INFO_T* frame, int cx, int cy, int radius,
                YUVColor color) {
    ASSERT_OR_RETURN(IsFrameValid(frame));
    int w = static_cast<int>(frame->stVFrame.u32Width);
    int h = static_cast<int>(frame->stVFrame.u32Height);
    if (cx < 0 || cx >= w || cy < 0 || cy >= h || radius <= 0) return;

    auto fb = GetFrameBuf(frame);

    // Bresenham midpoint circle — fills with horizontal lines
    int x = radius, y = 0, err = 1 - radius;
    while (x >= y) {
        DrawHorizLine(fb, cx - x, cx + x, cy - y, color);
        DrawHorizLine(fb, cx - x, cx + x, cy + y, color);
        DrawHorizLine(fb, cx - y, cx + y, cy - x, color);
        DrawHorizLine(fb, cx - y, cx + y, cy + x, color);
        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}
