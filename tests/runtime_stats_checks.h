#pragma once
#include <cassert>
// Deliberately dirty every reporting window before a host state transition.
static void seed_runtime_stats() {
    VMS_RS(g_paint_stats).completed(100);VMS_RS(g_paint_stats).worker_elapsed(100);
    VMS_PLAT(g_present_stats).count=1;
    VMS_RS(g_graphics_profile).pending.sampled_paints=1;VMS_RS(g_graphics_profile).global_pending.native_calls=1;
    VMS_EXEC(g_runtime_profile).pending.worker_gross_us=100;VMS_EXEC(g_runtime_profile).pending.gc_count=1;
    VMS_EXEC(g_wake_stats).sleep_wakes=1;VMS_EXEC(g_pace_aligns)=1;
    VMS_EXEC(g_frame_wait_budget).observe(100,100);VMS_EXEC(g_frame_work_us)=VMS_EXEC(g_frame_work_finished_us)=VMS_EXEC(g_last_slice_us)=100;
    VMS_EXEC(g_poll_calls)=VMS_EXEC(g_poll_gate_reads)=VMS_EXEC(g_poll_waits)=VMS_EXEC(g_poll_wait_us)=1;
    VMS_EXEC(g_speed_slept_us)=VMS_EXEC(g_speed_cut_us)=VMS_EXEC(g_speed_add_us)=100;
    VMS_EXEC(g_speed_budget_window_frames)=VMS_EXEC(g_speed_group_timeouts)=1;
}
static void assert_runtime_stats_reset() {
    assert(VMS_RS(g_paint_stats).pending.canvas_paints==0 && VMS_RS(g_paint_stats).pending.worker_us==0 && VMS_PLAT(g_present_stats).count==0);
    assert(VMS_RS(g_graphics_profile).pending.sampled_paints==0 && VMS_RS(g_graphics_profile).global_pending.native_calls==0);
    assert(VMS_EXEC(g_runtime_profile).pending.worker_gross_us==0 && VMS_EXEC(g_runtime_profile).pending.gc_count==0);
    assert(VMS_EXEC(g_wake_stats).sleep_wakes==0 && VMS_EXEC(g_wake_stats).ignore_before_ms>0 && VMS_EXEC(g_pace_aligns)==0);
    assert(VMS_EXEC(g_frame_wait_budget).samples==0 && VMS_EXEC(g_frame_work_us)==0 && VMS_EXEC(g_frame_work_finished_us)==0 && VMS_EXEC(g_last_slice_us)==0);
    assert(!VMS_EXEC(g_poll_calls) && !VMS_EXEC(g_poll_gate_reads) && !VMS_EXEC(g_poll_waits) && !VMS_EXEC(g_poll_wait_us));
    assert(!VMS_EXEC(g_speed_slept_us) && !VMS_EXEC(g_speed_cut_us) && !VMS_EXEC(g_speed_add_us) && !VMS_EXEC(g_speed_budget_window_frames) && !VMS_EXEC(g_speed_group_timeouts));
}
