// P7.5 desktop-only present measurement (J2ME_P7_PRESENT_MEASURE=1). The desktop harness "present" is a 16 ms sleep,
// so P7COUNT present_blit is 0 on desktop. This runs the per-frame CPU work the Switch presenter does on the main
// thread into private host buffers (never the Java framebuffer, no effect on timing decisions or draws):
//  gpu_prep = j2me_present::game_rgba (deko3d default path: RGB888 -> RGBA8 game layer before upload),
//  cpu_blit = the CPU-fallback 2x scale loop of compose_host_surface (game area into a 1280x720 frame).
// Not measurable here: GPU upload/submit, framebufferEnd, acquire waits, chrome rebuilds (menus/bezel, rare).
#pragma once
#ifndef __SWITCH__
#include "presentation/layers.h"
#include "p7_present.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
struct P7PresentMeasure { bool on = false; uint64_t frames = 0, gpu_prep_ns = 0, cpu_blit_ns = 0; uint32_t sink = 0; bool verify = false; uint64_t verified = 0, mismatch = 0;
    std::vector<uint32_t> rgba, frame, ref; };
inline P7PresentMeasure g_p7pm = [] { P7PresentMeasure m; const char* e = std::getenv("J2ME_P7_PRESENT_MEASURE"); m.on = e && std::strcmp(e, "1") == 0; const char* v = std::getenv("J2ME_P7_PRESENT_VERIFY"); m.verify = v && std::strcmp(v, "1") == 0; return m; }();
static void p7_present_measure() {
    if (!g_p7pm.on) return;
    P7_SCOPE(Present);
    const auto& fb = VMS_RS(g_fb); const int w = VMS_RS(g_screen_w), h = VMS_RS(g_screen_h);
    const auto t0 = std::chrono::steady_clock::now();
    j2me_present::game_rgba(fb, w, h, g_p7pm.rgba);
    const auto t1 = std::chrono::steady_clock::now();
    if (g_p7pm.frame.size() != j2me_present::canvas_pixels) g_p7pm.frame.assign(j2me_present::canvas_pixels, 0);
    const size_t need = size_t(w) * size_t(h) * 3;
    if (fb.size() >= need && 40 + h * 2 <= 720 && 400 + w * 2 <= 1280) {
        p7present::blit_game(g_p7pm.frame.data(), 1280, fb.data(), w, h); // the Switch CPU presenter's game loop
        if (g_p7pm.verify) { // frame-bytes check: serial reference into a second buffer, compare the whole frame
            if (g_p7pm.ref.size() != g_p7pm.frame.size()) g_p7pm.ref.assign(g_p7pm.frame.size(), 0);
            p7present::blit_rows(g_p7pm.ref.data(), 1280, fb.data(), w, 0, h);
            if (std::memcmp(g_p7pm.ref.data(), g_p7pm.frame.data(), g_p7pm.frame.size() * 4) != 0) ++g_p7pm.mismatch;
            ++g_p7pm.verified;
        }
    }
    const auto t2 = std::chrono::steady_clock::now();
    g_p7pm.sink ^= g_p7pm.frame[size_t(40) * 1280 + 400] ^ (g_p7pm.rgba.empty() ? 0u : g_p7pm.rgba[0]);
    ++g_p7pm.frames;
    g_p7pm.gpu_prep_ns += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    g_p7pm.cpu_blit_ns += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
}
static std::string p7_present_measure_summary() {
    return "P7PRESENT_MEASURE on=" + std::to_string(int(g_p7pm.on)) + " frames=" + std::to_string(g_p7pm.frames) +
           " gpu_prep_us=" + std::to_string(g_p7pm.gpu_prep_ns / 1000) + " cpu_blit_us=" + std::to_string(g_p7pm.cpu_blit_ns / 1000) + " sink=" + std::to_string(g_p7pm.sink) + " split=" + std::to_string(p7present::g_split) + " verified=" + std::to_string(g_p7pm.verified) + " mismatch=" + std::to_string(g_p7pm.mismatch) + " " + p7present::stats();
}
#else
static inline void p7_present_measure() {}
#endif
