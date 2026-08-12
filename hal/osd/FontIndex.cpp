#include "FontIndex.h"
#include "FontEn16.h"
#include "FontZh16.h"

namespace {

constexpr AX_U16 kEnGlyphCount =
    static_cast<AX_U16>(sizeof(g_fontEn16Index) / sizeof(g_fontEn16Index[0]));
constexpr AX_U16 kZhGlyphCount =
    static_cast<AX_U16>(sizeof(g_fontZh16Index) / sizeof(g_fontZh16Index[0]));

// Returns glyph index, or kEnGlyphCount / kZhGlyphCount on miss.
AX_U16 GetZhGlyphIndex(AX_U16 nUnicode) {
  for (AX_U16 i = 0; i < kZhGlyphCount; ++i) {
    if (g_fontZh16Index[i].nUnicode == nUnicode) {
      return i;
    }
    if (g_fontZh16Index[i].nUnicode > nUnicode) {
      break;
    }
  }
  return kZhGlyphCount;
}

}  // namespace

AX_S32 GetFontBitmap(AX_U16 nUnicode, FONT_BITMAP_T& bmp) {
  bmp.nWidth = 0;
  bmp.nHeight = 0;
  bmp.pBuffer = nullptr;

  if (nUnicode <= 0x7F) {
    // FontEn16 index is identity: unicode == glyph id for 0x00..0x7F.
    if (nUnicode >= kEnGlyphCount) {
      return -1;
    }
    bmp.nWidth = 8;
    bmp.nHeight = 16;
    bmp.pBuffer = g_fontEn16Glyphs + (bmp.nWidth / 8 * bmp.nHeight) * nUnicode;
    return 0;
  }

  AX_U16 nInd = GetZhGlyphIndex(nUnicode);
  if (nInd >= kZhGlyphCount) {
    return -1;
  }
  bmp.nWidth = 16;
  bmp.nHeight = 16;
  bmp.pBuffer = g_fontZh16Glyphs + (bmp.nWidth / 8 * bmp.nHeight) * nInd;
  return 0;
}
