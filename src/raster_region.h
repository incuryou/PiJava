#ifndef J2MEVM_RASTER_REGION_H
#define J2MEVM_RASTER_REGION_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace raster_region {
struct Clip { bool on = false; int x = 0, y = 0, w = 0, h = 0; };

// Clip in source traversal coordinates so self-blits keep their original read/write
// order. All rectangle arithmetic is widened before addition/subtraction.
template<int Transform, bool Alpha>
inline void pixels(const std::vector<uint8_t>& src, const std::vector<uint8_t>& alpha,
                   int sw, std::vector<uint8_t>& dst, int dw, int sx, int sy,
                   int w, int h, int dx, int dy, int64_t xx0, int64_t xx1,
                   int64_t yy0, int64_t yy1) {
    const int64_t xstep = Transform < 4 ? ((Transform == 2 || Transform == 3) ? -3 : 3)
        : ((Transform == 6 || Transform == 7) ? -int64_t(dw) * 3 : int64_t(dw) * 3);
    for (int64_t yy = yy0; yy < yy1; ++yy) {
        int64_t ox = xx0, oy = yy;
        if constexpr (Transform == 1) oy = int64_t(h) - 1 - yy;
        if constexpr (Transform == 2) ox = int64_t(w) - 1 - xx0;
        if constexpr (Transform == 3) { ox = int64_t(w) - 1 - xx0; oy = int64_t(h) - 1 - yy; }
        if constexpr (Transform == 4) { ox = yy; oy = xx0; }
        if constexpr (Transform == 5) { ox = int64_t(h) - 1 - yy; oy = xx0; }
        if constexpr (Transform == 6) { ox = yy; oy = int64_t(w) - 1 - xx0; }
        if constexpr (Transform == 7) { ox = int64_t(h) - 1 - yy; oy = int64_t(w) - 1 - xx0; }
        size_t si = (size_t(int64_t(sy) + yy) * size_t(sw) + size_t(int64_t(sx) + xx0)) * 3;
        int64_t di = ((int64_t(dy) + oy) * dw + int64_t(dx) + ox) * 3;
        size_t ai = si / 3;
        if constexpr (Transform == 0 && Alpha) {
            for (int64_t xx = xx0; xx < xx1; ) {
                unsigned a = alpha[ai];
                if (a == 0) { ++xx; si += 3; ++ai; di += 3; continue; }
                if (a != 255) {
                    for (int channel = 0; channel < 3; ++channel) {
                        unsigned value = src[si + channel] * a + dst[size_t(di) + channel] * (255 - a);
                        dst[size_t(di) + channel] = uint8_t(value / 255);
                    }
                    ++xx; si += 3; ++ai; di += 3; continue;
                }
                int64_t run = xx + 1;
                size_t ra = ai + 1;
                while (run < xx1 && alpha[ra] == 255) { ++run; ++ra; }
                std::memcpy(&dst[size_t(di)], &src[si], size_t(run - xx) * 3);
                si += size_t(run - xx) * 3;
                ai = ra;
                di += int64_t(run - xx) * 3;
                xx = run;
            }
            continue;
        }
        for (int64_t xx = xx0; xx < xx1; ++xx, si += 3, ++ai, di += xstep) {
            unsigned a = 255;
            if constexpr (Alpha) a = alpha[ai];
            if (a == 0) continue;
            if (a == 255) {
                dst[size_t(di)] = src[si]; dst[size_t(di) + 1] = src[si + 1]; dst[size_t(di) + 2] = src[si + 2];
            } else {
                for (int channel = 0; channel < 3; ++channel) {
                    unsigned value = src[si + channel] * a + dst[size_t(di) + channel] * (255 - a);
                    dst[size_t(di) + channel] = uint8_t(value / 255);
                }
            }
        }
    }
}

inline void draw(const std::vector<uint8_t>& src, const std::vector<uint8_t>& alpha,
                 int sw, int sh, std::vector<uint8_t>& dst, int dw, int dh,
                 int sx, int sy, int w, int h, int transform, int dx, int dy, const Clip& clip) {
    if (w <= 0 || h <= 0 || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    // Image dimensions can come from untrusted data; refuse incomplete storage.
    uint64_t source_count = uint64_t(sw) * uint64_t(sh), destination_count = uint64_t(dw) * uint64_t(dh);
    if (source_count > src.size() / 3 || destination_count > dst.size() / 3 ||
        (!alpha.empty() && source_count > alpha.size())) return;
    int64_t left = 0, top = 0, right = dw, bottom = dh;
    if (clip.on) {
        left = std::max(left, int64_t(clip.x)); top = std::max(top, int64_t(clip.y));
        right = std::min(right, int64_t(clip.x) + clip.w); bottom = std::min(bottom, int64_t(clip.y) + clip.h);
    }
    if (left >= right || top >= bottom) return;
    const bool full_identity = transform == 0 && sx >= 0 && sy >= 0 &&
        int64_t(sx) + w <= sw && int64_t(sy) + h <= sh && dx >= left && dy >= top &&
        int64_t(dx) + w <= right && int64_t(dy) + h <= bottom;
    // Keep the old pointer loop and its /256 rounding for fully visible alpha
    // sprites. Every clipped/transformed fallback continues to divide by 255.
    if (full_identity && !alpha.empty()) {
        for (int yy = 0; yy < h; ++yy) {
            const uint8_t* ss = src.data() + (size_t(sy + yy) * sw + sx) * 3;
            const uint8_t* aa = alpha.data() + size_t(sy + yy) * sw + sx;
            uint8_t* dd = dst.data() + (size_t(dy + yy) * dw + dx) * 3;
            for (int xx = 0; xx < w; ) {
                uint8_t a = aa[xx];
                if (a == 0) { ++xx; ss += 3; dd += 3; continue; }
                if (a != 255) {
                    dd[0] = uint8_t((ss[0] * a + dd[0] * (255 - a)) >> 8);
                    dd[1] = uint8_t((ss[1] * a + dd[1] * (255 - a)) >> 8);
                    dd[2] = uint8_t((ss[2] * a + dd[2] * (255 - a)) >> 8);
                    ++xx; ss += 3; dd += 3; continue;
                }
                int run = xx + 1;
                while (run < w && aa[run] == 255) ++run;
                std::memcpy(dd, ss, size_t(run - xx) * 3);
                ss += size_t(run - xx) * 3;
                dd += size_t(run - xx) * 3;
                xx = run;
            }
        }
        return;
    }
    int64_t xx0 = std::max(int64_t(0), -int64_t(sx)), yy0 = std::max(int64_t(0), -int64_t(sy));
    int64_t xx1 = std::min(int64_t(w), int64_t(sw) - sx), yy1 = std::min(int64_t(h), int64_t(sh) - sy);
    auto xclip = [&](int64_t lo, int64_t hi) { xx0 = std::max(xx0, lo); xx1 = std::min(xx1, hi); };
    auto yclip = [&](int64_t lo, int64_t hi) { yy0 = std::max(yy0, lo); yy1 = std::min(yy1, hi); };
    switch (transform) {
    case 1: xclip(left - dx, right - dx); yclip(int64_t(dy) + h - bottom, int64_t(dy) + h - top); break;
    case 2: xclip(int64_t(dx) + w - right, int64_t(dx) + w - left); yclip(top - dy, bottom - dy); break;
    case 3: xclip(int64_t(dx) + w - right, int64_t(dx) + w - left); yclip(int64_t(dy) + h - bottom, int64_t(dy) + h - top); break;
    case 4: xclip(top - dy, bottom - dy); yclip(left - dx, right - dx); break;
    case 5: xclip(top - dy, bottom - dy); yclip(int64_t(dx) + h - right, int64_t(dx) + h - left); break;
    case 6: xclip(int64_t(dy) + w - bottom, int64_t(dy) + w - top); yclip(left - dx, right - dx); break;
    case 7: xclip(int64_t(dy) + w - bottom, int64_t(dy) + w - top); yclip(int64_t(dx) + h - right, int64_t(dx) + h - left); break;
    default: xclip(left - dx, right - dx); yclip(top - dy, bottom - dy); break;
    }
    if (xx0 >= xx1 || yy0 >= yy1) return;
    if (alpha.empty() && transform == 0 && (&src != &dst || full_identity)) {
        for (int64_t yy = yy0; yy < yy1; ++yy) {
            const uint8_t* source = src.data() + (size_t(int64_t(sy) + yy) * sw + size_t(int64_t(sx) + xx0)) * 3;
            uint8_t* destination = dst.data() + (size_t(int64_t(dy) + yy) * dw + size_t(int64_t(dx) + xx0)) * 3;
            // Preserve row-by-row self-copy order. memmove defines overlapping
            // rows where the historical full-identity memcpy was undefined.
            if (&src == &dst) std::memmove(destination, source, size_t(xx1 - xx0) * 3);
            else std::memcpy(destination, source, size_t(xx1 - xx0) * 3);
        }
        return;
    }
#define J2MEVM_RASTER_CASE(N) case N: \
    if (alpha.empty()) pixels<N, false>(src, alpha, sw, dst, dw, sx, sy, w, h, dx, dy, xx0, xx1, yy0, yy1); \
    else pixels<N, true>(src, alpha, sw, dst, dw, sx, sy, w, h, dx, dy, xx0, xx1, yy0, yy1); break
    switch (transform) {
    J2MEVM_RASTER_CASE(1); J2MEVM_RASTER_CASE(2); J2MEVM_RASTER_CASE(3); J2MEVM_RASTER_CASE(4);
    J2MEVM_RASTER_CASE(5); J2MEVM_RASTER_CASE(6); J2MEVM_RASTER_CASE(7);
    default:
        if (alpha.empty()) pixels<0, false>(src, alpha, sw, dst, dw, sx, sy, w, h, dx, dy, xx0, xx1, yy0, yy1);
        else pixels<0, true>(src, alpha, sw, dst, dw, sx, sy, w, h, dx, dy, xx0, xx1, yy0, yy1);
    }
#undef J2MEVM_RASTER_CASE
}
} // namespace raster_region
#endif
