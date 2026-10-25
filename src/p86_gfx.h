// P8.6-r2 (0.2.77-p8.11-r2): log-only offscreen drawing breakdown, gated by the P8FRAME switch (p8-frame.enable).
// Times every drawImage/drawRegion (by path: plain copy, source alpha, transform, mutable-ARGB destination),
// Image.createImage and fillRect, split by destination (screen vs offscreen Image). Nothing feeds back.
#pragma once
#include <chrono>
#include <cstdint>
namespace p86 {
enum Cat { ImgPlain, ImgAlpha, ImgTransform, ImgArgbDst, CreateImage, FillRect, CreateBytes, CreateMutable, CreateNamed, kCats };
inline const char* const kNames[kCats] = {"img_plain", "img_alpha", "img_transform", "img_argbdst", "create_image", "fill_rect", "create_bytes", "create_mutable", "create_named"};
inline bool g_on = false;
struct Acc { uint64_t n = 0, ns = 0, max_ns = 0, px = 0; };
inline Acc g_sec[kCats][2];      // [cat][offscreen]
inline uint64_t g_frame_ns = 0;  // all timed natives since the current paint began (main thread)
inline int64_t now_ns() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Scope {
    int64_t t0 = 0; int cat; bool off; uint64_t px;
    Scope(int c, bool offscreen, uint64_t pixels = 0) : cat(c), off(offscreen), px(pixels) { if (g_on) t0 = now_ns(); }
    ~Scope() {
        if (!g_on || !t0) return;
        const uint64_t d = uint64_t(now_ns() - t0); Acc& a = g_sec[cat][off ? 1 : 0];
        ++a.n; a.ns += d; a.px += px; if (d > a.max_ns) a.max_ns = d; g_frame_ns += d;
    }
};
inline void reset_sec() { for (auto& r : g_sec) for (auto& a : r) a = Acc{}; }
}
// P8.6 wake statistics (see p86_wait_until_us in j2mevm.cpp).
namespace p86w { inline bool g_spin=false; inline uint64_t waits=0,spin_us=0,late_sum_us=0,late_max_us=0,late_1ms=0;
    inline void reset(){waits=spin_us=late_sum_us=late_max_us=late_1ms=0;} }
