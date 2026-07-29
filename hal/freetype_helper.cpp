#include "freetype_helper.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "ax_global_type.h"

extern "C" {
#include <ft2build.h>
#include FT_FREETYPE_H
}

// ============================================================
//  UTF-8 → char32_t（替代 deprecated std::wstring_convert）
// ============================================================
namespace {

std::u32string Utf8ToU32(const std::string& s) {
    std::u32string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        char32_t cp;
        unsigned char c = s[i];
        if (c < 0x80) {
            cp = c; i += 1;
        } else if (c < 0xE0) {
            cp = ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); i += 2;
        } else if (c < 0xF0) {
            cp = ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) |
                 (s[i + 2] & 0x3Fu);
            i += 3;
        } else {
            cp = ((c & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12) |
                 ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu);
            i += 4;
        }
        out.push_back(cp);
    }
    return out;
}

// ============================================================
//  字形缓存
// ============================================================
struct CachedGlyph {
    std::vector<uint8_t> bitmap;  // 8-bit grayscale
    int width = 0;
    int rows = 0;
    int advance = 0;   // 光标步进 (1/64 px → >>6 = pixels)
    int left = 0;      // bitmap_left
    int top = 0;       // bitmap_top
};

// ============================================================
//  上下文（thread_local：每线程独立 FreeType + 缓存）
// ============================================================
class GlyphContext {
 public:
    static GlyphContext& Get() {
        thread_local GlyphContext ctx;
        return ctx;
    }

    int DrawChar(char32_t ch, int x, int y, const YUVColor* color,
                 AX_VIDEO_FRAME_INFO_T* image) {
        if (!image || !image->stVFrame.u64VirAddr[0]) return 0;

        auto it = cache_.find(ch);
        if (it == cache_.end()) {
            CachedGlyph g;
            if (!RenderGlyph(ch, g)) return 0;
            it = cache_.insert({ch, std::move(g)}).first;
        }
        const CachedGlyph& g = it->second;
        if (g.bitmap.empty()) return g.advance;

        BlitGlyph(image, x, y - g.top, g, color);
        return g.advance;
    }

 private:
    GlyphContext() {
        static const char* kFonts[] = {
            "./resource/GB2312.ttf",
            "./resource/arial.ttf",
            "./resource/simsun.ttc",
        };
        if (FT_Init_FreeType(&library_)) return;
        for (auto* path : kFonts) {
            FT_Face face = nullptr;
            if (FT_New_Face(library_, path, 0, &face) == 0) {
                FT_Set_Pixel_Sizes(face, 0, kFontSize);
                faces_.push_back(face);
            }
        }
        last_idx_ = faces_.empty() ? -1 : 0;
    }

    ~GlyphContext() {
        for (auto* f : faces_) FT_Done_Face(f);
        if (library_) FT_Done_FreeType(library_);
    }

    // 遍历字体查找字形 → Load + Render → 缓存
    bool RenderGlyph(char32_t ch, CachedGlyph& out) {
        if (faces_.empty()) return false;

        // 先试上次命中的字体
        FT_Face face = nullptr;
        FT_UInt idx = 0;

        if (last_idx_ >= 0 && last_idx_ < (int)faces_.size()) {
            idx = FT_Get_Char_Index(faces_[last_idx_], ch);
            if (idx) face = faces_[last_idx_];
        }

        // 不命中则遍历其余字体
        if (!idx) {
            for (int i = 0; i < (int)faces_.size(); i++) {
                if (i == last_idx_) continue;
                idx = FT_Get_Char_Index(faces_[i], ch);
                if (idx) {
                    face = faces_[i];
                    last_idx_ = i;
                    break;
                }
            }
        }

        if (!idx || !face) {
            // 字形不存在 — 返回零宽占位
            out.bitmap.clear();
            out.width = 0;
            out.rows = 0;
            out.advance = kFontSize / 2;
            out.left = 0;
            out.top = kFontSize;
            return false;  // 标记不可用但 advance 有效
        }

        // Load + Render
        if (FT_Load_Glyph(face, idx, FT_LOAD_DEFAULT)) return false;
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) return false;

        auto* slot = face->glyph;
        out.width = slot->bitmap.width;
        out.rows = slot->bitmap.rows;
        out.advance = slot->advance.x >> 6;
        out.left = slot->bitmap_left;
        out.top = slot->bitmap_top;

        if (slot->bitmap.buffer && out.width > 0 && out.rows > 0) {
            int size = out.width * out.rows;
            out.bitmap.assign(slot->bitmap.buffer,
                              slot->bitmap.buffer + size);
        }
        return true;
    }

    // 批量写入字形位图到 YUV 帧（跳过透明像素）
    void BlitGlyph(AX_VIDEO_FRAME_INFO_T* image, int dx, int dy,
                   const CachedGlyph& g, const YUVColor* c) {
        int w = static_cast<int>(image->stVFrame.u32Width);
        int h = static_cast<int>(image->stVFrame.u32Height);
        int stride = static_cast<int>(image->stVFrame.u32PicStride[0]);
        auto* yBase =
            static_cast<uint8_t*>((void*)image->stVFrame.u64VirAddr[0]);
        auto* uvBase =
            static_cast<uint8_t*>((void*)image->stVFrame.u64VirAddr[1]);

        for (int row = 0; row < g.rows; row++) {
            int imgY = dy + row;
            if (imgY < 0 || imgY >= h) continue;

            int yRowOff = imgY * stride;
            int uvRowOff = (imgY / 2) * stride;
            const uint8_t* src = g.bitmap.data() + row * g.width;

            for (int col = 0; col < g.width; col++) {
                if (!src[col]) continue;  // 透明

                int imgX = dx + col;
                if (imgX < 0 || imgX >= w) continue;

                // Y
                yBase[yRowOff + imgX] = c->y;

                // UV（每 2 像素共享一对）
                int uvOff = uvRowOff + (imgX & ~1);
                uvBase[uvOff] = c->u;
                uvBase[uvOff + 1] = c->v;
            }
        }
    }

    static constexpr int kFontSize = 20;

    FT_Library library_ = nullptr;
    std::vector<FT_Face> faces_;
    std::unordered_map<char32_t, CachedGlyph> cache_;
    int last_idx_ = -1;
};

}  // namespace

// ============================================================
//  公开接口
// ============================================================

void RenderText(AX_VIDEO_FRAME_INFO_T* image, int x, int y,
                const std::string& text, const YUVColor* color) {
    auto& ctx = GlyphContext::Get();
    std::u32string u32 = Utf8ToU32(text);
    for (char32_t ch : u32) {
        x += ctx.DrawChar(ch, x, y, color, image);
    }
}
