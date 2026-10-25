#ifndef J2ME_HOST_FONT_H
#define J2ME_HOST_FONT_H

// Host-menu rasterizer only. Font bytes must be trusted OS fonts (or trusted
// local test fixtures) and remain alive until this Font is destroyed. stb is
// not a hardened parser for downloaded/user-supplied font files.
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "third_party/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef STB_TRUETYPE_IMPLEMENTATION
#undef STBTT_STATIC

namespace host_font {
class Font {
public:
    static constexpr size_t max_cache_bytes = 4 * 1024 * 1024;
    static constexpr size_t max_cache_glyphs = 1024;

    bool add_font(const void* bytes, size_t length) {
        const auto* data = static_cast<const unsigned char*>(bytes);
        // Reject short headers and out-of-range collection/table offsets before
        // invoking stb. These checks do not make arbitrary font input safe.
        if (!data || length < 12 || length > static_cast<size_t>(INT_MAX)) return false;
        size_t offset = 0;
        if (be32(data) == 0x74746366u) { // ttcf: use the first face
            if (length < 16 || be32(data + 8) == 0) return false;
            const size_t faces = be32(data + 8);
            if (faces > (length - 12) / 4) return false;
            offset = be32(data + 12);
        }
        if (offset > length - 12) return false;
        const uint32_t magic = be32(data + offset);
        if (magic != 0x00010000u && magic != 0x4f54544fu &&
            magic != 0x74727565u && magic != 0x74797031u) return false;
        const size_t tables = (size_t(data[offset + 4]) << 8) | data[offset + 5];
        if (!tables || tables > (length - offset - 12) / 16) return false;
        for (size_t n = 0; n < tables; ++n) {
            const auto* table = data + offset + 12 + n * 16;
            const size_t start = be32(table + 8), size = be32(table + 12);
            if (start > length || size > length - start) return false;
        }
        Face face{};
        if (!stbtt_InitFont(&face.info, data, static_cast<int>(offset))) return false;
        stbtt_GetFontVMetrics(&face.info, &face.ascent, &face.descent, &face.linegap);
        if (face.ascent <= face.descent) return false;
        faces_.push_back(face);
        cache_.clear();
        cache_bytes_ = 0;
        return true;
    }
    bool ready() const { return !faces_.empty(); }
    size_t cache_bytes() const { return cache_bytes_; }
    size_t cache_glyphs() const { return cache_.size(); }

    // Ink of draw(0, 0, ...). x/y is the top-left of nonzero pixels.
    struct Measure { bool ok = false; int x = 0, y = 0, w = 0, h = 0, advance = 0; };
    Measure measure(const std::string& utf8, int height) {
        Measure m;
        if (!ready() || height <= 0) return m;
        const Layout layout = make_layout(utf8, height);
        const float scale = stbtt_ScaleForMappingEmToPixels(&faces_[0].info, float(height));
        const int baseline = int(std::lround(faces_[0].ascent * double(scale)));
        m.ok = true;
        m.advance = int(std::lround(layout.advance));
        if (!layout.ink) {
            m.w = std::max(0, m.advance);
            m.h = height;
            return m;
        }
        m.x = int(layout.left);
        m.y = baseline + int(layout.top);
        m.w = int(layout.right - layout.left) + 1;
        m.h = int(layout.bottom - layout.top) + 1;
        if (m.w < 1) m.w = 1;
        if (m.h < 1) m.h = 1;
        if (m.advance < m.x + m.w) m.advance = m.x + m.w;
        return m;
    }

    // pitch is measured in uint32_t pixels. y is the top of the font's
    // ascent/descent line box; height is em size, not a glyph's ink height.
    bool draw(uint32_t* frame, size_t pitch, int x, int y,
              const std::string& utf8, uint32_t rgba, int height, int maxwidth,
              int screenw = 1280, int screenh = 720) {
        if (!ready()) return false;
        if (!valid_target(frame, pitch, height, maxwidth, screenw, screenh)) return true;
        const Layout layout = make_layout(utf8, height);
        const float scale = stbtt_ScaleForMappingEmToPixels(&faces_[0].info, float(height));
        const int64_t baseline = int64_t(y) + std::lround(faces_[0].ascent * scale);
        paint(frame, pitch, layout, x, baseline, rgba, x, int64_t(x) + maxwidth,
              screenw, screenh);
        return true;
    }

    // Center the union of nonzero bitmap pixels, rather than advance widths
    // or the font line box, so A/X/B/L/R sit in the middle of controller disks.
    bool draw_centered(uint32_t* frame, size_t pitch, int center_x, int center_y,
                       const std::string& utf8, uint32_t rgba, int height,
                       int maxwidth, int screenw = 1280, int screenh = 720) {
        if (!ready()) return false;
        if (!valid_target(frame, pitch, height, maxwidth, screenw, screenh)) return true;
        const Layout layout = make_layout(utf8, height);
        if (!layout.ink) return true;
        const int64_t x = std::llround(double(center_x) -
                                      (double(layout.left) + layout.right) / 2.0);
        const int64_t baseline = std::llround(double(center_y) -
                                            (double(layout.top) + layout.bottom) / 2.0);
        const int64_t clip_left = int64_t(center_x) - maxwidth / 2;
        paint(frame, pitch, layout, x, baseline, rgba, clip_left,
              clip_left + maxwidth, screenw, screenh);
        return true;
    }

private:
    struct Face { stbtt_fontinfo info{}; int ascent = 0, descent = 0, linegap = 0; };
    struct Key {
        size_t face; int glyph, height;
        bool operator<(const Key& b) const {
            if (face != b.face) return face < b.face;
            if (glyph != b.glyph) return glyph < b.glyph;
            return height < b.height;
        }
    };
    struct Glyph {
        int width = 0, height = 0, x = 0, y = 0;
        int left = 0, top = 0, right = -1, bottom = -1;
        float advance = 0;
        std::vector<unsigned char> pixels;
    };
    struct Placement { int64_t x; std::shared_ptr<Glyph> glyph; };
    struct Layout {
        std::vector<Placement> glyphs;
        bool ink = false;
        int64_t left = 0, top = 0, right = 0, bottom = 0;
        double advance = 0;
    };
    std::vector<Face> faces_;
    std::map<Key, std::shared_ptr<Glyph>> cache_;
    size_t cache_bytes_ = 0;

    static uint32_t be32(const unsigned char* p) {
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
    }
    static bool valid_target(uint32_t* frame, size_t pitch, int height,
                             int maxwidth, int screenw, int screenh) {
        return frame && height > 0 && height <= 256 && maxwidth > 0 &&
               screenw > 0 && screenh > 0 && pitch >= size_t(screenw) &&
               pitch <= std::numeric_limits<size_t>::max() / size_t(screenh);
    }
    static int next_codepoint(const std::string& s, size_t& i) {
        const auto first = static_cast<unsigned char>(s[i++]);
        if (first < 0x80) return first;
        unsigned count = 0; uint32_t cp = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { count = 1; cp = first & 31; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { count = 2; cp = first & 15; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { count = 3; cp = first & 7; minimum = 0x10000; }
        else return 0xfffd;
        if (count > s.size() - i) return 0xfffd;
        for (unsigned n = 0; n < count; ++n) {
            const auto c = static_cast<unsigned char>(s[i + n]);
            if ((c & 0xc0) != 0x80) return 0xfffd;
            cp = (cp << 6) | (c & 63);
        }
        i += count;
        return cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)
                   ? 0xfffd : static_cast<int>(cp);
    }
    Key select(int cp, int height) const {
        for (size_t f = 0; f < faces_.size(); ++f) {
            const int glyph = stbtt_FindGlyphIndex(&faces_[f].info, cp);
            if (glyph) return {f, glyph, height};
        }
        int glyph = stbtt_FindGlyphIndex(&faces_[0].info, 0xfffd);
        if (!glyph) glyph = stbtt_FindGlyphIndex(&faces_[0].info, '?');
        return {0, glyph, height};
    }
    std::shared_ptr<Glyph> bitmap(const Key& key) {
        const auto found = cache_.find(key);
        if (found != cache_.end()) return found->second;
        auto g = std::make_shared<Glyph>();
        const auto* info = &faces_[key.face].info;
        const float scale = stbtt_ScaleForMappingEmToPixels(info, float(key.height));
        int advance = 0, bearing = 0;
        stbtt_GetGlyphHMetrics(info, key.glyph, &advance, &bearing);
        g->advance = advance * scale;
        int right = 0, bottom = 0;
        stbtt_GetGlyphBitmapBox(info, key.glyph, scale, scale, &g->x, &g->y, &right, &bottom);
        g->width = right - g->x; g->height = bottom - g->y;
        if (g->width > 0 && g->height > 0 &&
            size_t(g->width) <= max_cache_bytes / size_t(g->height)) {
            g->pixels.resize(size_t(g->width) * g->height);
            stbtt_MakeGlyphBitmap(info, g->pixels.data(), g->width, g->height,
                                 g->width, scale, scale, key.glyph);
            bool ink = false;
            for (int y = 0; y < g->height; ++y) for (int x = 0; x < g->width; ++x) {
                if (!g->pixels[size_t(y) * g->width + x]) continue;
                if (!ink) { g->left = g->right = x; g->top = g->bottom = y; ink = true; }
                else { g->left = std::min(g->left, x); g->right = std::max(g->right, x);
                       g->top = std::min(g->top, y); g->bottom = std::max(g->bottom, y); }
            }
        } else { g->width = g->height = 0; }
        while (!cache_.empty() && (cache_.size() >= max_cache_glyphs ||
               cache_bytes_ + g->pixels.size() > max_cache_bytes)) {
            cache_bytes_ -= cache_.begin()->second->pixels.size();
            cache_.erase(cache_.begin());
        }
        cache_bytes_ += g->pixels.size();
        cache_.emplace(key, g);
        return g;
    }
    Layout make_layout(const std::string& s, int height) {
        Layout layout;
        double pen = 0;
        Key previous{0, 0, height}; bool have_previous = false;
        for (size_t i = 0; i < s.size();) {
            const int cp = next_codepoint(s, i);
            if (cp == '\n' || cp == '\r') break; // menu labels are single-line
            const Key key = select(cp == '\t' ? ' ' : cp, height);
            if (have_previous && previous.face == key.face) {
                const auto* info = &faces_[key.face].info;
                pen += stbtt_GetGlyphKernAdvance(info, previous.glyph, key.glyph) *
                       stbtt_ScaleForMappingEmToPixels(info, float(height));
            }
            auto g = bitmap(key);
            const int64_t gx = std::llround(pen);
            layout.glyphs.push_back({gx, g});
            if (g->right >= g->left && g->bottom >= g->top) {
                const int64_t left = gx + g->x + g->left, right = gx + g->x + g->right;
                const int64_t top = g->y + g->top, bottom = g->y + g->bottom;
                if (!layout.ink) { layout.left = left; layout.right = right;
                                  layout.top = top; layout.bottom = bottom; layout.ink = true; }
                else { layout.left = std::min(layout.left, left); layout.right = std::max(layout.right, right);
                       layout.top = std::min(layout.top, top); layout.bottom = std::max(layout.bottom, bottom); }
            }
            pen += g->advance;
            previous = key; have_previous = true;
        }
        layout.advance = pen;
        return layout;
    }
    static void paint(uint32_t* frame, size_t pitch, const Layout& layout,
                      int64_t x, int64_t baseline, uint32_t rgba,
                      int64_t clip_left, int64_t clip_right, int screenw, int screenh) {
        clip_left = std::max<int64_t>(0, clip_left);
        clip_right = std::min<int64_t>(screenw, clip_right);
        if (clip_left >= clip_right) return;
        const unsigned alpha = rgba >> 24;
        if (!alpha) return;
        for (const auto& p : layout.glyphs) {
            const Glyph& g = *p.glyph;
            const int64_t left = x + p.x + g.x, top = baseline + g.y;
            const int64_t x0 = std::max<int64_t>(0, clip_left - left);
            const int64_t end_x = std::min<int64_t>(g.width, clip_right - left);
            const int64_t start_y = std::max<int64_t>(0, -top);
            const int64_t end_y = std::min<int64_t>(g.height, int64_t(screenh) - top);
            if (int64_t(x0) >= end_x || start_y >= end_y) continue;
            for (int64_t gy = start_y; gy < end_y; ++gy) for (int64_t gx = x0; gx < end_x; ++gx) {
                const unsigned a = (g.pixels[size_t(gy) * g.width + size_t(gx)] * alpha + 127) / 255;
                if (!a) continue;
                uint32_t& dst = frame[size_t(top + gy) * pitch + size_t(left + gx)];
                uint32_t out = 0xff000000u;
                for (unsigned shift = 0; shift < 24; shift += 8) {
                    const unsigned src = (rgba >> shift) & 255, old = (dst >> shift) & 255;
                    out |= ((src * a + old * (255 - a) + 127) / 255) << shift;
                }
                dst = out;
            }
        }
    }
};
} // namespace host_font
#endif
