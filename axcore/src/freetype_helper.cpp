#include <codecvt>
#include <exception>
#include <iostream>
#include <locale>
#include <mutex>
#include <vector>

#include "ax_global_type.h"
#include "freetype_helper.h"

extern "C"
{
#include <ft2build.h>
#include FT_FREETYPE_H
    const char *FreeTypeErrorMessage(FT_Error err)
    {
#undef FTERRORS_H_
#define FT_ERRORDEF(e, v, s) \
    case e:                  \
        return s;
#define FT_ERROR_START_LIST \
    switch (err)            \
    {
#define FT_ERROR_END_LIST }
#include FT_ERRORS_H
        return "(Unknown error)";
    }
}

#define CHECK_FREETYPE(x)                                            \
    {                                                                \
        auto error = x;                                              \
        if (error)                                                   \
        {                                                            \
            auto err_msg = FreeTypeErrorMessage(error);              \
            std::cerr << "FreeType Error: " << err_msg << std::endl; \
            throw std::runtime_error(err_msg);                       \
        }                                                            \
    }

class GlpyhContext
{
public:
    static GlpyhContext &GetInstance()
    {
        static thread_local GlpyhContext ctx;
        return ctx;
    }

    int DrawChar(char32_t ch, int x, int y, const YUVColor *color,
                 AX_VIDEO_FRAME_INFO_T *image)
    {
        FT_UInt glyph_idx = 0;
        FT_Face face;
        int fontIdx = 0;
        int fontListSize = faces.size();
        for (fontIdx = 0; fontIdx < fontListSize; ++fontIdx)
        {
            face = faces[fontIdx];
            glyph_idx = FT_Get_Char_Index(face, ch);
            CHECK_FREETYPE(FT_Load_Glyph(face, glyph_idx, FT_LOAD_RENDER));
            // if glyph_idx is 0 that means symbol not found,
            // try next font!
            if (glyph_idx > 0)
            {
                break;
            }
        }

        if (image == nullptr)
        {
            return -1;
        }

        if (x < 0 || x > image->stVFrame.u32Width || y < 0 || y > image->stVFrame.u32Height)
        {
            return -1;
        }

        CHECK_FREETYPE(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL));

        FT_GlyphSlot slot = face->glyph;

        int h = image->stVFrame.u32Height;
        int w = image->stVFrame.u32Width;

        int imageY = y - slot->bitmap_top + fontSize;

        for (int i = 0; i < slot->bitmap.rows && imageY < h; ++i, ++imageY)
        {
            int imageX = x + slot->bitmap_left;
            for (int j = 0; j < slot->bitmap.width && imageX < w; ++j, ++imageX)
            {
                auto bitmap_val = slot->bitmap.buffer[i * slot->bitmap.width + j];
                if (bitmap_val > 0)
                {
                    SetPixel(image, imageX, imageY, *color);
                }
            }
        }
        return slot->advance.x >> 6;
    }

private:
    GlpyhContext()
    {
        std::vector<const char *> fontList{
            "./resource/GB2312.ttf", // For CJK
            "./resource/arial.ttf",  // For latin and
            "./resource/simsun.ttc"  // digits
        };

        for (const char *fontPath : fontList)
        {
            FT_Face face;
            CHECK_FREETYPE(FT_Init_FreeType(&library));
            CHECK_FREETYPE(FT_New_Face(library, fontPath, 0, &face));

            CHECK_FREETYPE(FT_Set_Pixel_Sizes(face, fontSize, fontSize));
            faces.push_back(face);
        }
    }

    int fontSize{20};
    FT_Library library;
    std::vector<FT_Face> faces;
    std::mutex mtx;
};

#pragma GCC push_options
#pragma GCC optimize("O0")

// cache glyphs
void RenderText(AX_VIDEO_FRAME_INFO_T *image, int x, int y, const std::string &text, const YUVColor *color)
{
    auto &ctx = GlpyhContext::GetInstance();
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cvt;
    std::u32string utf32_str = cvt.from_bytes(text);
    for (auto u32char : utf32_str)
    {
        x += ctx.DrawChar(u32char, x, y, color, image);
    }
}

#pragma GCC pop_options