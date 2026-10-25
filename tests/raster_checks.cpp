// Host-only differential regression and microbenchmark; no Switch FPS claims.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <random>
#include <cassert>

static void legacy_draw_region(int gfx, int target, int image_ref, int sx, int sy, int w, int h, int transform, int dx, int dy) {
    if (image_ref <= 0 || image_ref >= (int)VMS_HEAP(g_heap).size()) return;
    Obj& src = VMS_HEAP(g_heap)[image_ref];
    int tw, th;
    auto* dst = pixels_of(target, tw, th);
    if (!dst || src.pixels.empty()) return;
    if (transform == 0 && src.alpha.empty() && w > 0 && h > 0 &&
        dx >= 0 && dy >= 0 && dx + w <= tw && dy + h <= th &&
        sx >= 0 && sy >= 0 && sx + w <= src.width && sy + h <= src.height) {
        auto it = VMS_RS(g_clip).find(gfx);
        bool inside = it == VMS_RS(g_clip).end() || !it->second.on ||
                      (dx >= it->second.x && dy >= it->second.y && dx + w <= it->second.x + it->second.w && dy + h <= it->second.y + it->second.h);
        if (inside) {
            for (int yy = 0; yy < h; ++yy)
                std::memcpy(&(*dst)[((size_t)(dy + yy) * tw + dx) * 3],
                            &src.pixels[((size_t)(sy + yy) * src.width + sx) * 3], (size_t)w * 3);
            if (VMS_RS(g_drawacc).on) VMS_RS(g_drawacc).images++;
            return;
        }
    }
    if (transform == 0 && !src.alpha.empty() && w > 0 && h > 0 &&
        dx >= 0 && dy >= 0 && dx + w <= tw && dy + h <= th &&
        sx >= 0 && sy >= 0 && sx + w <= src.width && sy + h <= src.height) {
        auto it = VMS_RS(g_clip).find(gfx);
        bool inside = it == VMS_RS(g_clip).end() || !it->second.on ||
                      (dx >= it->second.x && dy >= it->second.y && dx + w <= it->second.x + it->second.w && dy + h <= it->second.y + it->second.h);
        if (inside) {
            for (int yy = 0; yy < h; ++yy) {
                const uint8_t* ss = &src.pixels[((size_t)(sy + yy) * src.width + sx) * 3];
                const uint8_t* aa = &src.alpha[(size_t)(sy + yy) * src.width + sx];
                uint8_t* dd = &(*dst)[((size_t)(dy + yy) * tw + dx) * 3];
                for (int xx = 0; xx < w; ++xx, ss += 3, dd += 3) {
                    uint8_t a = aa[xx];
                    if (a == 0) continue;
                    if (a == 255) { dd[0] = ss[0]; dd[1] = ss[1]; dd[2] = ss[2]; }
                    else {
                        dd[0] = (uint8_t)((ss[0] * a + dd[0] * (255 - a)) >> 8);
                        dd[1] = (uint8_t)((ss[1] * a + dd[1] * (255 - a)) >> 8);
                        dd[2] = (uint8_t)((ss[2] * a + dd[2] * (255 - a)) >> 8);
                    }
                }
            }
            if (VMS_RS(g_drawacc).on) VMS_RS(g_drawacc).images++;
            return;
        }
    }
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx) {
            int ox = xx, oy = yy;
            if (transform == 1) oy = h - 1 - yy;
            else if (transform == 2) ox = w - 1 - xx;
            else if (transform == 3) { ox = w - 1 - xx; oy = h - 1 - yy; }
            else if (transform == 4) { ox = yy; oy = xx; }
            else if (transform == 5) { ox = h - 1 - yy; oy = xx; }
            else if (transform == 6) { ox = yy; oy = w - 1 - xx; }
            else if (transform == 7) { ox = h - 1 - yy; oy = w - 1 - xx; }
            int x0 = sx + xx, y0 = sy + yy, x1 = dx + ox, y1 = dy + oy;
            if ((unsigned)x0 >= (unsigned)src.width || (unsigned)y0 >= (unsigned)src.height) continue;
            if ((unsigned)x1 >= (unsigned)tw || (unsigned)y1 >= (unsigned)th) continue;
            if (!clip_ok(gfx, x1, y1)) continue;
            size_t si = ((size_t)y0 * src.width + x0) * 3;
            size_t di = ((size_t)y1 * tw + x1) * 3;
            int a = src.alpha.empty() ? 255 : src.alpha[(size_t)y0 * src.width + x0];
            if (a == 0) continue;
            if (a >= 255) {
                (*dst)[di] = src.pixels[si]; (*dst)[di + 1] = src.pixels[si + 1]; (*dst)[di + 2] = src.pixels[si + 2];
            } else {
                (*dst)[di] = (uint8_t)((src.pixels[si] * a + (*dst)[di] * (255 - a)) / 255);
                (*dst)[di + 1] = (uint8_t)((src.pixels[si + 1] * a + (*dst)[di + 1] * (255 - a)) / 255);
                (*dst)[di + 2] = (uint8_t)((src.pixels[si + 2] * a + (*dst)[di + 2] * (255 - a)) / 255);
            }
        }
    if (VMS_RS(g_drawacc).on) VMS_RS(g_drawacc).images++;
}

static void raster_require(bool value, const char* message) {
    if (!value) { std::cerr << "RASTER FAIL " << message << '\n'; std::exit(1); }
}
static uint64_t checksum(const std::vector<uint8_t>& bytes) {
    uint64_t value = 0;
    for (uint8_t b : bytes) value = value * 131 + b;
    return value;
}
static void setup_image(int ref, int width, int height, bool alpha, std::mt19937& rng) {
    Obj& image = VMS_HEAP(g_heap)[ref]; image.width = width; image.height = height;
    image.pixels.resize(size_t(width) * height * 3);
    for (auto& b : image.pixels) b = uint8_t(rng());
    image.alpha.clear();
    if (alpha) {
        image.alpha.resize(size_t(width) * height);
        for (auto& a : image.alpha) { unsigned n = rng() % 5; a = n == 0 ? 0 : n == 1 ? 255 : uint8_t(rng()); }
    }
}
static void differential() {
    std::mt19937 rng(0x17071);
    VMS_HEAP(g_heap).resize(3); VMS_RS(g_drawacc).on = true;
    unsigned count = 0;
    for (int i = 0; i < 24000; ++i) {
        bool alias = ((i / 10) % 4 == 0), alpha = (i % 3 != 0);
        int sw = 1 + int(rng() % 35), sh = 1 + int(rng() % 31);
        setup_image(1, sw, sh, alpha, rng);
        if (!alias) setup_image(2, 1 + int(rng() % 35), 1 + int(rng() % 31), false, rng);
        int target = alias ? 1 : 2;
        int w = int(rng() % 39) - 2, h = int(rng() % 37) - 2;
        int sx = int(rng() % 49) - 12, sy = int(rng() % 43) - 10;
        int dx = int(rng() % 49) - 12, dy = int(rng() % 43) - 10;
        int transform = i % 10; // Invalid transforms retain the legacy identity fallback.
        VMS_RS(g_clip).clear();
        if (i % 3) { Clip c; c.on = i % 7 != 0; c.x = int(rng() % 23) - 4; c.y = int(rng() % 23) - 4;
            c.w = int(rng() % 37) - 2; c.h = int(rng() % 33) - 2; VMS_RS(g_clip)[17] = c; }
        // The legacy fully visible opaque self-copy used overlapping memcpy (undefined).
        // Exclude those cases from this exact comparison; fallback aliases are covered.
        if (alias && !alpha && transform == 0 && w > 0 && h > 0 &&
            dx >= 0 && dy >= 0 && dx + w <= sw && dy + h <= sh &&
            sx >= 0 && sy >= 0 && sx + w <= sw && sy + h <= sh) continue;
        auto initial = VMS_HEAP(g_heap)[target].pixels;
        auto images = VMS_RS(g_drawacc).images;
        legacy_draw_region(17, target, 1, sx, sy, w, h, transform, dx, dy);
        auto expected = VMS_HEAP(g_heap)[target].pixels;
        auto expected_images = VMS_RS(g_drawacc).images - images;
        VMS_HEAP(g_heap)[target].pixels = initial; images = VMS_RS(g_drawacc).images;
        draw_region(17, target, 1, sx, sy, w, h, transform, dx, dy);
        if (VMS_HEAP(g_heap)[target].pixels != expected) {
            std::cerr << "case=" << i << " alias=" << alias << " alpha=" << alpha << " transform=" << transform
                << " source=" << sw << ',' << sh << " region=" << sx << ',' << sy << ',' << w << ',' << h
                << " destination=" << dx << ',' << dy << '\n';
            raster_require(false, "legacy pixel mismatch");
        }
        raster_require(VMS_RS(g_drawacc).images - images == expected_images, "draw counter mismatch"); ++count;
    }
    // Explicitly test the historical /256 full-alpha versus /255 clipped-alpha split.
    VMS_RS(g_clip).clear(); setup_image(1, 3, 1, true, rng); setup_image(2, 3, 1, false, rng);
    VMS_HEAP(g_heap)[1].pixels.assign(9, 255); VMS_HEAP(g_heap)[1].alpha.assign(3, 128); VMS_HEAP(g_heap)[2].pixels.assign(9, 255);
    draw_region(17, 2, 1, 0, 0, 3, 1, 0, 0, 0);
    raster_require(VMS_HEAP(g_heap)[2].pixels[0] == 254, "full-alpha legacy rounding");
    Clip c; c.on = true; c.x = 0; c.y = 0; c.w = 2; c.h = 1; VMS_RS(g_clip)[17] = c;
    VMS_HEAP(g_heap)[2].pixels.assign(9, 255); draw_region(17, 2, 1, 0, 0, 3, 1, 0, 0, 0);
    raster_require(VMS_HEAP(g_heap)[2].pixels[0] == 255, "clipped-alpha legacy rounding");
    std::cout << "raster differential: " << count + 2 << " cases passed\n";
}
static void safety_and_alias_checks() {
    std::mt19937 rng(71); VMS_HEAP(g_heap).resize(3); VMS_RS(g_clip).clear();
    setup_image(1, 7, 5, false, rng); setup_image(2, 7, 5, false, rng);
    auto initial = VMS_HEAP(g_heap)[2].pixels;
    for (int transform = 0; transform < 10; ++transform) {
        draw_region(17, 2, 1, INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, transform, INT32_MAX, INT32_MIN);
        draw_region(17, 2, 1, INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX, transform, INT32_MIN, INT32_MAX);
        raster_require(VMS_HEAP(g_heap)[2].pixels == initial, "extreme coordinates changed destination");
    }
    // A giant mirrored rectangle has only the 7x5 source intersection to visit.
    draw_region(17, 2, 1, 0, 0, 7, 5, 3, 0, 0);
    auto mirrored = VMS_HEAP(g_heap)[2].pixels;
    VMS_HEAP(g_heap)[2].pixels = initial;
    draw_region(17, 2, 1, 0, 0, INT32_MAX, INT32_MAX, 3, INT32_MIN + 8, INT32_MIN + 6);
    raster_require(VMS_HEAP(g_heap)[2].pixels == mirrored, "giant visible rectangle clipping");
    VMS_HEAP(g_heap)[2].pixels = initial;
    Clip c; c.on = true; c.x = INT32_MAX; c.y = INT32_MAX; c.w = INT32_MAX; c.h = INT32_MAX; VMS_RS(g_clip)[17] = c;
    draw_region(17, 2, 1, 0, 0, 7, 5, 0, 0, 0);
    raster_require(VMS_HEAP(g_heap)[2].pixels == initial, "overflowing clip changed destination");
    VMS_RS(g_clip).clear();
    for (int malformed = 0; malformed < 4; ++malformed) {
        setup_image(1, 7, 5, true, rng); setup_image(2, 7, 5, false, rng); initial = VMS_HEAP(g_heap)[2].pixels;
        if (malformed == 0) VMS_HEAP(g_heap)[1].pixels.resize(1);
        if (malformed == 1) VMS_HEAP(g_heap)[1].alpha.resize(1);
        if (malformed == 2) VMS_HEAP(g_heap)[2].pixels.resize(1);
        if (malformed == 3) VMS_HEAP(g_heap)[1].width = INT32_MAX;
        auto saved = VMS_HEAP(g_heap)[2].pixels;
        draw_region(17, 2, 1, 0, 0, 7, 5, 0, 0, 0);
        raster_require(VMS_HEAP(g_heap)[2].pixels == saved, "incomplete storage changed destination");
    }
    // Define the old memcpy overlap case as per-row memmove, while retaining
    // top-to-bottom propagation across overlapping rows (no whole-image snapshot).
    for (int vertical = 0; vertical < 2; ++vertical) {
        setup_image(1, 7, 5, false, rng);
        auto expected = VMS_HEAP(g_heap)[1].pixels;
        int sx = 0, sy = 0, dx = vertical ? 0 : 1, dy = vertical ? 1 : 0;
        int w = vertical ? 7 : 6, h = vertical ? 4 : 5;
        for (int y = 0; y < h; ++y)
            std::memmove(expected.data() + ((dy + y) * 7 + dx) * 3,
                         expected.data() + ((sy + y) * 7 + sx) * 3, size_t(w) * 3);
        draw_region(17, 1, 1, sx, sy, w, h, 0, dx, dy);
        raster_require(VMS_HEAP(g_heap)[1].pixels == expected, "full-identity alias row order");
    }
    setup_image(1, 7, 5, true, rng); VMS_RS(g_screen_w) = 7; VMS_RS(g_screen_h) = 5; VMS_RS(g_fb).assign(7 * 5 * 3, 27);
    auto saved = VMS_RS(g_fb); legacy_draw_region(17, 0, 1, 0, 0, 7, 5, 5, 0, 0); auto expected = VMS_RS(g_fb);
    VMS_RS(g_fb) = saved; draw_region(17, 0, 1, 0, 0, 7, 5, 5, 0, 0);
    raster_require(VMS_RS(g_fb) == expected, "screen target integration");
    std::cout << "raster safety, alias, screen checks passed\n";
}
using RasterFn = void(*)(int,int,int,int,int,int,int,int,int,int);
static long benchmark(RasterFn fn, const char* kind) {
    auto start = std::chrono::steady_clock::now();
    for (int round = 0; round < 120; ++round)
        for (int i = 0; i < 550; ++i) {
            int transform = std::strcmp(kind, "full-alpha") == 0 ? 0 : i % 8;
            int x = transform == 0 && std::strcmp(kind, "full-alpha") == 0 ? 40 : (i * 37) % 280 - 20;
            int y = transform == 0 && std::strcmp(kind, "full-alpha") == 0 ? 40 : (i * 29) % 360 - 20;
            fn(17, 2, 1, i % 41, i % 47, 48, 40, transform, x, y);
        }
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}
static void benchmarks() {
    std::mt19937 rng(0x71); VMS_HEAP(g_heap).resize(3); VMS_RS(g_drawacc).on = false;
    for (const char* kind : {"clipped-opaque", "clipped-alpha", "full-alpha"}) {
        setup_image(1, 96, 96, std::strcmp(kind, "clipped-opaque") != 0, rng); setup_image(2, 240, 320, false, rng);
        VMS_RS(g_clip).clear(); Clip c; c.on = true; c.x = 7; c.y = 9; c.w = 226; c.h = 300; VMS_RS(g_clip)[17] = c;
        auto initial = VMS_HEAP(g_heap)[2].pixels;
        std::cout << "host raster " << kind << " (120 frames x 550 sprites):";
        for (int trial = 0; trial < 3; ++trial) {
            long old_us, new_us; uint64_t old_sum, new_sum;
            auto old_run = [&] { VMS_HEAP(g_heap)[2].pixels = initial; old_us = benchmark(legacy_draw_region, kind); old_sum = checksum(VMS_HEAP(g_heap)[2].pixels); };
            auto new_run = [&] { VMS_HEAP(g_heap)[2].pixels = initial; new_us = benchmark(draw_region, kind); new_sum = checksum(VMS_HEAP(g_heap)[2].pixels); };
            if (trial % 2) { new_run(); old_run(); } else { old_run(); new_run(); }
            raster_require(old_sum == new_sum, "benchmark checksum mismatch");
            std::cout << " legacy_us=" << old_us << " current_us=" << new_us << " speedup=" << double(old_us) / new_us;
        }
        std::cout << '\n';
    }
}
int main(int argc, char** argv) {
    differential();
    safety_and_alias_checks();
    if (argc > 1 && std::string(argv[1]) == "--benchmark") benchmarks();
    return 0;
}
