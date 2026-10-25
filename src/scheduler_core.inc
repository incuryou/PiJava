// P2.6: ONE scheduler core for desktop and Switch. Platform layers only provide clock-free I/O hooks:
// frame acquisition, host sleep, presentation. Every scheduling decision (pacing sleep, heartbeat, work dispatch,
// scheduled-frame wake budget, present decision, post-present speculation, budget feedback, worker slice length)
// lives here and is identical on both platforms.
// Fallback: the legacy per-platform paths stay compiled. Switch: sdmc:/switch/j2me/sched_legacy.enable;
// desktop: J2ME_SCHED_LEGACY=1. Legacy = P2.5 behaviour exactly (desktop worker slice 4 ms, hand-written loops).
struct SchedCoreStats { uint64_t work_turns=0, present_turns=0, presents=0, observes=0, legacy=0; };
static SchedCoreStats g_sched_core;
static bool sched_legacy() {
    static const bool legacy = [] {
#ifdef __SWITCH__
        struct stat st{}; return stat("sdmc:/switch/j2me/sched_legacy.enable", &st) == 0;
#else
        const char* e = std::getenv("J2ME_SCHED_LEGACY"); return e && std::strcmp(e, "1") == 0;
#endif
    }();
    return legacy;
}
// Worker slice policy (core). Unified = the production (Switch) values on every platform: 14 ms for the worker
// turn inside advance_game_frame, 4 ms otherwise. Legacy desktop kept the 4 ms default for the worker too.
static int sched_worker_slice_ms() {
#ifdef __SWITCH__
    return 14;
#else
    return sched_legacy() ? VMS_EXEC(g_slice_ms) : 14;
#endif
}
// One in-game work turn: production pacing sleep, heartbeat, work dispatch.
// acquire = platform frame-slot acquisition; idle_sleep = platform host nap. Wake waits are core (wait_game_wake).
template<class Acquire, class SleepMs>
static p1::Work sched_game_work(bool gpu, Acquire acquire, SleepMs idle_sleep) {
    ++g_sched_core.work_turns;
    speed_wait_paced_sleep();
    const int64_t pace_now = paint_clock_us();
    const bool present_due = VMS_PLAT(g_last_present_us) != 0 && pace_now - VMS_PLAT(g_last_present_us) >= 50000;
    return p1_host_work_turn(gpu, present_due, [&](bool g) {
        if (g) scheduled_frame(acquire, game_wake_delay, wait_game_wake, advance_game_frame, 0);
        else scheduled_frame(acquire, game_wake_delay, wait_game_wake, advance_game_frame,
            VMS_EXEC(g_frame_wait_budget), paint_clock_us, &VMS_EXEC(g_wake_stats));
    }, idle_sleep);
}
// The present decision and its consequences. present() is the platform presenter.
template<class Present>
static bool sched_present_turn(bool in_game, bool exit_sheet, bool gpu, int cpu_cleared, Present present) {
    ++g_sched_core.present_turns;
    const bool heartbeat = VMS_PLAT(g_last_present_us) != 0 && paint_clock_us() - VMS_PLAT(g_last_present_us) >= 50000;
    if (p1::should_show(in_game, exit_sheet, gpu, VMS_RS(g_fb_dirty), cpu_cleared, heartbeat, speed_group_incomplete())) {
        ++g_sched_core.presents;
        present(); VMS_PLAT(g_last_present_us) = paint_clock_us(); if (in_game && VMS_RS(g_fb_dirty)) ++VMS_RS(g_present_frames);  // 0.2.114 F4: only presents carrying a new frame (heartbeat re-presents not counted); 0.2.113: in-game only
        p1_host_post_present(in_game, exit_sheet);
        return true;
    }
    if (VMS_EXEC(g_frame_work_finished_us)) {
        ++g_sched_core.observes;
        VMS_EXEC(g_frame_wait_budget).observe(VMS_EXEC(g_frame_work_us), 0);
        VMS_EXEC(g_frame_work_finished_us) = 0;
    }
    return false;
}
static void sched_core_report() {
    log_line("P26OBS sched mode=" + std::string(sched_legacy() ? "legacy" : "unified") + " worker_slice_ms=" + std::to_string(sched_worker_slice_ms()) +
        " work_turns=" + std::to_string(g_sched_core.work_turns) + " present_turns=" + std::to_string(g_sched_core.present_turns) +
        " presents=" + std::to_string(g_sched_core.presents) + " observes=" + std::to_string(g_sched_core.observes) + " scope=process_cumulative");
}
