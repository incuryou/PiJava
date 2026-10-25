// P7.5 present split; P7.6 adaptive serial/split by measured wall time (switch p7-present.enable / J2ME_P7_PRESENT=1, requires p7-worker, default off).
// The CPU presenter's 2x game scale (RGB888 -> XRGB, 240x320 -> 480x640 at (400,40) of the 1280x720 frame) is split by
// rows: worker 0 does the top half while the main thread does the bottom half, then the main thread waits before any
// later drawing (FPS badge, exit sheet) and before framebufferEnd. Same pixels, same order of visible operations, same
// present time or earlier: frame bytes are identical and display timing is not delayed.
#pragma once
#include "p7_worker.h"
#include <cstdint>
#include <string>
#include "logical_screen.h"
#include "presentation/filters.h"
namespace p7present {
inline bool g_enabled = false;
inline uint64_t g_split = 0, g_serial = 0, g_steal = 0, g_probe = 0;
// P7.6 adaptive rule (like P7.5 GC): per mode an EMA of blit_game wall ns per frame; probe each mode until it has
// 2 samples, then use the faster one and re-probe the slower one every ProbeEvery frames. Split mode lets the main
// thread take back the top half if worker 0 has not started it yet (Pool::steal), so a slow wake never adds a wait.
constexpr uint64_t ProbeEvery = 16;
inline uint64_t g_ema[2] = {0, 0}, g_n[2] = {0, 0}, g_frames = 0, g_wall_ns[2] = {0, 0};
inline void blit_rows(uint32_t* frame, uint32_t pitch, const uint8_t* fb, int w, int y0, int y1) {
    const uint8_t* src = fb + size_t(y0) * size_t(w) * 3;
    for (int sy = y0; sy < y1; ++sy) {
        uint32_t dy = 40 + uint32_t(sy) * 2;
        uint32_t* row0 = frame + dy * pitch + 400;
        uint32_t* row1 = frame + (dy + 1) * pitch + 400;
        for (int sx = 0; sx < w; ++sx, src += 3) {
            uint32_t c = src[0] | (uint32_t(src[1]) << 8) | (uint32_t(src[2]) << 16) | 0xff000000u;
            row0[0] = c; row0[1] = c; row1[0] = c; row1[1] = c;
            row0 += 2; row1 += 2;
        }
    }
}
inline int pick() {
    if (g_n[0] < 2) return 0;
    if (g_n[1] < 2) return 1;
    const int fast = g_ema[1] < g_ema[0] ? 1 : 0;
    if (g_frames % ProbeEvery == 0) { ++g_probe; return 1 - fast; }
    return fast;
}
inline uint64_t ns_of(uint64_t t) {
#ifdef __SWITCH__
    return armTicksToNs(t);
#else
    return t;
#endif
}
// Whole game area; serial or split across worker 0 + main, chosen by measured wall time. Pixels identical either way.
inline void blit_game(uint32_t* frame, uint32_t pitch, const uint8_t* fb, int w, int h, display_filter::Filter filter=display_filter::Filter::Original) {
    if(!logical_screen::valid(w,h)||!frame||!fb||pitch<1280)return;
    if(display_filter::normalized(filter)!=display_filter::Filter::Original){
        ++g_serial;display_filter::render(frame+size_t(40)*pitch+400,pitch,fb,w,h,filter);return;
    }
    if(w!=240||h!=320) {
        ++g_serial;const logical_screen::View view(w,h);
        for(int y=0;y<640;++y)for(int x=0;x<480;++x) {
            frame[size_t(40+y)*pitch+400+x]=view.sample(fb,x,y);
        }
        return;
    }
    if (!g_enabled || h < 2) { ++g_serial; blit_rows(frame, pitch, fb, w, 0, h); return; }
    const int mode = pick(); ++g_frames;
    const uint64_t t0 = p7::ticks();
    if (mode == 0) { ++g_serial; blit_rows(frame, pitch, fb, w, 0, h); }
    else {
        const int mid = h / 2;
        const uint64_t t = p7::pool().submit([frame, pitch, fb, w, mid] { P7_SCOPE(Present); blit_rows(frame, pitch, fb, w, 0, mid); }, 0);
        blit_rows(frame, pitch, fb, w, mid, h);
        p7::Pool::Task top;
        if (p7::pool().steal(t, top)) { ++g_steal; top(); }
        else { P7_SCOPE(PresentWait); p7::pool().wait(t); }
        ++g_split;
    }
    const uint64_t d = ns_of(p7::ticks() - t0);
    g_wall_ns[mode] += d;
    g_ema[mode] = g_n[mode]++ == 0 ? d : (g_ema[mode] * 7 + d) / 8;
}
inline std::string stats() {
    return "P7PRESENT_STATS split=" + std::to_string(g_split) + " serial=" + std::to_string(g_serial) + " steals=" + std::to_string(g_steal) +
           " probes=" + std::to_string(g_probe) + " ema_ns_serial=" + std::to_string(g_ema[0]) + " ema_ns_split=" + std::to_string(g_ema[1]) +
           " wall_us_serial=" + std::to_string(g_wall_ns[0] / 1000) + " wall_us_split=" + std::to_string(g_wall_ns[1] / 1000);
}
inline std::string configure(bool requested) {
    if (!requested) return "P7PRESENT schema=1 requested=0 enabled=0 reason=disabled";
    if (!p7::pool().enabled()) return "P7PRESENT schema=1 requested=1 enabled=0 reason=worker_off";
    g_enabled = true;
    return "P7PRESENT schema=1 requested=1 enabled=1 mode=row_split_worker0_top_main_bottom rule=adaptive_wall probe_every=16 steal=1 wait_before_overlay=1";
}
} // namespace p7present
