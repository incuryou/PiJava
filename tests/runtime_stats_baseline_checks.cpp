// 0.2.113 item 2 (F4): STAT canvas_fps counts only in-game presents, and its window baseline is re-taken by
// reset_runtime_stats() (launch, continue/restore, resume), so the first STAT after a launch is not inflated by
// menu presents or by frames of an earlier session.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <sstream>

static int checks;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL RuntimeStatsBaseline " << why << "\n"; std::exit(1); } }
static int presents(bool in_game, int n) {
    int shown = 0;
    for (int i = 0; i < n; ++i) { VMS_RS(g_fb_dirty) = true; shown += sched_present_turn(in_game, false, false, 3, [] {}) ? 1 : 0; }
    return shown;
}
static double stat_canvas_fps() {   // force a closed 1.000 s window and read canvas_fps from the STAT line
    VMS_RS(g_paint_stats).window_start_us = paint_clock_us() - 1000000;
    std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf()); maybe_emit_runtime_stats(); std::cerr.rdbuf(old);
    const std::string s = out.str(); const size_t at = s.find(" canvas_fps=");
    check(at != std::string::npos, "STAT line emitted");
    return std::atof(s.c_str() + at + 12);
}
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    const uint64_t start = VMS_RS(g_present_frames);
    check(presents(false, 300) == 300 && VMS_RS(g_present_frames) == start, "menu presents are shown but not counted");
    check(presents(true, 200) > 0 && VMS_RS(g_present_frames) == start + 200, "in-game presents are counted");
    reset_runtime_stats();   // what launch / continue / resume do
    check(g_stat_present_seen == VMS_RS(g_present_frames), "reset_runtime_stats re-takes the STAT baseline");
    presents(false, 120); presents(true, 30);
    const double first = stat_canvas_fps();
    check(first > 29.0 && first < 31.0, "first STAT after launch reports only this window's in-game presents (30/s, not 230 or 350)");
    check(g_stat_present_seen == VMS_RS(g_present_frames), "the STAT window moves the baseline forward");
    presents(true, 45);
    VMS_RS(g_paint_stats).window_start_us = paint_clock_us() - 1000000; maybe_emit_runtime_stats();   // silent (10 s log period) window
    check(g_stat_present_seen == VMS_RS(g_present_frames), "silent windows move the baseline too (no carry-over into the next STAT)");
    presents(true, 10);
    stop_thread();           // exit to menu / relaunch resets through reset_runtime_stats
    check(g_stat_present_seen == VMS_RS(g_present_frames), "stop_thread re-takes the baseline");
    presents(false, 500);
    check(g_stat_present_seen == VMS_RS(g_present_frames), "menu presents after exit do not open a window");
    std::cout << "PASS RuntimeStatsBaseline " << checks << " F4 in-game present count and STAT baseline checks\n";
    return 0;
}
