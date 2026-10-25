// P2.1-c4: Exec migration + ExecutionContext checks.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cstdio>
static int fails = 0;
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
template <class T> static bool inside(const T& x) {
    auto* lo = reinterpret_cast<const char*>(&j2me_vmstate::g_vm_storage);
    auto* p = reinterpret_cast<const char*>(&x);
    return p >= lo && p + sizeof(T) <= lo + sizeof(j2me_vmstate::g_vm_storage);
}
int main() {
    using namespace j2me_vmstate;
    CHECK(&VMS_EXEC(g_completed_paint241) == &g_vm_storage.exec.g_completed_paint241 && inside(VMS_EXEC(g_completed_paint241)), "exec.g_completed_paint241 single storage");
    CHECK(&VMS_EXEC(g_fast_blocks) == &g_vm_storage.exec.g_fast_blocks && inside(VMS_EXEC(g_fast_blocks)), "exec.g_fast_blocks single storage");
    CHECK(&VMS_EXEC(g_fast_ops) == &g_vm_storage.exec.g_fast_ops && inside(VMS_EXEC(g_fast_ops)), "exec.g_fast_ops single storage");
    CHECK(&VMS_EXEC(g_slow_ops) == &g_vm_storage.exec.g_slow_ops && inside(VMS_EXEC(g_slow_ops)), "exec.g_slow_ops single storage");
    CHECK(&VMS_EXEC(g_fast_float241_enabled) == &g_vm_storage.exec.g_fast_float241_enabled && inside(VMS_EXEC(g_fast_float241_enabled)), "exec.g_fast_float241_enabled single storage");
    CHECK(&VMS_EXEC(g_fast_float241_ops) == &g_vm_storage.exec.g_fast_float241_ops && inside(VMS_EXEC(g_fast_float241_ops)), "exec.g_fast_float241_ops single storage");
    CHECK(&VMS_EXEC(g_midlet) == &g_vm_storage.exec.g_midlet && inside(VMS_EXEC(g_midlet)), "exec.g_midlet single storage");
    CHECK(&VMS_EXEC(g_midlet_state) == &g_vm_storage.exec.g_midlet_state && inside(VMS_EXEC(g_midlet_state)), "exec.g_midlet_state single storage");
    CHECK(&VMS_EXEC(g_midlet_resume) == &g_vm_storage.exec.g_midlet_resume && inside(VMS_EXEC(g_midlet_resume)), "exec.g_midlet_resume single storage");
    CHECK(&VMS_EXEC(g_notify_destroyed) == &g_vm_storage.exec.g_notify_destroyed && inside(VMS_EXEC(g_notify_destroyed)), "exec.g_notify_destroyed single storage");
    CHECK(&VMS_EXEC(g_notify_paused) == &g_vm_storage.exec.g_notify_paused && inside(VMS_EXEC(g_notify_paused)), "exec.g_notify_paused single storage");
    CHECK(&VMS_EXEC(g_lifecycle_busy) == &g_vm_storage.exec.g_lifecycle_busy && inside(VMS_EXEC(g_lifecycle_busy)), "exec.g_lifecycle_busy single storage");
    CHECK(&VMS_EXEC(g_serial_calls) == &g_vm_storage.exec.g_serial_calls && inside(VMS_EXEC(g_serial_calls)), "exec.g_serial_calls single storage");
    CHECK(&VMS_EXEC(g_wake_stats) == &g_vm_storage.exec.g_wake_stats && inside(VMS_EXEC(g_wake_stats)), "exec.g_wake_stats single storage");
    CHECK(&VMS_EXEC(g_pace_aligns) == &g_vm_storage.exec.g_pace_aligns && inside(VMS_EXEC(g_pace_aligns)), "exec.g_pace_aligns single storage");
    CHECK(&VMS_EXEC(g_slice_ms) == &g_vm_storage.exec.g_slice_ms && inside(VMS_EXEC(g_slice_ms)), "exec.g_slice_ms single storage");
    CHECK(&VMS_EXEC(g_frame_wait_budget) == &g_vm_storage.exec.g_frame_wait_budget && inside(VMS_EXEC(g_frame_wait_budget)), "exec.g_frame_wait_budget single storage");
    CHECK(&VMS_EXEC(g_frame_work_finished_us) == &g_vm_storage.exec.g_frame_work_finished_us && inside(VMS_EXEC(g_frame_work_finished_us)), "exec.g_frame_work_finished_us single storage");
    CHECK(&VMS_EXEC(g_frame_work_us) == &g_vm_storage.exec.g_frame_work_us && inside(VMS_EXEC(g_frame_work_us)), "exec.g_frame_work_us single storage");
    CHECK(&VMS_EXEC(g_last_slice_us) == &g_vm_storage.exec.g_last_slice_us && inside(VMS_EXEC(g_last_slice_us)), "exec.g_last_slice_us single storage");
    CHECK(&VMS_EXEC(g_thread_live) == &g_vm_storage.exec.g_thread_live && inside(VMS_EXEC(g_thread_live)), "exec.g_thread_live single storage");
    CHECK(&VMS_EXEC(g_thrown) == &g_vm_storage.exec.ctx.g_thrown && inside(VMS_EXEC(g_thrown)), "exec.g_thrown single storage");
    CHECK(&VMS_EXEC(g_yield) == &g_vm_storage.exec.ctx.g_yield && inside(VMS_EXEC(g_yield)), "exec.g_yield single storage");
    CHECK(&VMS_EXEC(g_cuts) == &g_vm_storage.exec.g_cuts && inside(VMS_EXEC(g_cuts)), "exec.g_cuts single storage");
    CHECK(&VMS_EXEC(g_run_depth) == &g_vm_storage.exec.ctx.g_run_depth && inside(VMS_EXEC(g_run_depth)), "exec.g_run_depth single storage");
    CHECK(&VMS_EXEC(g_slices) == &g_vm_storage.exec.g_slices && inside(VMS_EXEC(g_slices)), "exec.g_slices single storage");
    CHECK(&VMS_EXEC(g_runtime_profile) == &g_vm_storage.exec.g_runtime_profile && inside(VMS_EXEC(g_runtime_profile)), "exec.g_runtime_profile single storage");
    CHECK(&VMS_EXEC(g_execution_profile) == &g_vm_storage.exec.g_execution_profile && inside(VMS_EXEC(g_execution_profile)), "exec.g_execution_profile single storage");
    CHECK(&VMS_EXEC(g_speed_cap) == &g_vm_storage.exec.g_speed_cap && inside(VMS_EXEC(g_speed_cap)), "exec.g_speed_cap single storage");
    CHECK(&VMS_EXEC(g_poll_calls) == &g_vm_storage.exec.g_poll_calls && inside(VMS_EXEC(g_poll_calls)), "exec.g_poll_calls single storage");
    CHECK(&VMS_EXEC(g_poll_gate_reads) == &g_vm_storage.exec.g_poll_gate_reads && inside(VMS_EXEC(g_poll_gate_reads)), "exec.g_poll_gate_reads single storage");
    CHECK(&VMS_EXEC(g_poll_wait_us) == &g_vm_storage.exec.g_poll_wait_us && inside(VMS_EXEC(g_poll_wait_us)), "exec.g_poll_wait_us single storage");
    CHECK(&VMS_EXEC(g_poll_waits) == &g_vm_storage.exec.g_poll_waits && inside(VMS_EXEC(g_poll_waits)), "exec.g_poll_waits single storage");
    CHECK(&VMS_EXEC(g_game_clock) == &g_vm_storage.exec.g_game_clock && inside(VMS_EXEC(g_game_clock)), "exec.g_game_clock single storage");
    CHECK(&VMS_EXEC(g_game_clock_mode) == &g_vm_storage.exec.g_game_clock_mode && inside(VMS_EXEC(g_game_clock_mode)), "exec.g_game_clock_mode single storage");
    CHECK(&VMS_EXEC(g_clock_input_hold_us) == &g_vm_storage.exec.g_clock_input_hold_us && inside(VMS_EXEC(g_clock_input_hold_us)), "exec.g_clock_input_hold_us single storage");
    CHECK(&VMS_EXEC(g_game_clock_inherited) == &g_vm_storage.exec.g_game_clock_inherited && inside(VMS_EXEC(g_game_clock_inherited)), "exec.g_game_clock_inherited single storage");
    CHECK(&VMS_EXEC(g_speed_last_paint_us) == &g_vm_storage.exec.g_speed_last_paint_us && inside(VMS_EXEC(g_speed_last_paint_us)), "exec.g_speed_last_paint_us single storage");
    CHECK(&VMS_EXEC(g_speed_next_us) == &g_vm_storage.exec.g_speed_next_us && inside(VMS_EXEC(g_speed_next_us)), "exec.g_speed_next_us single storage");
    CHECK(&VMS_EXEC(g_speed_slept_us) == &g_vm_storage.exec.g_speed_slept_us && inside(VMS_EXEC(g_speed_slept_us)), "exec.g_speed_slept_us single storage");
    CHECK(&VMS_EXEC(g_speed_cut_us) == &g_vm_storage.exec.g_speed_cut_us && inside(VMS_EXEC(g_speed_cut_us)), "exec.g_speed_cut_us single storage");
    CHECK(&VMS_EXEC(g_speed_add_us) == &g_vm_storage.exec.g_speed_add_us && inside(VMS_EXEC(g_speed_add_us)), "exec.g_speed_add_us single storage");
    CHECK(&VMS_EXEC(g_repaint_owner_ref) == &g_vm_storage.exec.ctx.g_repaint_owner_ref && inside(VMS_EXEC(g_repaint_owner_ref)), "exec.g_repaint_owner_ref single storage");
    CHECK(&VMS_EXEC(g_repaint_owner_yields) == &g_vm_storage.exec.ctx.g_repaint_owner_yields && inside(VMS_EXEC(g_repaint_owner_yields)), "exec.g_repaint_owner_yields single storage");
    CHECK(&VMS_EXEC(g_repaint_yield_canvas) == &g_vm_storage.exec.ctx.g_repaint_yield_canvas && inside(VMS_EXEC(g_repaint_yield_canvas)), "exec.g_repaint_yield_canvas single storage");
    CHECK(&VMS_EXEC(g_repaint_yield_handoff) == &g_vm_storage.exec.ctx.g_repaint_yield_handoff && inside(VMS_EXEC(g_repaint_yield_handoff)), "exec.g_repaint_yield_handoff single storage");
    CHECK(&VMS_EXEC(g_speed_frame_open) == &g_vm_storage.exec.g_speed_frame_open && inside(VMS_EXEC(g_speed_frame_open)), "exec.g_speed_frame_open single storage");
    CHECK(&VMS_EXEC(g_speed_serial_serviced) == &g_vm_storage.exec.g_speed_serial_serviced && inside(VMS_EXEC(g_speed_serial_serviced)), "exec.g_speed_serial_serviced single storage");
    CHECK(&VMS_EXEC(g_error) == &g_vm_storage.exec.g_error && inside(VMS_EXEC(g_error)), "exec.g_error single storage");
    CHECK(&VMS_EXEC(g_main_thread) == &g_vm_storage.exec.g_main_thread && inside(VMS_EXEC(g_main_thread)), "exec.g_main_thread single storage");
    CHECK(&VMS_EXEC(g_speed_budget_frames) == &g_vm_storage.exec.g_speed_budget_frames && inside(VMS_EXEC(g_speed_budget_frames)), "exec.g_speed_budget_frames single storage");
    CHECK(&VMS_EXEC(g_speed_budget_window_frames) == &g_vm_storage.exec.g_speed_budget_window_frames && inside(VMS_EXEC(g_speed_budget_window_frames)), "exec.g_speed_budget_window_frames single storage");
    CHECK(&VMS_EXEC(g_speed_group_timeouts) == &g_vm_storage.exec.g_speed_group_timeouts && inside(VMS_EXEC(g_speed_group_timeouts)), "exec.g_speed_group_timeouts single storage");
    CHECK(&VMS_EXEC(g_speed_budget_owner) == &g_vm_storage.exec.g_speed_budget_owner && inside(VMS_EXEC(g_speed_budget_owner)), "exec.g_speed_budget_owner single storage");
    CHECK(&VMS_EXEC(g_running_thread) == &g_vm_storage.exec.ctx.g_running_thread && inside(VMS_EXEC(g_running_thread)), "exec.g_running_thread single storage");
    CHECK(&VMS_EXEC(g_threads) == &g_vm_storage.exec.g_threads && inside(VMS_EXEC(g_threads)), "exec.g_threads single storage");
    CHECK(&VMS_EXEC(g_monitors) == &g_vm_storage.exec.g_monitors && inside(VMS_EXEC(g_monitors)), "exec.g_monitors single storage");
    CHECK(&VMS_EXEC(g_can_suspend) == &g_vm_storage.exec.g_can_suspend && inside(VMS_EXEC(g_can_suspend)), "exec.g_can_suspend single storage");
    CHECK(&VMS_EXEC(g_resume_request) == &g_vm_storage.exec.g_resume_request && inside(VMS_EXEC(g_resume_request)), "exec.g_resume_request single storage");
    CHECK(&VMS_EXEC(g_round_robin) == &g_vm_storage.exec.g_round_robin && inside(VMS_EXEC(g_round_robin)), "exec.g_round_robin single storage");
    CHECK(&VMS_EXEC(g_pump_depth) == &g_vm_storage.exec.ctx.g_pump_depth && inside(VMS_EXEC(g_pump_depth)), "exec.g_pump_depth single storage");
    CHECK(&VMS_EXEC(g_farm_startup) == &g_vm_storage.exec.g_farm_startup && inside(VMS_EXEC(g_farm_startup)), "exec.g_farm_startup single storage");
    CHECK(&VMS_EXEC(g_flag_spin_pumps) == &g_vm_storage.exec.g_flag_spin_pumps && inside(VMS_EXEC(g_flag_spin_pumps)), "exec.g_flag_spin_pumps single storage");
    CHECK(&VMS_EXEC(g_active_stack) == &g_vm_storage.exec.ctx.g_active_stack && inside(VMS_EXEC(g_active_stack)), "exec.g_active_stack single storage");
    CHECK(&VMS_EXEC(g_repaint_yield_method) == &g_vm_storage.exec.ctx.g_repaint_yield_method && inside(VMS_EXEC(g_repaint_yield_method)), "exec.g_repaint_yield_method single storage");
    CHECK(&VMS_EXEC(g_repaint_yield_pc) == &g_vm_storage.exec.ctx.g_repaint_yield_pc && inside(VMS_EXEC(g_repaint_yield_pc)), "exec.g_repaint_yield_pc single storage");
    CHECK(&VMS_EXEC(g_exec_ctx) == &g_vm_storage.exec.ctx, "ExecutionContext is one object inside VMStorage.exec");
    CHECK(&vm_state().exec == &g_vm_storage.exec, "facade exec = one reference to VMStorage.exec");
    CHECK(VMS_EXEC(g_slice_ms) == 4 && VMS_EXEC(g_game_clock_mode) == -1 && VMS_EXEC(g_repaint_yield_pc) == -1 && VMS_EXEC(g_fast_float241_enabled) && VMS_EXEC(g_midlet_state) == MidletState::Empty, "initial values preserved (slice_ms=4, clock_mode=-1, yield_pc=-1, float241=true, midlet Empty)");
    CHECK(VMS_EXEC(g_running_thread) == nullptr && !VMS_EXEC(g_yield) && !VMS_EXEC(g_thrown) && VMS_EXEC(g_run_depth) == 0, "context starts empty");
    // Thread switch: A yields, B runs, A resumes and sees its own values (same API the production pump uses).
    JavaThreadContext A, B; auto& ctx = VMS_EXEC(g_exec_ctx);
    std::vector<Frame> stackA, stackB;
    ctx.enter_thread(&A); VMS_EXEC(g_yield) = true; VMS_EXEC(g_thrown) = false; VMS_EXEC(g_active_stack) = &stackA;
    VMS_EXEC(g_run_depth) = 1; VMS_EXEC(g_pump_depth) = 1; VMS_EXEC(g_repaint_owner_ref) = 77; VMS_EXEC(g_repaint_owner_yields) = 1;
    const auto saved = ctx.save_slice();
    ctx.enter_thread(&B);
    CHECK(VMS_EXEC(g_running_thread) == &B && !VMS_EXEC(g_yield) && !VMS_EXEC(g_thrown) && &current_context() == &B, "B runs with a fresh slice (running=B, yield=0, thrown=0)");
    VMS_EXEC(g_thrown) = true; VMS_EXEC(g_yield) = true; VMS_EXEC(g_active_stack) = &stackB; VMS_EXEC(g_repaint_owner_yields) = 2;
    ctx.restore_slice(saved);
    CHECK(VMS_EXEC(g_active_stack) == &stackA, "A gets its own active stack back (B used stackB)");
    CHECK(VMS_EXEC(g_run_depth) == 1 && VMS_EXEC(g_pump_depth) == 1, "run/pump depth are VM-global nesting counters: not touched by the slice switch");
    CHECK(VMS_EXEC(g_repaint_owner_ref) == 77 && VMS_EXEC(g_repaint_owner_yields) == 2, "repaint handshake is frame-scoped (keyed by owner ref): progress made while B ran is kept");
    CHECK(VMS_EXEC(g_running_thread) == &A && VMS_EXEC(g_yield) && !VMS_EXEC(g_thrown) && &current_context() == &A, "A resumes and sees its own values (running=A, yield=1, thrown=0); B's thrown did not leak");
    ctx.restore_slice({nullptr, false, false, nullptr});
    CHECK(&current_context() == &VMS_EXEC(g_main_thread), "no running thread -> main thread context");
    std::printf("EXEC_CONTEXT_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
