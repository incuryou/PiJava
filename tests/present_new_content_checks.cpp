// 0.2.114 item 4 (user-approved): the FPS counter (top-left badge and STAT canvas_fps, one counter) advances only for
// in-game presents that carry a new frame (g_fb_dirty true at present time). The 50 ms heartbeat re-present, forced
// border clears and the pause sheet are still presented exactly as before, but are not counted.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <fstream>
#include <sstream>

static int checks;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL PresentNewContentCount " << why << "\n"; std::exit(1); } }
static double stat_canvas_fps() {
    VMS_RS(g_paint_stats).window_start_us = paint_clock_us() - 1000000;
    std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf()); maybe_emit_runtime_stats(); std::cerr.rdbuf(old);
    const std::string s = out.str(); const size_t at = s.find(" canvas_fps=");
    check(at != std::string::npos, "STAT line emitted");
    return std::atof(s.c_str() + at + 12);
}
static std::string squeeze(const std::string& path) {
    std::ifstream f(path); std::stringstream b; b << f.rdbuf(); std::string s; for (char c : b.str()) if (c != ' ' && c != '\t') s += c; return s;
}
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    int presented = 0; auto pres = [&] { ++presented; };
    auto heartbeat_ready = [] { VMS_PLAT(g_last_present_us) = paint_clock_us() - 60000; };
    {   // A. heartbeat re-present
        VMS_RS(g_fb_dirty) = false; heartbeat_ready(); const uint64_t f0 = VMS_RS(g_present_frames), p0 = g_sched_core.presents; presented = 0;
        check(sched_present_turn(true, false, false, 3, pres) && presented == 1 && g_sched_core.presents == p0 + 1, "A1: heartbeat is still presented");
        check(VMS_RS(g_present_frames) == f0, "A2: heartbeat re-present is not counted");
    }
    {   // B. new content
        VMS_RS(g_fb_dirty) = true; const uint64_t f0 = VMS_RS(g_present_frames); presented = 0;
        check(sched_present_turn(true, false, false, 3, pres) && presented == 1 && VMS_RS(g_present_frames) == f0 + 1, "B1: a present with a new frame counts");
        check(!VMS_RS(g_fb_dirty), "B2: post_present still clears g_fb_dirty after the count");
    }
    {   // C. forced border clear (cpu_cleared<3) without new content
        VMS_RS(g_fb_dirty) = false; VMS_PLAT(g_last_present_us) = paint_clock_us(); const uint64_t f0 = VMS_RS(g_present_frames); presented = 0;
        check(sched_present_turn(true, false, false, 0, pres) && presented == 1 && VMS_RS(g_present_frames) == f0, "C: forced clear present not counted");
    }
    {   // D. pause / exit sheet in game
        VMS_RS(g_fb_dirty) = false; VMS_PLAT(g_last_present_us) = paint_clock_us(); const uint64_t f0 = VMS_RS(g_present_frames); presented = 0;
        check(sched_present_turn(true, true, false, 3, pres) && presented == 1 && VMS_RS(g_present_frames) == f0, "D: exit sheet present not counted");
    }
    {   // E. menu: never counted
        const uint64_t f0 = VMS_RS(g_present_frames);
        for (int i = 0; i < 50; ++i) { VMS_RS(g_fb_dirty) = (i & 1); sched_present_turn(false, false, false, 3, pres); }
        check(VMS_RS(g_present_frames) == f0, "E: menu presents not counted (dirty or not)");
    }
    {   // F. one second: 12 new frames + 8 heartbeat re-presents -> canvas_fps ~12 (was ~20)
        reset_runtime_stats(); presented = 0;
        for (int i = 0; i < 12; ++i) { VMS_RS(g_fb_dirty) = true; sched_present_turn(true, false, false, 3, pres); }
        for (int i = 0; i < 8; ++i) { VMS_RS(g_fb_dirty) = false; heartbeat_ready(); sched_present_turn(true, false, false, 3, pres); }
        check(presented == 20, "F1: all 20 presents happened");
        const double fps = stat_canvas_fps();
        check(fps > 11.0 && fps < 13.0, "F2: STAT canvas_fps counts only the 12 new frames");
    }
    {   // G. source parity: both counter lines (shared core + Switch legacy path) use the same condition; the badge reads the same counter
        const std::string core = squeeze("src/scheduler_core.inc"), sw = squeeze("src/platform/switch_host.inc");
        size_t nc = 0, ns = 0; for (size_t p = core.find("++VMS_RS(g_present_frames)"); p != std::string::npos; p = core.find("++VMS_RS(g_present_frames)", p + 1)) ++nc;
        for (size_t p = sw.find("++VMS_RS(g_present_frames)"); p != std::string::npos; p = sw.find("++VMS_RS(g_present_frames)", p + 1)) ++ns;
        check(nc == 1 && ns == 1, "G1: exactly one counter line in each file");
        check(core.find("if(in_game&&VMS_RS(g_fb_dirty))++VMS_RS(g_present_frames);") != std::string::npos, "G2: scheduler_core.inc condition");
        check(sw.find("if(in_game&&VMS_RS(g_fb_dirty))++VMS_RS(g_present_frames);") != std::string::npos, "G3: switch_host.inc condition");
        check(sw.find("constuint64_tfps_frames=VMS_RS(g_present_frames);") != std::string::npos, "G4: top-left badge reads the same counter as STAT");
    }
    std::cout << "PASS PresentNewContentCount " << checks << " new-frame present count checks\n";
    return 0;
}
