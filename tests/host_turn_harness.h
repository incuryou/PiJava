// P2.6: desktop harness host turn. Default = the SAME scheduler core the Switch build runs (sched_game_work +
// sched_present_turn); the harness supplies only a no-op frame acquire, host naps and a 16 ms "vsync" present.
// J2ME_SCHED_LEGACY=1 = the exact P2.5 hand-written harness loop (fallback / old-path comparison).
#pragma once
static bool harness_host_turn(int64_t& last_present, bool heartbeat_needs_present) {
    if (sched_legacy()) {
        const bool heartbeat = (!heartbeat_needs_present || last_present) && paint_clock_us() - last_present >= 50000;
        p1_host_work_turn(false, heartbeat, [](bool){ scheduled_frame([](){}, game_wake_delay,
            [](int64_t d){ std::this_thread::sleep_for(std::chrono::milliseconds(d)); }, advance_game_frame,
            VMS_EXEC(g_frame_wait_budget), paint_clock_us, &VMS_EXEC(g_wake_stats)); },
            [](int64_t d){ std::this_thread::sleep_for(std::chrono::milliseconds(d)); });
        if (p1::should_show(true, false, false, VMS_RS(g_fb_dirty), 3, heartbeat, speed_group_incomplete())) {
            p7_present_measure(); p1_host_post_present(true, false); last_present = paint_clock_us();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            return true;
        }
        return false;
    }
    sched_game_work(false, [] {}, [](int64_t d) { std::this_thread::sleep_for(std::chrono::milliseconds(d)); });
    const bool shown = sched_present_turn(true, false, false, 3, [] { p7_present_measure(); std::this_thread::sleep_for(std::chrono::milliseconds(16)); });
    if (shown) last_present = paint_clock_us();
    maybe_emit_runtime_stats();
    return shown;
}
