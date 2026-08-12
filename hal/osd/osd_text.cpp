#include "osd_text.h"

#include "FontIndex.h"

namespace {

constexpr int kBaseCell = 16;  // FontEn16 / FontZh16 design size
constexpr int kEnCellW = 8;
constexpr int kZhCellW = 16;

int ScaleFromFontSize(int font_size) {
  if (font_size <= 0) font_size = kBaseCell;
  int scale = (font_size + kBaseCell - 1) / kBaseCell;  // (uFontSize+15)/16
  return scale < 1 ? 1 : scale;
}

int FallbackAdvance(AX_U16 unicode, int scale) {
  return ((unicode > 0x7F) ? kZhCellW : kEnCellW) * scale;
}

// Decode one UTF-8 codepoint; returns bytes consumed (0 on truncated input).
// Sets *out to 0 for unsupported supplementary-plane sequences (still consumes 4).
size_t Utf8Next(const std::string& s, size_t i, AX_U16* out) {
  if (i >= s.size()) return 0;
  unsigned char c = static_cast<unsigned char>(s[i]);
  if (c < 0x80) {
    *out = c;
    return 1;
  }
  if (c < 0xE0) {
    if (i + 1 >= s.size()) return 0;
    *out = static_cast<AX_U16>(((c & 0x1Fu) << 6) |
                               (static_cast<unsigned char>(s[i + 1]) & 0x3Fu));
    return 2;
  }
  if (c < 0xF0) {
    if (i + 2 >= s.size()) return 0;
    *out = static_cast<AX_U16>(
        ((c & 0x0Fu) << 12) |
        ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
        (static_cast<unsigned char>(s[i + 2]) & 0x3Fu));
    return 3;
  }
  // Outside BMP / unsupported for 16-bit font index — skip 4-byte seq.
  if (i + 3 >= s.size()) return 0;
  *out = 0;
  return 4;
}

void BlitBitmapScaled(AX_VIDEO_FRAME_INFO_T* frame, int x, int y,
                      const FONT_BITMAP_T& bmp, const YUVColor& color,
                      int scale) {
  if (!bmp.pBuffer || bmp.nWidth == 0 || bmp.nHeight == 0 || scale < 1) return;

  const int row_bytes = bmp.nWidth / 8;
  for (AX_U16 row = 0; row < bmp.nHeight; ++row) {
    const AX_U8* src = bmp.pBuffer + row * row_bytes;
    for (AX_U16 col = 0; col < bmp.nWidth; ++col) {
      if ((src[col / 8] & (0x80u >> (col & 7))) == 0) continue;
      for (int hy = 0; hy < scale; ++hy) {
        for (int wx = 0; wx < scale; ++wx) {
          SetPixel(frame, x + col * scale + wx, y + row * scale + hy, color);
        }
      }
    }
  }
}

}  // namespace

void RenderOsdText(AX_VIDEO_FRAME_INFO_T* frame, int x, int y,
                   const std::string& text, const YUVColor& color,
                   int font_size) {
  if (!frame) return;

  const int scale = ScaleFromFontSize(font_size);

  // `y` is the text baseline (same convention as FreeType RenderText).
  int cursor_x = x;
  for (size_t i = 0; i < text.size();) {
    AX_U16 unicode = 0;
    size_t n = Utf8Next(text, i, &unicode);
    if (n == 0) break;
    i += n;

    if (unicode == 0) {
      cursor_x += kEnCellW * scale;
      continue;
    }

    FONT_BITMAP_T bmp{};
    if (GetFontBitmap(unicode, bmp) != 0) {
      cursor_x += FallbackAdvance(unicode, scale);
      continue;
    }

    // Place scaled bitmap so its bottom edge sits on the baseline.
    BlitBitmapScaled(frame, cursor_x,
                     y - static_cast<int>(bmp.nHeight) * scale, bmp, color,
                     scale);
    cursor_x += static_cast<int>(bmp.nWidth) * scale;
  }
}
