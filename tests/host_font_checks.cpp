#include "../src/host_font.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

static std::string utf8(unsigned cp) {
    std::string s;
    if (cp < 0x80) s += char(cp);
    else if (cp < 0x800) { s += char(0xc0 | (cp >> 6)); s += char(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { s += char(0xe0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 63)); s += char(0x80 | (cp & 63)); }
    else { s += char(0xf0 | (cp >> 18)); s += char(0x80 | ((cp >> 12) & 63)); s += char(0x80 | ((cp >> 6) & 63)); s += char(0x80 | (cp & 63)); }
    return s;
}
static void centered(host_font::Font& font) {
    constexpr int width = 160, height = 100;
    for (const std::string label : {"A", "X", "B", "L", "R", "AV", "j", " "}) {
        std::vector<uint32_t> frame(width * height, 0xff000000u);
        assert(font.draw_centered(frame.data(), width, 80, 50, label,
                                  0xffffffffu, 28, 80, width, height));
        int left = width, right = -1, top = height, bottom = -1;
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            if (frame[y * width + x] == 0xff000000u) continue;
            left = std::min(left, x); right = std::max(right, x);
            top = std::min(top, y); bottom = std::max(bottom, y);
        }
        if (label == " ") { assert(right == -1); continue; }
        assert(right >= left && bottom >= top);
        assert(std::abs((left + right) / 2.0 - 80) <= 0.5);
        assert(std::abs((top + bottom) / 2.0 - 50) <= 0.5);
    }
}
int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    std::ifstream input(path, std::ios::binary);
    if (!input) { std::cerr << "Missing trusted local font fixture: " << path << '\n'; return 1; }
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
    host_font::Font font;
    std::vector<uint32_t> frame(240 * 100, 0xff000000u);
    assert(!font.ready());
    assert(!font.draw(frame.data(), 240, 5, 5, "A", 0xffffffffu, 24, 100, 240, 100));
    assert(!font.add_font(nullptr, 0));
    assert(!font.add_font(bytes.data(), 11));
    assert(!font.add_font(bytes.data(), 100));
    unsigned char broken_collection[16] = {'t','t','c','f',0,1,0,0,0,0,0,1,0xff,0xff,0xff,0xff};
    assert(!font.add_font(broken_collection, sizeof broken_collection));
    assert(font.add_font(bytes.data(), bytes.size()));
    assert(font.ready());
    assert(font.draw(frame.data(), 240, 10, 5, "AV / Smooth", 0xffffffffu, 24, 220, 240, 100));
    std::set<unsigned> levels;
    for (auto p : frame) levels.insert(p & 255);
    assert(levels.count(0) && levels.count(255) && levels.size() > 10);
    // Same-byte alpha blending is applied to R/G/B, while framebuffer alpha stays opaque.
    std::vector<uint32_t> alpha(240 * 100, 0xff101010u);
    assert(font.draw(alpha.data(), 240, 10, 5, "A", 0x80ffffffu, 30, 100, 240, 100));
    bool saw_alpha = false;
    for (auto p : alpha) {
        assert((p >> 24) == 255);
        assert((p & 255) == ((p >> 8) & 255) && (p & 255) == ((p >> 16) & 255));
        assert((p & 255) <= 136);
        saw_alpha |= (p & 255) > 16;
    }
    assert(saw_alpha);
    // A narrow clip with negative origin may touch screen pixels only in x=[0,7).
    constexpr int width = 40, height = 20, pitch = 47, guard = 19;
    constexpr uint32_t sentinel = 0xff153759u;
    std::vector<uint32_t> guarded(guard + pitch * height + guard, sentinel);
    auto* pixels = guarded.data() + guard;
    assert(font.draw(pixels, pitch, -8, -7, "Wide clipping", 0xffeeeeeeu, 31, 15, width, height));
    bool clipped_ink = false;
    for (int y = 0; y < height; ++y) for (int x = 0; x < pitch; ++x) {
        if (x >= 7) assert(pixels[y * pitch + x] == sentinel);
        else clipped_ink |= pixels[y * pitch + x] != sentinel;
    }
    assert(clipped_ink);
    for (int i = 0; i < guard; ++i) {
        assert(guarded[i] == sentinel);
        assert(guarded[guard + pitch * height + i] == sentinel);
    }
    const auto previous = guarded;
    assert(font.draw(pixels, pitch, INT_MIN, INT_MAX, "A", 0xffffffffu, 28, 20, width, height));
    assert(font.draw(pixels, pitch, INT_MAX, INT_MIN, "A", 0xffffffffu, 28, 20, width, height));
    assert(guarded == previous);
    centered(font);
    // Decode multibyte Unicode, supplementary-plane missing glyphs, and invalid UTF-8.
    std::vector<uint32_t> unicode(240 * 100, 0xff000000u), reference = unicode;
    assert(font.draw(unicode.data(), 240, 5, 5, utf8(0xe9) + utf8(0x3a9), 0xffffffffu, 24, 220, 240, 100));
    assert(font.draw(reference.data(), 240, 5, 5, "\xc3\xa9\xce\xa9", 0xffffffffu, 24, 220, 240, 100));
    assert(unicode == reference && unicode != std::vector<uint32_t>(240 * 100, 0xff000000u));
    std::fill(reference.begin(), reference.end(), 0xff000000u);
    font.draw(reference.data(), 240, 5, 5, utf8(0xfffd) + utf8(0xfffd), 0xffffffffu, 24, 220, 240, 100);
    assert(unicode != reference);
    std::fill(unicode.begin(), unicode.end(), 0xff000000u);
    std::fill(reference.begin(), reference.end(), 0xff000000u);
    assert(font.draw(unicode.data(), 240, 5, 5, utf8(0x10ffff), 0xffffffffu, 24, 220, 240, 100));
    assert(font.draw(reference.data(), 240, 5, 5, utf8(0xfffd), 0xffffffffu, 24, 220, 240, 100));
    assert(unicode == reference);
    font.draw(unicode.data(), 240, 5, 5, "\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80\xe2", 0xffffffffu, 24, 220, 240, 100);
    // Many sizes/glyphs exercise both cache budgets; clipped output avoids large frames.
    for (int i = 0; i < 1200; ++i) {
        font.draw(pixels, pitch, 0, 0, utf8(33 + i % 100), 0xffffffffu, 10 + i / 100, 1, width, height);
        assert(font.cache_glyphs() <= host_font::Font::max_cache_glyphs);
        assert(font.cache_bytes() <= host_font::Font::max_cache_bytes);
    }
    assert(font.cache_glyphs() == host_font::Font::max_cache_glyphs);
    for (int i = 0; i < 1000; ++i) {
        font.draw(pixels, pitch, 0, 0, utf8(33 + i % 100), 0xffffffffu, 220 + i / 100, 1, width, height);
        assert(font.cache_glyphs() <= host_font::Font::max_cache_glyphs);
        assert(font.cache_bytes() <= host_font::Font::max_cache_bytes);
    }
    assert(font.cache_glyphs() < host_font::Font::max_cache_glyphs);
    assert(font.add_font(bytes.data(), bytes.size()));
    assert(font.cache_bytes() == 0 && font.cache_glyphs() == 0);
    std::cout << "host_font_checks: passed (antialiasing, alpha, clipping, Unicode, ink centering, cache budgets)\n";
}
