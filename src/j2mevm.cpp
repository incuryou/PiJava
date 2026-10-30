/* J2ME VM 0.2.36 runtime composition unit.
 * Portable object model, interpreter, class loading, native API bridge and
 * platform frontends are separate implementation modules. Shared private state
 * is intentionally single-instance; this is not a multiple-VM/concurrent GC API.
 * Build this unit plus puff.c; do not separately compile implementation .inc files.
 */
#include "optimization_config.h"
#include "p7_worker.h"
#include "p7_audio.h"
#include "p7_gc.h"
#include "p7_present.h"
#include "p1_profile.h"
#include "p1_host_turn.h"
#include "native_class_dispatch.h"
#include "frame_buffer_pool.h"
#include "execution_profile.h"
#include "frame_metrics.h"
#include "rgb_fill.h"
#include "decoded_image_cache.h"
#include "font8x8_basic.h"
#include "latin16.h"
#include "logical_screen.h"
#include "canvas_resolution.h"
#include "canvas_declared_size.h"
#include "raster_region.h"
#include "raster_primitives.h"
#include "pixel_compat236.h"
#include "numeric_compat236.h"
#include "native_jit236.h"
#include "presentation_pixels.h"
#include "suite_identity.h"
#include "deterministic_test.h"
#include "rms_catalog.h"
#include "farm_startup.h"
#include "audio_engine.h"
#include "audio_ffmpeg236.h"
#include "jpeg_decoder.h"
#include "frame_schedule.h"
#include "pace.h"
#include "frame_prep.h"
#include "cached_elapsed_gate.h"
#include "cooperative_poll.h"
#include "wait_frame.h"
#include "residual_sleep.h"
#include "rechecked_deadline_gate.h"
#include "compensated_sleep.h"
#include "event_frame_sleep.h"
#include "auto_fps.h"
#include "timer_pacing.h"
#include "poll_gate.h"
#include "time_gate_patterns.h"
#include "gate_recovery.h"
#include <climits>
#ifdef __SWITCH__
#include "switch_audio.h"
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <functional>
#include <iostream>
#include <setjmp.h>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
extern "C" int puff(unsigned char* dest, unsigned long* destlen, const unsigned char* source, unsigned long* sourcelen);

#ifndef J2ME_VERSION
#define J2ME_VERSION "unversioned-test-build"  // real builds always pass -DJ2ME_VERSION from tools/build.py
#endif
static const char* kVer = J2ME_VERSION;  // single source: tools/build.py VERSION (-DJ2ME_VERSION)
 VMS_DIAG_ACCESSOR(g_log_draw, bool)
 VMS_PLAT_ACCESSOR(g_log, std::ofstream) VMS_PLAT_ACCESSOR(audio_pump_previous, int64_t)
 VMS_PLAT_ACCESSOR(g_boot_ms, int64_t)
 VMS_RS_ACCESSOR(g_screen_w, int) VMS_RS_ACCESSOR(g_screen_h, int)
 VMS_RS_ACCESSOR(g_fb, std::vector<uint8_t>)
   VMS_EXEC_ACCESSOR(g_midlet, int) VMS_PLAT_ACCESSOR(g_key_states, int) VMS_RS_ACCESSOR(g_current, int) VMS_RS_ACCESSOR(g_display, int)
enum class MidletState { Empty, Active, Paused, Destroyed };
 VMS_EXEC_ACCESSOR(g_midlet_state, MidletState)
 VMS_EXEC_ACCESSOR(g_notify_destroyed, bool) VMS_EXEC_ACCESSOR(g_last_console_line, std::string) VMS_EXEC_ACCESSOR(g_self_exit, bool) VMS_EXEC_ACCESSOR(g_notify_paused, bool) VMS_EXEC_ACCESSOR(g_midlet_resume, bool)
  VMS_EXEC_ACCESSOR(g_lifecycle_busy, bool) VMS_PLAT_ACCESSOR(g_host_paused, bool)
 VMS_PLAT_ACCESSOR(g_host_frozen236, bool)
 VMS_PLAT_ACCESSOR(g_host_freeze_started236, int64_t)
 VMS_RS_ACCESSOR(g_shown, int) VMS_RS_ACCESSOR(g_alert_return, int)
 VMS_RS_ACCESSOR(g_alert_deadline, int64_t) VMS_RS_ACCESSOR(g_alert_remaining, int64_t)
 VMS_EXEC_ACCESSOR(g_serial_calls, std::deque<int>)
 VMS_PLAT_ACCESSOR(g_pressed_keys, std::unordered_set<int>)
 VMS_PLAT_ACCESSOR(g_command_keys, std::unordered_set<int>)
 VMS_PLAT_ACCESSOR(g_audio, j2me_audio::Engine)
 VMS_NATIVE_ACCESSOR(g_native_jit, jit236::Engine)
 VMS_EXEC_ACCESSOR(g_wake_stats, WakeStats)
 VMS_EXEC_ACCESSOR(g_pace_aligns, uint64_t)
 VMS_EXEC_ACCESSOR(g_slice_ms, int)
 VMS_EXEC_ACCESSOR(g_frame_wait_budget, FrameWaitBudget)
 VMS_EXEC_ACCESSOR(g_frame_work_us, int64_t) VMS_EXEC_ACCESSOR(g_frame_work_finished_us, int64_t)
 VMS_EXEC_ACCESSOR(g_last_slice_us, int64_t)
 VMS_PLAT_ACCESSOR(g_audio_event_window, int64_t)
 VMS_PLAT_ACCESSOR(g_audio_event_count, unsigned)
 VMS_PLAT_ACCESSOR(g_audio_event_suppressed, uint64_t)
 VMS_PLAT_ACCESSOR(g_tones, std::unordered_set<int>)
#ifdef __SWITCH__
 VMS_PLAT_ACCESSOR(g_audio_sink, j2me_audio::SwitchAudioSink)
#endif
static void audio_pump();
static bool p7_pending_ends();
static bool p7_audio_paused();
static bool p7_poll_end(int& handle);
static void p7_audio_snapshot_if_needed(uint64_t frames);
static void audio_suspend(bool paused);
static void audio_reset();
static void host_events(bool callbacks=true);
static void display_visibility();
static void lifecycle_pause(bool host);
static void lifecycle_resume();
static void lifecycle_destroy(bool notify_only = false);
  VMS_EXEC_ACCESSOR(g_yield, bool) VMS_EXEC_ACCESSOR(g_thread_live, bool) VMS_EXEC_ACCESSOR(g_thrown, bool) VMS_RS_ACCESSOR(g_repaint, bool) VMS_RS_ACCESSOR(g_fb_dirty, bool)
      VMS_HEAP_ACCESSOR(g_gc_logs, int) VMS_HEAP_ACCESSOR(g_alloc_since, int) VMS_EXEC_ACCESSOR(g_run_depth, int) VMS_EXEC_ACCESSOR(g_slices, int) VMS_EXEC_ACCESSOR(g_cuts, int) VMS_PLAT_ACCESSOR(g_log_pending, int) VMS_RS_ACCESSOR(g_paint_serial, int) VMS_RS_ACCESSOR(g_game_frames, int) VMS_RS_ACCESSOR(g_present_frames, uint64_t)
#include "paint_stats.h"
#include "graphics_profile.h"
#include "p86_gfx.h"
#include "runtime_profile.h"
 VMS_RS_ACCESSOR(g_paint_stats, CanvasPaintStats)
 VMS_RS_ACCESSOR(g_graphics_profile, GraphicsProfile)
 VMS_EXEC_ACCESSOR(g_runtime_profile, RuntimeProfile)
 VMS_EXEC_ACCESSOR(g_execution_profile, ExecutionProfile)
 VMS_RS_ACCESSOR(g_frame_metrics, FrameMetrics)
 VMS_PLAT_ACCESSOR(g_cpu_window_start_ns, int64_t)
 VMS_HEAP_ACCESSOR(g_gc_mark_scratch, std::vector<char>)
 VMS_HEAP_ACCESSOR(g_gc_work_scratch, std::vector<int>)
static void runtime_early_repaint_handoff() {++VMS_EXEC(g_runtime_profile).pending.early_repaint_handoffs;}
static void reset_runtime_stats();
static uint64_t g_stat_present_seen=0;   // 0.2.113 F4: g_present_frames at the start of the current STAT window
static uint64_t g_stat_jit_ops_seen=0;   // Diagnostic window baseline, never serialized.
 VMS_PLAT_ACCESSOR(g_present_stats, PresentStats)
#if defined(__SWITCH__) && defined(J2ME_ENABLE_DEKO3D)
 VMS_PLAT_ACCESSOR(g_gpu_active, bool) // Renderer diagnostics precede the host module.
#endif
 VMS_PLAT_ACCESSOR(g_last_present_us, int64_t)
static int64_t paint_clock_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
 VMS_EXEC_ACCESSOR(g_speed_cap, int)
 VMS_EXEC_ACCESSOR(g_poll_calls, uint64_t) VMS_EXEC_ACCESSOR(g_poll_gate_reads, uint64_t) VMS_EXEC_ACCESSOR(g_poll_waits, uint64_t) VMS_EXEC_ACCESSOR(g_poll_wait_us, uint64_t)
 VMS_EXEC_ACCESSOR(g_game_clock, GameClock)
 VMS_EXEC_ACCESSOR(g_game_clock_mode, int)
// A prompt that polls currentTimeMillis drops keys when one real frame is
// several game steps. Hold the clock at 1x after a key so that window reopens.
 VMS_EXEC_ACCESSOR(g_clock_input_hold_us, int64_t)
VMS_RS_ACCESSOR(g_frame_sig, uint32_t)
static void mix_frame_sig(uint32_t v) { VMS_RS(g_frame_sig) ^= v; VMS_RS(g_frame_sig) *= 16777619u; }
// Fresh Native launches keep the wall clock. Capped or restored games retain
// their Java epoch when returning to Native, and continue at real-time rate.
 VMS_EXEC_ACCESSOR(g_game_clock_inherited, bool)
 VMS_EXEC_ACCESSOR(g_speed_last_paint_us, int64_t)
 VMS_EXEC_ACCESSOR(g_speed_next_us, int64_t)
 VMS_EXEC_ACCESSOR(g_speed_slept_us, int64_t)
 VMS_EXEC_ACCESSOR(g_speed_cut_us, int64_t)
 VMS_EXEC_ACCESSOR(g_speed_add_us, int64_t)
// The requester of a deferred Canvas repaint; zero denotes the host context.
 VMS_EXEC_ACCESSOR(g_repaint_owner_ref, int)
 VMS_EXEC_ACCESSOR(g_repaint_yield_canvas, int) VMS_EXEC_ACCESSOR(g_repaint_owner_yields, int)
 VMS_EXEC_ACCESSOR(g_repaint_yield_handoff, bool)
// One canvas paint per host frame while a cap is active. A second
// serviceRepaints inside the same 14ms slice is what kept zxx near 40 at a 30 cap.
 VMS_EXEC_ACCESSOR(g_speed_frame_open, bool)
 VMS_EXEC_ACCESSOR(g_speed_serial_serviced, bool)
#include "completed_paint241.h"
static int64_t speed_frame_wake_us(int64_t requested_ms);
struct JavaThreadContext; static int64_t speed_timer_wait_ms(int queue_ref,int64_t timeout_ms,JavaThreadContext& t);
struct Frame;
static bool speed_frame_wait_clock(const Frame& fr,int64_t now_ms);
struct QuickSite;
static int speed_rechecked_branch(Frame& fr,int pc,int remainder);
static void speed_rechecked_clock(const Frame& fr,int64_t now,QuickSite& site);
static int64_t speed_rechecked_sleep_wake_us(int64_t requested_ms);
static bool speed_residual_clock(const Frame& fr,int64_t now_ms);
static void speed_compensated_clock(const Frame&,int64_t);
static void speed_compensated_sleep_begin(int64_t);
static void speed_compensated_sleep_retimed(int64_t,int64_t);
static int64_t speed_compensated_subtract(const Frame&,int64_t,int64_t,int64_t);
static void speed_event_frame_clock(Frame&,int64_t,QuickSite&);
static int64_t speed_event_frame_sleep_wake_us(int64_t);
static int64_t speed_residual_sleep_wake_us(int64_t requested_ms);
static int64_t speed_frame_wait_wake_us(int ref,int64_t requested_ms,JavaThreadContext& t);
static void speed_frame_wait_repaint(int canvas);
static void speed_frame_wait_cancel_request(JavaThreadContext& owner);
static void speed_timer_block(JavaThreadContext& t);
// Window-only diagnostics; deliberately excluded from instant states.
struct FrameSleepStats { uint64_t short_calls=0, long_calls=0, owned=0, unowned=0, requested_ms=0, effective_ms=0, timer_paced=0, timer_unpaced=0, timer_scaled=0; };   // 0.2.113: timer_* = java.util.Timer queue waits
static FrameSleepStats g_frame_sleep_stats;
// P8.4 (P8-BUG-01): after an instant-state restore, record the first Java Thread.sleep requests
// (requested vs effective ms) once, so Ryujinx logs show the game's own frame period. Log only.
struct P84SleepLog { int left=0; std::vector<int64_t> req, eff; };
static P84SleepLog g_p84_sleep;
static void log_line(const std::string&);
static void p84_note_sleep(int64_t req, int64_t eff) {
    g_frame_sleep_stats.requested_ms+=uint64_t(req);
    g_frame_sleep_stats.effective_ms+=uint64_t(eff);
    if (g_p84_sleep.left <= 0) return;
    g_p84_sleep.req.push_back(req); g_p84_sleep.eff.push_back(eff);
    if (--g_p84_sleep.left) return;
    auto med=[](std::vector<int64_t> v){std::sort(v.begin(),v.end());return v.empty()?int64_t(0):v[v.size()/2];};
    auto mn=[](const std::vector<int64_t>& v){return v.empty()?int64_t(0):*std::min_element(v.begin(),v.end());};
    auto mx=[](const std::vector<int64_t>& v){return v.empty()?int64_t(0):*std::max_element(v.begin(),v.end());};
    log_line("SPEEDRESTORE sleeps n="+std::to_string(g_p84_sleep.req.size())+" req_ms_min="+std::to_string(mn(g_p84_sleep.req))+
             " req_ms_median="+std::to_string(med(g_p84_sleep.req))+" req_ms_max="+std::to_string(mx(g_p84_sleep.req))+
             " eff_ms_median="+std::to_string(med(g_p84_sleep.eff))+" eff_ms_max="+std::to_string(mx(g_p84_sleep.eff)));
    g_p84_sleep = {};
}
// Array payload only: excludes objects, strings, images and allocator metadata.
 VMS_HEAP_ACCESSOR(g_array_payload_live, uint64_t) VMS_HEAP_ACCESSOR(g_array_payload_peak, uint64_t) VMS_HEAP_ACCESSOR(g_array_alloc_since_gc, uint64_t)
 VMS_HEAP_ACCESSOR(g_gc_array_refs_scanned, uint64_t) VMS_HEAP_ACCESSOR(g_gc_cycles, uint64_t)
 VMS_EXEC_ACCESSOR(g_error, std::string)
static jmp_buf g_die_jmp;
static bool g_die_catch = false;
 VMS_CL_ACCESSOR(g_jar_files, std::unordered_map<std::string, std::vector<uint8_t>>)
 VMS_CL_ACCESSOR(g_props, std::unordered_map<std::string, std::string>)
 VMS_RS_ACCESSOR(g_decoded_images, DecodedImageCache)
 VMS_RS_ACCESSOR(g_png_fast, uint64_t) VMS_RS_ACCESSOR(g_png_fallback, uint64_t)
 VMS_RS_ACCESSOR(g_image_decode_us, uint64_t) VMS_RS_ACCESSOR(g_image_copy_us, uint64_t)
 VMS_PLAT_ACCESSOR(g_rms_suite, std::string)
 VMS_PLAT_ACCESSOR(g_rms_import_legacy, bool)
 VMS_RS_ACCESSOR(g_offscreen, std::unordered_map<int, int>)

#include "runtime/value.h"
#include "array_storage.h"
VMS_HEAP_ACCESSOR(g_primitive_buffers, PrimitiveBufferPool)

#include "runtime/object_model.inc"

struct Clip { int x = 0, y = 0, w = 10000, h = 10000; bool on = false; };
struct DrawAcc {
    bool on = false;
    int rects = 0, lines = 0, images = 0, clips = 0;
    int minx = 10000, miny = 10000, maxx = 0, maxy = 0;
    std::string textblob, sample;
};

 VMS_CL_ACCESSOR(g_classes, std::deque<Class>)
 VMS_HEAP_ACCESSOR(g_heap, std::deque<Obj>)
 VMS_HEAP_ACCESSOR(g_free, std::vector<int>)
 VMS_CL_ACCESSOR(g_class_index, std::unordered_map<std::string, int>)
 VMS_CL_ACCESSOR(g_classpath, std::vector<std::string>)
 VMS_RS_ACCESSOR(g_gfx_color, std::unordered_map<int, int>) VMS_RS_ACCESSOR(g_gfx_target, std::unordered_map<int, int>) VMS_RS_ACCESSOR(g_gfx_tx, std::unordered_map<int, int>) VMS_RS_ACCESSOR(g_gfx_ty, std::unordered_map<int, int>)
 VMS_RS_ACCESSOR(g_clip, std::unordered_map<int, Clip>)
enum class ThreadState { Runnable, Sleeping, Blocked, Waiting, Joining, Initializing, Terminated };
struct JavaThreadContext {
    int ref = 0;
    Method* p1_last_method=nullptr,*p1_throw_method=nullptr;
    int p1_throw_bci=-1;size_t p1_throw_frames=0;
    int p1_last_bci=-1,p1_end_reason=0;
    size_t p1_last_frames=0;
    std::vector<Frame> frames;
    ThreadState state = ThreadState::Runnable;
    int64_t wake = 0, sleep_sample_due = 0, wake_us = 0;
    // Next frame deadline earned by this thread's last completed drawing.
    // It survives cooperative slices and is consumed by one eligible sleep.
    int64_t frame_due_us = 0;
    TimerFramePacing timer;
    FramePrepBudget frame_prep;
    ResidualSleepState residual_sleep;
    CompensatedSleepState compensated_sleep;
    EventFrameSleepState event_frame_sleep;
    uint64_t event_frame_generation=0; // transient cycle identity; never a Java/save field
    RecheckedDeadlineState rechecked_deadline;
    FrameWaitState wait_frame;
    bool sleep_paced = false;
    bool frame_sleep_bridge = false, mixed_wait = false;
    int64_t frame_group_started_us = 0;
    AutoFpsLearner auto_fps;
    PollGateState poll_gate;
    NativeDeadlineEvidence native_deadline;
    uint64_t native_deadline_generation = 0;
    CachedElapsedState cached_elapsed;
    FieldGateAnchor field_gate_anchor;
    GateRecovery gate_recovery;
    uint64_t owned_draws = 0;
    int sleep_request = 0;
    int monitor = 0, restore_depth = 0, join_target = 0, unhandled = 0, init_wait = -1;
    bool reacquire = false, interrupt_exception = false, executing = false, begun = false;
    bool service_repaints_deferred = false; // pending original invoke, never a saved callback
};
static JavaThreadContext* g_timer_clock_owner=nullptr;
struct Monitor { JavaThreadContext* owner = nullptr; int depth = 0; std::deque<JavaThreadContext*> waiters; };
#include "exec_context.h"
 VMS_EXEC_ACCESSOR(g_main_thread, JavaThreadContext)
// Raw drawing callbacks remain VMS_RS(g_game_frames). Since 0.2.112 (F4) the on-screen FPS badge
// counts in-game present() executions (VMS_RS(g_present_frames)), not paint callbacks or budget slots.
 VMS_EXEC_ACCESSOR(g_speed_budget_frames, uint64_t) VMS_EXEC_ACCESSOR(g_speed_budget_window_frames, uint64_t) VMS_EXEC_ACCESSOR(g_speed_group_timeouts, uint64_t)
 VMS_EXEC_ACCESSOR(g_speed_budget_owner, JavaThreadContext*)
 VMS_EXEC_ACCESSOR(g_running_thread, JavaThreadContext*)
 VMS_EXEC_ACCESSOR(g_threads, std::list<JavaThreadContext>)
 VMS_EXEC_ACCESSOR(g_monitors, std::unordered_map<int, Monitor>)
 VMS_CL_ACCESSOR(g_class_objects, std::unordered_map<std::string, int>)
 VMS_CL_ACCESSOR(g_literal_cache, LiteralCache)
 VMS_HEAP_ACCESSOR(g_root_stacks, std::vector<std::pair<std::vector<Frame>*, JavaThreadContext*>>)
 VMS_EXEC_ACCESSOR(g_resume_request, bool) VMS_EXEC_ACCESSOR(g_can_suspend, bool)
 VMS_EXEC_ACCESSOR(g_round_robin, size_t)
 VMS_EXEC_ACCESSOR(g_pump_depth, int)
 VMS_RS_ACCESSOR(g_paint_depth, int)
 VMS_EXEC_ACCESSOR(g_farm_startup, FarmStartup)
 VMS_EXEC_ACCESSOR(g_flag_spin_pumps, uint64_t)
// P2.1-c4 (C10, J2S5): VM-owned logical clock for savestates. exec_epoch counts every Java execution
// (RunScope) and scheduler turn. While nothing has executed since a restore, logical "now" stays at the
// restore instant, so save -> load -> save writes the same deadlines (stored relative to logical now).
// Runtime scheduling still uses the real clocks: game timing is unchanged.
struct SaveClockAnchor { uint64_t exec_epoch = 0, anchor_epoch = 0; bool valid = false; int64_t ms = 0, us = 0; };
 VMS_EXEC_ACCESSOR(g_save_clock, SaveClockAnchor)
static void dispatch_key(int code, bool release);
static void repaint_current();
static JavaThreadContext& current_context() { return VMS_EXEC(g_running_thread) ? *VMS_EXEC(g_running_thread) : VMS_EXEC(g_main_thread); }
static void release_frame_monitor(Frame& fr);
static void complete_class_initialization(Frame& fr, bool success, int exception = 0);
static int class_object(const std::string& name);
static bool monitor_enter(int ref);
static void monitor_exit(int ref);
static void suspend_current(bool run_others = false);
static void deliver_interrupt();
static bool pump_one_thread();
static void refresh_thread(JavaThreadContext& t);
static void finish_thread(JavaThreadContext& t);
static int speed_period_us();
static bool speed_ahead();
static void speed_cached_elapsed_clock(const Frame&,int64_t,QuickSite&);
static bool speed_cached_elapsed_branch(Frame&,int,int);
static int64_t speed_cached_elapsed_sleep_wake_us(int64_t);
static void cooperative_poll_clock(Frame&,int64_t&,QuickSite&,bool,int64_t);
static int64_t speed_begin_frame(JavaThreadContext* owner = nullptr);
static bool speed_group_helper(JavaThreadContext* owner);
static bool speed_pending_group();
static bool speed_group_incomplete();
static void speed_note_paint(int64_t started_us, JavaThreadContext* owner = nullptr);

 VMS_EXEC_ACCESSOR(g_active_stack, std::vector<Frame>*)
 VMS_EXEC_ACCESSOR(g_repaint_yield_method, Method*)
 VMS_EXEC_ACCESSOR(g_repaint_yield_pc, int)
 VMS_RS_ACCESSOR(g_drawacc, DrawAcc) VMS_RS_ACCESSOR(draw_end_prev, std::string)

static int64_t monotonic_ms() {
    if(r4::state.active)return r4::state.us()/1000;
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
static void speed_reset_learning() {
    g_timer_clock_owner=nullptr;
    VMS_EXEC(g_exec_ctx).frame_wait_request_owner=0;
    VMS_EXEC(g_repaint_owner_yields)=0;VMS_EXEC(g_repaint_yield_handoff)=false;
    VMS_EXEC(g_repaint_yield_method)=nullptr;VMS_EXEC(g_repaint_yield_pc)=-1;
    VMS_EXEC(g_speed_budget_owner)=nullptr;
    VMS_EXEC(g_main_thread).auto_fps.reset(); VMS_EXEC(g_main_thread).gate_recovery.reset(); VMS_EXEC(g_main_thread).poll_gate.reset(); VMS_EXEC(g_main_thread).field_gate_anchor.reset(); VMS_EXEC(g_main_thread).owned_draws=0;
    VMS_EXEC(g_main_thread).native_deadline.reset();
    VMS_EXEC(g_main_thread).frame_due_us=0; VMS_EXEC(g_main_thread).timer={}; VMS_EXEC(g_main_thread).sleep_paced=false; VMS_EXEC(g_main_thread).frame_sleep_bridge=VMS_EXEC(g_main_thread).mixed_wait=false; VMS_EXEC(g_main_thread).frame_group_started_us=0;
    VMS_EXEC(g_main_thread).event_frame_sleep.reset();
    VMS_EXEC(g_main_thread).compensated_sleep.reset();VMS_EXEC(g_main_thread).cached_elapsed.reset();VMS_EXEC(g_main_thread).frame_prep.reset();VMS_EXEC(g_main_thread).wait_frame.reset();VMS_EXEC(g_main_thread).residual_sleep.reset();VMS_EXEC(g_main_thread).rechecked_deadline.reset();
    for(auto& t:VMS_EXEC(g_threads)) {
        t.auto_fps.reset(); t.gate_recovery.reset(); t.poll_gate.reset(); t.field_gate_anchor.reset(); t.owned_draws=0; t.frame_due_us=0; t.timer={}; t.sleep_paced=false; t.frame_sleep_bridge=t.mixed_wait=false; t.frame_group_started_us=0;
        t.native_deadline.reset();
        t.event_frame_sleep.reset();
        t.compensated_sleep.reset();t.cached_elapsed.reset();t.frame_prep.reset();t.wait_frame.reset();t.residual_sleep.reset();t.rechecked_deadline.reset();
    }
}
static void speed_sync_clock() {
    if(VMS_EXEC(g_game_clock_mode)==VMS_EXEC(g_speed_cap))return;
    const int64_t now=paint_clock_us();
    if(!VMS_EXEC(g_game_clock).initialized || !VMS_EXEC(g_game_clock_inherited))VMS_EXEC(g_game_clock).start(now,now_ms());
    else VMS_EXEC(g_game_clock).rate(now,1.0);
    if(VMS_EXEC(g_speed_cap)!=0)VMS_EXEC(g_game_clock_inherited)=true;
    VMS_EXEC(g_game_clock_mode)=VMS_EXEC(g_speed_cap);
    speed_reset_learning();
}
static int64_t game_current_ms() {
    if(r4::state.active)return r4::state.wall_ms();
    speed_sync_clock();
    if(!speed_period_us() && !VMS_EXEC(g_game_clock_inherited))return now_ms();
    const int64_t game_us=VMS_EXEC(g_game_clock).value_us(paint_clock_us());
    if(speed_period_us() && VMS_RS(g_paint_depth)==0)current_context().auto_fps.poll(game_us);
    return game_us/1000;
}
static void speed_pause_clock(bool paused) {
    speed_sync_clock();
    speed_reset_learning();
    const int64_t now=paint_clock_us();
    // Reanchor continuously while discarding the rate's obsolete evidence.
    // Resume learns the current scene again instead of inheriting a boost.
    VMS_EXEC(g_game_clock).rate(now,1.0);
    VMS_EXEC(g_game_clock).pause(now,paused && (speed_period_us() || VMS_EXEC(g_game_clock_inherited)));
}
// Losing the current gate also loses the evidence for its global boost.
// Keep Java time continuous, and leave other owners and legacy learners alone.
static void speed_invalidate_group(JavaThreadContext& owner,bool transient_timeout=false) {
    owner.frame_sleep_bridge=false;
    const bool authority=owner.auto_fps.verified_gate_seen && &owner==VMS_EXEC(g_speed_budget_owner);
    if(J2ME_OPT_GATE_RECOVERY && transient_timeout && authority)
        owner.gate_recovery.remember(owner.auto_fps,speed_period_us(),VMS_EXEC(g_game_clock).scale,paint_clock_us());
    else owner.gate_recovery.reset();
    owner.auto_fps.invalidate_gate();owner.poll_gate.reset();owner.frame_group_started_us=0;
    if(authority && speed_period_us()) {
        if(!g_timer_clock_owner)VMS_EXEC(g_game_clock).rate(paint_clock_us(),1.0);
        VMS_EXEC(g_speed_budget_owner)=nullptr;
    }
}
// P8.7: async durable log (see async_log.inc). Main thread enqueues; writer thread flushes in batches.
#include "runtime_diagnostics.h"
#include "causal_diagnostics.h"
struct DeviceDiagnostics {
    runtime_diag::Flight flight;runtime_diag::Labels labels;runtime_diag::Live live;
    runtime_diag::LiveSnapshot owner;runtime_diag::WindowDetector detector;runtime_diag::IncidentGate gate;
    runtime_diag::AudioLive audio;
    runtime_cause::Allocations allocations;runtime_cause::Sleeps sleeps;
    uint64_t sleep_reported=0,jit_creates_seen=0,jit_failures_seen=0,jit_create_us_seen=0;
    uint64_t incidents=0,throw_count=0,caught_count=0,windows=0;
    int64_t last_point_note=0;bool reporting=false;
};
static DeviceDiagnostics g_device_diag;
struct DiagnosticNativeToken {runtime_diag::LiveSnapshot previous;int64_t began=0;uint32_t method=0;runtime_diag::Phase phase=runtime_diag::Phase::Native;};
static void diag_watchdog_tick();
static void diag_fault(const char* reason,const char* detail,bool force=true);
static void diag_exception(int ref);
static void diag_caught(const Frame&,int handler);
static void diag_host(bool active,bool paused);
static void diag_checkpoint(const std::vector<Frame>&);
static void diag_launch(const std::string&,const std::string&);
static DiagnosticNativeToken diag_native_enter(Method*);
static DiagnosticNativeToken diag_phase_enter(runtime_diag::Phase);
static void diag_native_leave(const DiagnosticNativeToken&);
static void diag_window(int64_t,const PaintStatsWindow&,const RuntimeProfileWindow&,uint64_t);
static void diag_alloc(const std::string&,size_t,uint64_t);
static void diag_sleep_start(JavaThreadContext&,int64_t,int64_t,runtime_cause::Route,bool);
static void diag_sleep_ready(JavaThreadContext&);
static void diag_sleep_dispatch(JavaThreadContext&);
static void diag_sleep_cancel(JavaThreadContext&,runtime_cause::CancelReason reason=runtime_cause::CancelReason::Manual);
static void diag_causal_reset();
static void diag_causal_window(int64_t);
static void diag_causal_snapshot();
#include "async_log.inc"
static void log_open(const char* path,bool truncate){
    j2melog::shutdown(); // join previous writer if any (suite relaunch)
    if(VMS_PLAT(g_log).is_open())VMS_PLAT(g_log).close();
    VMS_PLAT(g_log).clear();
    j2melog::g_path=path; j2melog::g_bytes=0; j2melog::g_shed=0; j2melog::g_shed_reported=0; j2melog::g_drops=0; j2melog::g_drops_reported=0;
    j2melog::g_main_us.store(0,std::memory_order_relaxed);
    j2melog::g_io_uncertain=false;j2melog::g_key_drops=0;j2melog::g_hf_drops=0;j2melog::g_fault_drops=0;
    g_device_diag.owner.session=uint64_t(paint_clock_us());
    g_device_diag.owner.active=false;g_device_diag.live.publish(g_device_diag.owner);
    j2melog::g_tick=&diag_watchdog_tick;
    VMS_PLAT(g_log).open(path,truncate?std::ios::out|std::ios::trunc:std::ios::out|std::ios::app);
    j2melog::g_fault_at=0;
    if(const char* f=std::getenv("J2ME_LOG_FAULT_AT"))j2melog::g_fault_at=std::strtoull(f,nullptr,10);
    j2melog::g_sync=j2melog::sync_requested();
    {
        std::string cfg="LOGASYNC schema=1 sync="+(j2melog::g_sync?std::string("1"):std::string("0"))+" queue_cap="+std::to_string(j2melog::kQueueCap)+" flush=batch_or_1s";
#ifdef __SWITCH__
        cfg+=" cerr=0 writer_core=2";
#else
        cfg+=" cerr=1";
#endif
        // direct write before enqueue path is hot (and so BOOT ordering stays obvious)
        j2melog::write_raw("["+std::to_string(VMS_PLAT(g_boot_ms)?now_ms()-VMS_PLAT(g_boot_ms):0)+"] "+cfg,true);
    }
    j2melog::start_writer();
}
static void log_line(const std::string& m) {
    const int64_t t0=paint_clock_us();
    int64_t t = VMS_PLAT(g_boot_ms) ? now_ms() - VMS_PLAT(g_boot_ms) : 0;
    std::string line = "[" + std::to_string(t) + "] " + m;
#ifndef __SWITCH__
    std::cerr << line << "\n";
#endif
    if (j2melog::enabled()) {
        const bool is_key = j2melog::key(m);
        if (!is_key && j2melog::g_bytes >= j2melog::kSoftBudget) { ++j2melog::g_shed; j2melog::g_main_us.fetch_add(uint64_t(std::max<int64_t>(0,paint_clock_us()-t0)),std::memory_order_relaxed); return; }
        if (is_key && j2melog::g_shed != j2melog::g_shed_reported) {
            j2melog::enqueue("[" + std::to_string(t) + "] LOGBUDGET shed_high_frequency=" + std::to_string(j2melog::g_shed) + " budget_bytes=" + std::to_string(j2melog::kSoftBudget), true);
            j2melog::g_shed_reported = j2melog::g_shed;
        }
        if (is_key && j2melog::g_drops != j2melog::g_drops_reported) {
            j2melog::enqueue("[" + std::to_string(t) + "] LOGDROP queue_shed=" + std::to_string(j2melog::g_drops) + " key_lost="+std::to_string(j2melog::g_key_drops)+" hf_lost="+std::to_string(j2melog::g_hf_drops)+" fault_lost="+std::to_string(j2melog::g_fault_drops)+" cap=" + std::to_string(j2melog::kQueueCap), true);
            j2melog::g_drops_reported = j2melog::g_drops;
        }
        j2melog::enqueue(std::move(line), is_key);
    }
    j2melog::g_main_us.fetch_add(uint64_t(std::max<int64_t>(0,paint_clock_us()-t0)),std::memory_order_relaxed);
}
static void log_drain(){
    if(j2melog::drain())return;
    log_line("LOGDRAIN result=timeout_or_uncertain io_uncertain="+std::to_string(j2melog::g_io_uncertain.load())+" io_failures="+std::to_string(j2melog::g_failures.load())+" key_lost="+std::to_string(j2melog::g_key_drops.load()));
}
static void log_shutdown(){ j2melog::shutdown(); }
// Startup-only breadcrumbs also reach the host debugger independently of SD
// logging, so a stalled logger cannot hide an audio/JIT/reset boundary.
static void startup_trace(const char* phase) {
    char line[160];
    const int count=std::snprintf(line,sizeof(line),"LAUNCHTRACE schema=1 revision=FrameLogicFix-r2 phase=%s",phase);
    if(count<=0)return;
#ifdef __SWITCH__
    (void)svcOutputDebugString(line,std::min<size_t>(size_t(count),sizeof(line)-1));
#endif
    log_line(line);
}
static void city_diag_launch(const std::string& identity);
static void city_diag_report();
#include "p2s0_trace.inc"
#include "p1_diagnostics.inc"
#include "p1_city_diagnostics.inc"
static void speed_certified_gate(JavaThreadContext& context,Method* method,int pc,int64_t previous,int64_t millis) {
    const int period=speed_period_us();
    if(!period)return;
    // A sleep-only scene correctly discards the old learner. Fresh waiting
    // followed by ready, after this owner's completed drawing, can start it
    // again. The cached certification alone cannot resurrect an old boost.
    const int64_t now=paint_clock_us();
    if(!context.auto_fps.started && millis>=8 && millis<=120 &&
       &context==VMS_EXEC(g_speed_budget_owner) && context.owned_draws>0 &&
       VMS_EXEC(g_speed_last_paint_us)>0 && now>=VMS_EXEC(g_speed_last_paint_us) &&
       now-VMS_EXEC(g_speed_last_paint_us)<=120000 && context.poll_gate.fresh_ready &&
       context.poll_gate.method==method && context.poll_gate.pc==pc && context.poll_gate.previous_ms==previous) {
        context.auto_fps.reset();context.gate_recovery.reset();
        context.auto_fps.started=true;context.auto_fps.cycle_game_us=VMS_EXEC(g_game_clock).value_us(now);
    }
    if(context.auto_fps.gate_period(millis*1000,previous,method,pc) && J2ME_OPT_GATE_RECOVERY &&
       context.gate_recovery.restore(context.auto_fps,method,pc,millis*1000,period,paint_clock_us()))
        log_line("GATERECOVER owner="+std::to_string(context.ref)+" method="+method->name+
            " pc="+std::to_string(pc)+" period_ms="+std::to_string(millis)+" proof=fresh_certified_same_gate");
}
static void audio_event(const std::string& message) {
    const int64_t now=monotonic_ms();
    g_device_diag.flight.note(paint_clock_us(),"audio",message.c_str());
    if(now-VMS_PLAT(g_audio_event_window)>=1000) {VMS_PLAT(g_audio_event_window)=now;VMS_PLAT(g_audio_event_count)=0;}
    if(VMS_PLAT(g_audio_event_count)++<24)log_line("AUDIO "+message);
    else ++VMS_PLAT(g_audio_event_suppressed);
}
[[noreturn]] static void r4_abort(const char* reason) {
    std::fprintf(stderr,"R4 failure %s tick=%lld\n",reason,(long long)r4::state.tick);
    p7audio::fence();p7::pool().shutdown();r4::finish("INCOMPLETE",reason);std::exit(70);
}
static void die(const std::string& m) {
    diag_fault("VM_FATAL",m.c_str()); // Before longjmp, cleanup or frame destruction.
    if(r4::state.active){log_line("EX "+m);log_drain();r4_abort("VM_FATAL");}
    VMS_EXEC(g_error) = m;
    log_line("EX " + m);
    if (g_die_catch) longjmp(g_die_jmp, 1);
    std::exit(1);
}
static uint16_t ru16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t ru32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int16_t rs16(const uint8_t* p) { return (int16_t)ru16(p); }
static uint16_t ru16le(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t ru32le(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static int ensure_class(const std::string& name);
static int load_class_file(const std::string& name, bool fatal = true);
static Value run(Method* entry, const std::vector<Value>& args);
static Value run_event_callback(Method* entry, const std::vector<Value>& args) {
    struct EventScope {
        JavaThreadContext* previous;
        EventScope():previous(VMS_EXEC(g_exec_ctx).event_callback_owner) {
            VMS_EXEC(g_exec_ctx).event_callback_owner=&current_context();
        }
        ~EventScope(){VMS_EXEC(g_exec_ctx).event_callback_owner=previous;}
    } event;
    return run(entry,args);
}
static void throw_ref(int ref);
static int alloc_obj(int cls);
static bool read_whole(const std::string& path, std::vector<uint8_t>& out);
static int make_string(const std::string& s);
static int literal_string(const std::string& s, int candidate = 0);
static constexpr const char* kLiteralRootsClass = "j2me/internal/StringLiterals";
static int utf8_next(const std::string& s, size_t& i) {
    if (i >= s.size()) return -1;
    unsigned char c = (unsigned char)s[i];
    if ((c & 0x80) == 0) { i += 1; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        int cp = ((c & 31) << 6) | ((unsigned char)s[i + 1] & 63);
        i += 2; return cp;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        int cp = ((c & 15) << 12) | (((unsigned char)s[i + 1] & 63) << 6) | ((unsigned char)s[i + 2] & 63);
        i += 3; return cp;
    }
    i += 1; return c;
}
// Skip ASCII runs without changing the existing UTF-8/CESU-8 interpretation.
// memcpy avoids alignment and strict-aliasing assumptions on both ARM64 and x86.
#if J2ME_OPT_ASCII_SCAN
static inline size_t utf8_ascii_span(const std::string& s,size_t at,size_t limit) {
    const size_t end=at+std::min(limit,s.size()-at);
    size_t i=at;
    while(end-i>=sizeof(uint64_t)) {
        uint64_t word;std::memcpy(&word,s.data()+i,sizeof(word));
        if(word & UINT64_C(0x8080808080808080))break;
        i+=sizeof(word);
    }
    while(i<end && !(static_cast<unsigned char>(s[i])&0x80))++i;
    return i-at;
}
#endif
static int utf8_len(const std::string& s) {
    int n = 0; size_t i = 0;
    while (i < s.size()) {
#if J2ME_OPT_ASCII_SCAN
        if(!(static_cast<unsigned char>(s[i])&0x80)) {
            const size_t ascii=utf8_ascii_span(s,i,s.size()-i);i+=ascii;n+=int(ascii);continue;
        }
#endif
        utf8_next(s, i); n++;
    }
    return n;
}
static size_t utf8_byte(const std::string& s, int char_index) {
    size_t i = 0; int n = 0;
    while (i < s.size() && n < char_index) {
#if J2ME_OPT_ASCII_SCAN
        if(!(static_cast<unsigned char>(s[i])&0x80)) {
            const size_t ascii=utf8_ascii_span(s,i,size_t(char_index-n));i+=ascii;n+=int(ascii);continue;
        }
#endif
        utf8_next(s, i); n++;
    }
    return i;
}
static int utf8_at(const std::string& s, int char_index) {
    size_t i = utf8_byte(s, char_index);
    if (i >= s.size()) return -1;
    return utf8_next(s, i);
}
static void utf8_append(std::string& s, int cp) {
    cp &= 0xFFFF;
    if (cp < 0x80) s.push_back((char)cp);
    else if (cp < 0x800) {
        s.push_back((char)(0xC0 | (cp >> 6)));
        s.push_back((char)(0x80 | (cp & 63)));
    } else {
        s.push_back((char)(0xE0 | (cp >> 12)));
        s.push_back((char)(0x80 | ((cp >> 6) & 63)));
        s.push_back((char)(0x80 | (cp & 63)));
    }
}
// Font and Graphics carry immutable font values in the existing serialized text
// payload, never Java handles. Empty pre-R3 payloads retain the medium default.
struct MidpFont {
    int face=0,style=0,size=0;
    int pixels() const { return size==8?12:size==16?20:16; }
    int height() const { return pixels()+(size?2:0); }
    int baseline() const { return pixels()*3/4; }
    int padding() const { return ((style&1)?1:0)+((style&2)?(pixels()-1)/4:0); }
    int cell(int cp) const { return (face==32||cp>=128)?pixels():pixels()/2; }
    int advance(int cp) const {
        // Small system/proportional Latin cells are 6px wide. Keeping the
        // 2px CJK leading here made them as wide as the 8px medium font and
        // overflowed compact handset layouts. Monospace retains equal cells.
        const int spacing=size==8&&face!=32&&cp<128?1:(size?2:0);
        return cell(cp)+padding()+spacing;
    }
};
static MidpFont midp_font(int ref,bool graphics=false) {
    if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return {};
    const auto& tag=VMS_HEAP(g_heap)[ref].text;
    const size_t at=graphics?5:4;
    if(tag.size()!=(graphics?9:7)||tag.compare(0,4,graphics?"GFR3":"FNR3")!=0)return {};
    MidpFont f{uint8_t(tag[at]),uint8_t(tag[at+1]),uint8_t(tag[at+2])};
    if((f.face!=0&&f.face!=32&&f.face!=64)||(f.style&~7)||(f.size!=0&&f.size!=8&&f.size!=16))return {};
    return f;
}
static void midp_font_set(int ref,MidpFont f,bool graphics=false) {
    auto& tag=VMS_HEAP(g_heap)[ref].text;
    unsigned alpha=255;
    if(graphics) {
        if(tag.size()==8&&tag.compare(0,7,"ARGB236")==0)alpha=uint8_t(tag[7]);
        else if(tag.size()==9&&tag.compare(0,4,"GFR3")==0)alpha=uint8_t(tag[4]);
    }
    tag=graphics?"GFR3":"FNR3";
    if(graphics)tag.push_back(char(alpha));
    tag.push_back(char(f.face));tag.push_back(char(f.style));tag.push_back(char(f.size));
    if(graphics)tag.push_back(0); // reserved, part of the tagged payload version
}
static int text_width(const std::string& s,MidpFont font={}) {
    uint32_t w=0;size_t i=0;
    while(i<s.size())w+=unsigned(font.advance(utf8_next(s,i)));
    int32_t result;std::memcpy(&result,&w,4);return result;
}
struct CjkGlyph { uint16_t code; uint8_t bits[32]; };
 VMS_RS_ACCESSOR(g_cjk, std::vector<CjkGlyph>)
 VMS_RS_ACCESSOR(g_cjk_tried, bool)
static const uint8_t* find_cjk(int cp) {
    if (!VMS_RS(g_cjk_tried)) {
        VMS_RS(g_cjk_tried) = true;
        std::vector<uint8_t> raw;
        if (!read_whole("romfs:/font/cjk16.bin", raw)) read_whole("romfs/font/cjk16.bin", raw);
        if (raw.size() >= 38) {
            uint32_t n = ru32le(raw.data());
            if (4 + (size_t)n * 34 > raw.size()) n = (uint32_t)((raw.size() - 4) / 34);
            VMS_RS(g_cjk).resize(n);
            const uint8_t* p = raw.data() + 4;
            for (uint32_t i = 0; i < n; ++i) {
                VMS_RS(g_cjk)[i].code = ru16le(p);
                std::memcpy(VMS_RS(g_cjk)[i].bits, p + 2, 32);
                p += 34;
            }
        }
    }
    int lo = 0, hi = (int)VMS_RS(g_cjk).size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int c = VMS_RS(g_cjk)[mid].code;
        if (c == cp) return VMS_RS(g_cjk)[mid].bits;
        if (c < cp) lo = mid + 1; else hi = mid - 1;
    }
    return nullptr;
}

static int alloc_slot() {
    if (VMS_HEAP(g_free).empty() && VMS_HEAP(g_heap).size() > size_t(Value::max_reference)) die("Java heap handle limit exceeded");
    ++VMS_HEAP(g_alloc_since);
    if (!VMS_HEAP(g_free).empty()) { int r = VMS_HEAP(g_free).back(); VMS_HEAP(g_free).pop_back(); const uint32_t gen = VMS_HEAP(g_heap)[r].generation; VMS_HEAP(g_heap)[r] = Obj{}; VMS_HEAP(g_heap)[r].generation = gen; return r; }
    VMS_HEAP(g_heap).push_back(Obj{});
    return (int)VMS_HEAP(g_heap).size() - 1;
}
static int alloc_array(const std::string& descriptor, size_t length) {
    int ref = alloc_slot();
    Obj& o = VMS_HEAP(g_heap)[ref];
    if (!o.elems.reset(descriptor, length, &VMS_HEAP(g_primitive_buffers))) die("invalid array descriptor or length");
    o.is_array = true;
    o.array_desc = descriptor;
    uint64_t bytes = o.elems.payload_bytes();
    VMS_HEAP(g_array_payload_live) += bytes;
    VMS_HEAP(g_array_payload_peak) = std::max(VMS_HEAP(g_array_payload_peak), VMS_HEAP(g_array_payload_live));
    VMS_HEAP(g_array_alloc_since_gc) += bytes;
    diag_alloc(descriptor,length,bytes);
    return ref;
}
static int alloc_obj(int cls) {
    int ref = alloc_slot();
    VMS_HEAP(g_heap)[ref].cls = cls;
    if (cls >= 0) {
        VMS_HEAP(g_heap)[ref].fields.assign(VMS_CL(g_classes)[cls].instance_fields, Value{});
        for (int c = cls; c >= 0; c = VMS_CL(g_classes)[c].super_id)
            for (const auto& f : VMS_CL(g_classes)[c].fields)
                if (!f.is_static && (f.desc == "J" || f.desc == "D")) VMS_HEAP(g_heap)[ref].fields[f.slot] = Value::of_l(0);
    }
    for(int c=cls;c>=0;c=VMS_CL(g_classes)[c].super_id)if(VMS_CL(g_classes)[c].name=="java/lang/Thread"){p1_event("worker created ref=%d class=%s",ref,VMS_CL(g_classes)[cls].name.c_str());break;}
    return ref;
}
static Method* method_at(uint32_t loc) {
    int cls = (int)(loc >> 16), mi = (int)(loc & 0xffff);
    if (cls < 0 || cls >= (int)VMS_CL(g_classes).size() || mi >= (int)VMS_CL(g_classes)[cls].methods.size()) return nullptr;
    return &VMS_CL(g_classes)[cls].methods[mi];
}
static int graphics_intrinsic(Method* mm) {
    if (!mm || !mm->native || (mm->flags & 0x0028)) return 0;
    if (VMS_CL(g_classes)[mm->class_id].name != "javax/microedition/lcdui/Graphics") return 0;
    if (mm->name == "drawImage" && mm->desc=="(Ljavax/microedition/lcdui/Image;III)V") return 1;
    if (mm->name == "drawRegion" && mm->desc=="(Ljavax/microedition/lcdui/Image;IIIIIIII)V") return 2;
    if (mm->name == "fillRect" && mm->desc=="(IIII)V") return 3;
    if (mm->name == "drawLine" && mm->desc=="(IIII)V") return 9;
    if(J2ME_OPT_GFX_STATE) {
        if(mm->name=="setColor" && mm->desc=="(I)V")return 4;
        if(mm->name=="setColor" && mm->desc=="(III)V")return 5;
        if(mm->name=="setClip" && mm->desc=="(IIII)V")return 6;
        if(mm->name=="clipRect" && mm->desc=="(IIII)V")return 7;
        if(mm->name=="translate" && mm->desc=="(II)V")return 8;
        if(mm->desc=="()I") {
            if(mm->name=="getClipX")return 10;
            if(mm->name=="getClipY")return 11;
            if(mm->name=="getClipWidth")return 12;
            if(mm->name=="getClipHeight")return 13;
            if(mm->name=="getTranslateX")return 14;
            if(mm->name=="getTranslateY")return 15;
            if(mm->name=="getColor")return 16;
        }
    }
    return 0;
}
// JVM method identity includes the return descriptor. Never match by parameters alone.
static Method* find_method(int class_id, const std::string& name, const std::string& desc, bool walk) {
    for (int id = class_id; id >= 0; id = VMS_CL(g_classes)[id].super_id) {
        for (auto& m : VMS_CL(g_classes)[id].methods)
            if (m.name == name && m.desc == desc) return &m;
        if (!walk) break;
    }
    return nullptr;
}
static Field* find_field(int class_id, const std::string& name, const std::string& desc) {
    for (int id = class_id; id >= 0; ) {
        for (auto& f : VMS_CL(g_classes)[id].fields)
            if (f.name == name && f.desc == desc) return &f;
        id = VMS_CL(g_classes)[id].super_id;
    }
    return nullptr;
}
static int count_args(const std::string& desc) {
    int n = 0;
    for (size_t i = 1; i < desc.size() && desc[i] != ')'; ++i) {
        if (desc[i] == 'L') { i = desc.find(';', i); ++n; }
        else if (desc[i] == '[') { while (desc[i] == '[') ++i; if (desc[i] == 'L') i = desc.find(';', i); ++n; }
        else ++n;
    }
    return n;
}
static bool returns_void(const std::string& desc) {
    auto p = desc.find(')');
    return p != std::string::npos && p + 1 < desc.size() && desc[p + 1] == 'V';
}
static std::string cp_utf(const Class& c, int idx) {
    if (idx <= 0 || idx >= (int)c.cp.size() || c.cp[idx].tag != 1) die("bad cp utf " + c.name);
    return c.cp[idx].utf;
}
static std::string cp_class(const Class& c, int idx) { return cp_utf(c, c.cp[idx].a); }
static void resolve_member(const Class& c, int idx, std::string& owner, std::string& name, std::string& desc) {
    const CpEntry& e = c.cp[idx];
    owner = cp_class(c, e.a);
    const CpEntry& nt = c.cp[e.b];
    name = cp_utf(c, nt.a);
    desc = cp_utf(c, nt.b);
}
// Pacing recognizers inspect code that has not executed. Reject malformed CP
// links locally instead of exposing the unchecked execution resolver to scans.
static bool pacing_resolve_member(const Class& c,int idx,std::string& owner,std::string& name,std::string& desc) {
    auto tag=[&](int cp,int kind){return cp>0&&size_t(cp)<c.cp.size()&&c.cp[cp].tag==kind;};
    if(!tag(idx,9)&&!tag(idx,10))return false;
    const auto& e=c.cp[idx];
    if(!tag(e.a,7)||!tag(e.b,12))return false;
    const auto& nt=c.cp[e.b];
    if(!tag(c.cp[e.a].a,1)||!tag(nt.a,1)||!tag(nt.b,1))return false;
    owner=c.cp[c.cp[e.a].a].utf;name=c.cp[nt.a].utf;desc=c.cp[nt.b].utf;return true;
}
// Speculative pacing scans must not load or initialize a Fieldref's class.
// Resolve only already-loaded metadata to the same declaring owner/slot used
// by actual getstatic/putstatic execution (including inherited field names).
static const Field* pacing_loaded_static_long(const Class& c,int cp) {
    if(cp<=0||size_t(cp)>=c.cp.size()||c.cp[cp].tag!=9)return nullptr;
    std::string owner,name,desc;
    if(!pacing_resolve_member(c,cp,owner,name,desc)||desc!="J")return nullptr;
    const auto loaded=VMS_CL(g_class_index).find(owner);
    if(loaded==VMS_CL(g_class_index).end()||loaded->second<0||size_t(loaded->second)>=VMS_CL(g_classes).size())return nullptr;
    const auto* field=find_field(loaded->second,name,desc);
    if(!field||!field->is_static||field->owner<0||size_t(field->owner)>=VMS_CL(g_classes).size()||
       field->slot<0||size_t(field->slot)>=VMS_CL(g_classes)[field->owner].statics.size())return nullptr;
    return field;
}
static const Field* pacing_loaded_static_integral(const Class& c,int cp) {
    if(cp<=0||size_t(cp)>=c.cp.size()||c.cp[cp].tag!=9)return nullptr;
    std::string owner,name,desc;
    if(!pacing_resolve_member(c,cp,owner,name,desc)||
       (desc!="B"&&desc!="S"&&desc!="I"&&desc!="C"&&desc!="Z"))return nullptr;
    const auto loaded=VMS_CL(g_class_index).find(owner);
    if(loaded==VMS_CL(g_class_index).end()||loaded->second<0||size_t(loaded->second)>=VMS_CL(g_classes).size())return nullptr;
    const auto* field=find_field(loaded->second,name,desc);
    if(!field||!field->is_static||(field->flags&0x0040)||field->owner<0||size_t(field->owner)>=VMS_CL(g_classes).size()||
       field->slot<0||size_t(field->slot)>=VMS_CL(g_classes)[field->owner].statics.size())return nullptr;
    return field;
}
static bool type_compatible(int ref, const std::string& want);
#include "continuation.inc"
#include "p2s5_rootscope.inc"
#include "p2s3_present.inc"
#include "gbk.inc"
static int gbk_cp(unsigned a, unsigned b) {
    if (a < 0x81 || a > 0xFE || b == 0x7F || b < 0x40 || b > 0xFE) return -1;
    int i = (int)(a - 0x81) * 192 + (int)(b - 0x40);
    if (i < 0 || i >= (int)(sizeof(kGbk) / sizeof(kGbk[0]))) return -1;
    return kGbk[i] ? (int)kGbk[i] : -1;
}
#include "string_encoding.h"
#include "self_exit_hint.h"
static int make_string(const std::string& s) {
    std::string copy = s;
    int cid = ensure_class("java/lang/String");
    int ref = alloc_obj(cid);
    VMS_HEAP(g_heap)[ref].text = std::move(copy);
    return ref;
}
static int literal_string(const std::string& s, int candidate) {
    // Class loading can allocate, so never keep a borrowed source string.
    const std::string text = s;
    const int cid = ensure_class(kLiteralRootsClass);
    const Field* field = find_field(cid, "roots", "[Ljava/lang/String;");
    if (!field) die("literal root field missing");
    const int slot = field->slot;
    int roots = VMS_CL(g_classes)[cid].statics[slot].ref;
    auto& cache = VMS_CL(g_literal_cache);
    if (cache.epoch != resver::g_epoch || cache.roots != roots) {
        cache.refs.clear(); cache.epoch = resver::g_epoch; cache.roots = roots;
        if (roots) {
            if (roots <= 0 || size_t(roots) >= VMS_HEAP(g_heap).size() ||
                !VMS_HEAP(g_heap)[roots].is_array || VMS_HEAP(g_heap)[roots].array_desc != "[Ljava/lang/String;")
                die("invalid literal root array");
            const auto& array = VMS_HEAP(g_heap)[roots].elems;
            bool ended = false;
            for (size_t i = 0; i < array.size(); ++i) {
                const int ref = array.get(i).ref;
                if (!ref) { ended = true; continue; }
                if (ended || ref < 0 || size_t(ref) >= VMS_HEAP(g_heap).size() ||
                    VMS_HEAP(g_heap)[ref].cls < 0 ||
                    VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name != "java/lang/String" ||
                    !cache.refs.emplace(VMS_HEAP(g_heap)[ref].text, ref).second)
                    die("invalid literal root entry");
            }
        }
    }
    auto found = cache.refs.find(text);
    if (found != cache.refs.end()) return found->second;
    const size_t used = cache.refs.size();
    if (!roots || used == VMS_HEAP(g_heap)[roots].elems.size()) {
        const size_t capacity = roots ? VMS_HEAP(g_heap)[roots].elems.size() : 0;
        if (capacity > size_t(INT32_MAX) / 2) die("literal root capacity exhausted");
        const int replacement = alloc_array("[Ljava/lang/String;", capacity ? capacity * 2 : 16);
        if (roots) VMS_HEAP(g_heap)[replacement].elems.copy_from(VMS_HEAP(g_heap)[roots].elems, 0, 0, used);
        roots = replacement;
        VMS_CL(g_classes)[cid].statics[slot] = Value::of_ref(roots);
        cache.roots = roots;
    }
    // Publish the receiver (intern) or new literal in the registered static-array
    // root before adding its weak native index. Allocations are non-reentrant.
    const int ref = candidate ? candidate : make_string(text);
    VMS_HEAP(g_heap)[roots].elems.set(used, Value::of_ref(ref));
    cache.refs.emplace(text, ref);
    return ref;
}
static const std::string& read_string(int ref) {
    static const std::string empty;
    if (ref <= 0 || ref >= (int)VMS_HEAP(g_heap).size()) return empty;
    return VMS_HEAP(g_heap)[ref].text;
}
#include "exception_trace.h"
#include "p8_diag.h"
static void throw_new(const std::string& cls, const std::string& msg) {
#ifdef J2ME_TRACE_THROW
    log_line("TRACE_THROW "+cls+" "+msg);
#endif
    int cid = ensure_class(cls);
    int ref = alloc_obj(cid);
    if (Field* f = find_field(cid, "detail", "Ljava/lang/String;")) VMS_HEAP(g_heap)[ref].fields[f->slot] = Value::of_ref(make_string(msg));
    capture_exception_trace(ref);
    throw_ref(ref);
}
static bool class_assignable(int actual, int wanted) {
    if (actual == wanted) return true;
    const auto& c = VMS_CL(g_classes)[actual];
    if (c.super_id >= 0 && class_assignable(c.super_id, wanted)) return true;
    for (int i : c.interfaces) if (class_assignable(i, wanted)) return true;
    return false;
}
static std::string component_type(const std::string& desc) {
    std::string c = desc.substr(1);
    if (!c.empty() && c[0] == 'L') return c.substr(1, c.size() - 2);
    return c;
}
static bool descriptor_assignable(const std::string& actual, const std::string& want) {
    if (actual == want) return true;
    if (actual.empty() || want.empty()) return false;
    if (actual[0] == '[') {
        if (want == "java/lang/Object" || want == "java/lang/Cloneable" || want == "java/io/Serializable") return true;
        if (want[0] != '[') return false;
        // Primitive components require equality; reference components are covariant.
        if (actual.size() == 2 || want.size() == 2) return false;
        return descriptor_assignable(component_type(actual), component_type(want));
    }
    if (want[0] == '[') return false;
    return class_assignable(ensure_class(actual), ensure_class(want));
}
static bool type_compatible(int ref, const std::string& want) {
    if (!ref) return true;
    const auto& o = VMS_HEAP(g_heap)[ref];
    return descriptor_assignable(o.is_array ? o.array_desc : VMS_CL(g_classes)[o.cls].name, want);
}
static void throw_ref(int ref) {
    diag_exception(ref); // Actual throw origin, before searching/unwinding handlers.
    current_context().compensated_sleep.cancel();
    current_context().rechecked_deadline.cancel(); // Same-method catches must earn fresh frame evidence.
    auto& cached_context=current_context();
    cached_context.cached_elapsed.cancel();
    cached_context.cached_elapsed.rejected_anchor_us=cached_context.field_gate_anchor.real_us;
    VMS_EXEC(g_thrown) = true;
    auto* st = VMS_EXEC(g_active_stack);
    if (!st) die("throw outside run");
    Method* p1_origin=st->empty()?nullptr:st->back().method;
    const int p1_origin_bci=st->empty()?-1:st->back().op_pc;const size_t p1_origin_depth=st->size();
    current_context().p1_throw_method=p1_origin;current_context().p1_throw_bci=p1_origin_bci;current_context().p1_throw_frames=p1_origin_depth;
    while (!st->empty()) {
        Frame& fr = st->back();
        p1_snapshot(current_context(),fr,st->size());
        Method* m = fr.method;
        int pc = fr.op_pc;
        const Class& owner = VMS_CL(g_classes)[m->class_id];
        for (auto& h : m->handlers) {
            if (fr.init_super_pending) break; // The subclass body has not started.
            if (pc < h.start || pc >= h.end) continue;
            bool match = h.catch_cp == 0;
            if (!match) match = type_compatible(ref, cp_class(owner, h.catch_cp));
            if (!match) continue;
            p1_caught(ref,fr,h.pc,p1_origin,p1_origin_bci,p1_origin_depth);
            fr.stack.clear();
            fr.stack.push_back(Value::of_ref(ref));
            fr.pc = h.pc;
            return;
        }
        if (st->back().init_class >= 0) {
            complete_class_initialization(st->back(), false, ref);
            if (!type_compatible(ref,"java/lang/Error")) {
                RootScope roots(ReentrySite::ThrowWrap,{&ref}); // P2.4: the in-flight exception is only a C++ local while ensure_class may run <clinit>
                int wrapped = alloc_obj(ensure_class("java/lang/ExceptionInInitializerError"));
                capture_exception_trace(wrapped);
                if (Field* f = find_field(VMS_HEAP(g_heap)[wrapped].cls,"exception","Ljava/lang/Throwable;")) VMS_HEAP(g_heap)[wrapped].fields[f->slot]=Value::of_ref(ref);
                ref=wrapped;
            }
        }
        release_frame_monitor(st->back());
        st->pop_back();
    }
    current_context().unhandled = ref;
}
static void report_uncaught(int ref) {
    std::string cn = (ref > 0 && VMS_HEAP(g_heap)[ref].cls >= 0) ? VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name : "exception";
    std::string msg;
    if (ref > 0) {
        if (Field* f = find_field(VMS_HEAP(g_heap)[ref].cls, "detail", "Ljava/lang/String;")) msg = read_string(VMS_HEAP(g_heap)[ref].fields[f->slot].ref);
    }
    std::string message = "uncaught " + cn + (msg.empty() ? "" : " " + msg);
    diag_fault("JAVA_UNCAUGHT",message.c_str());
    log_line("EX " + message);
    log_exception_trace(ref);
    if (VMS_EXEC(g_running_thread)) { VMS_EXEC(g_running_thread)->p1_end_reason=1; log_line("THREAD " + message); finish_thread(*VMS_EXEC(g_running_thread)); }
    else die(message);
}

static void image_layout_dirty(int ref) {
    Obj& o = VMS_HEAP(g_heap)[ref]; o.image_layout_version = o.image_content_version = resver::tick(); ++resver::g_stats.image_layout;
}
static std::vector<uint8_t>* pixels_of(int target, int& w, int& h) {
    if (target == 0) { w = VMS_RS(g_screen_w); h = VMS_RS(g_screen_h); return &VMS_RS(g_fb); }
    if (target <= 0 || target >= (int)VMS_HEAP(g_heap).size()) return nullptr;
    w = VMS_HEAP(g_heap)[target].width; h = VMS_HEAP(g_heap)[target].height;
    // P2.5: the mutable image view is the image write barrier (short borrow; pixels and alpha).
    VMS_HEAP(g_heap)[target].image_content_version = resver::tick(); ++resver::g_stats.image_content;
    return &VMS_HEAP(g_heap)[target].pixels;
}
static void set_px(std::vector<uint8_t>& px, int w, int h, int x, int y, int rgb) {
    if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
    size_t i = ((size_t)y * w + x) * 3;
    px[i] = (rgb >> 16) & 255; px[i + 1] = (rgb >> 8) & 255; px[i + 2] = rgb & 255;
}
static int get_px(const std::vector<uint8_t>& px, int w, int h, int x, int y) {
    if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h || px.empty()) return 0;
    size_t i = ((size_t)y * w + x) * 3;
    return 0xff000000 | (px[i] << 16) | (px[i + 1] << 8) | px[i + 2];
}
static raster_primitives::Bounds gfx_bounds(int gfx,int tw,int th) {
    raster_primitives::Bounds b{0,0,tw,th};
    auto it=VMS_RS(g_clip).find(gfx);
    if(it!=VMS_RS(g_clip).end()&&it->second.on) {
        const Clip& c=it->second;
        b=raster_primitives::intersect(b,{c.x,c.y,int64_t(c.x)+std::max(0,c.w),int64_t(c.y)+std::max(0,c.h)});
    }
    return b;
}
static bool clip_ok(int gfx, int x, int y) {
    auto it=VMS_RS(g_clip).find(gfx);
    if(it==VMS_RS(g_clip).end()||!it->second.on) return true;
    const Clip& c=it->second;
    return x>=c.x&&y>=c.y&&int64_t(x)<int64_t(c.x)+c.w&&int64_t(y)<int64_t(c.y)+c.h;
}
// Graphics native metadata uses the already serialized Obj.text payload. No
// Java field/BCI or snapshot wire-layout change is needed. Older bindings with
// an empty payload keep their original opaque behavior.
static unsigned gfx_opacity(int gfx) {
    if(gfx<=0||size_t(gfx)>=VMS_HEAP(g_heap).size())return 255;
    const auto& tag=VMS_HEAP(g_heap)[gfx].text;
    if(tag.size()==9&&tag.compare(0,4,"GFR3")==0)return uint8_t(tag[4]);
    return tag.size()==8&&tag.compare(0,7,"ARGB236")==0 ? uint8_t(tag[7]) : 255;
}
static void gfx_opacity_set(int gfx,unsigned alpha) {
    if(gfx<=0||size_t(gfx)>=VMS_HEAP(g_heap).size())return;
    auto& tag=VMS_HEAP(g_heap)[gfx].text;
    if(tag.size()==9&&tag.compare(0,4,"GFR3")==0)tag[4]=char(alpha);
    else if(alpha==255)tag.clear();else {tag="ARGB236";tag.push_back(char(alpha));}
}
// Only Java mutable images carry writable destination alpha. Raw raster-test
// surfaces and immutable sources retain the legacy RGB-only behavior.
static bool mutable_argb_target236(int target) {
    if(target<=0||size_t(target)>=VMS_HEAP(g_heap).size())return false;
    const auto& image=VMS_HEAP(g_heap)[target];
    if(image.cls<0||size_t(image.cls)>=VMS_CL(g_classes).size()||image.width<=0||image.height<=0)return false;
    const uint64_t count=uint64_t(image.width)*uint64_t(image.height);
    if(count>image.alpha.size()||count>image.pixels.size()/3)return false;
    Field* flag=find_field(image.cls,"mutable","Z");
    return flag&&flag->slot>=0&&size_t(flag->slot)<image.fields.size()&&image.fields[flag->slot].i!=0;
}
static raster_primitives::Surface gfx_surface(int gfx,int target,std::vector<uint8_t>& pixels,int w,int h) {
    return {pixels,w,h,gfx_bounds(gfx,w,h),mutable_argb_target236(target)?&VMS_HEAP(g_heap)[target].alpha:nullptr,gfx_opacity(gfx)};
}
static void fill_rect(int gfx, int target, int64_t x, int64_t y, int w, int h, int rgb) {
    int tw=0,th=0;
    auto* px=pixels_of(target,tw,th);
    if(!px||w<=0||h<=0) return;
    auto b=raster_primitives::intersect(gfx_bounds(gfx,tw,th),{x,y,x+w,y+h});
    if(b.left>=b.right||b.top>=b.bottom) return;
    auto surface=gfx_surface(gfx,target,*px,tw,th);
    if(surface.opacity!=255||surface.alpha) {
        for(int64_t yy=b.top;yy<b.bottom;++yy)for(int64_t xx=b.left;xx<b.right;++xx)surface.put(xx,yy,uint32_t(rgb));
        return;
    }
    uint8_t r=(rgb>>16)&255,g=(rgb>>8)&255,blue=rgb&255;
    rgb_fill::rectangle(px->data()+(size_t(b.top)*tw+size_t(b.left))*3,
                        size_t(tw)*3,size_t(b.right-b.left),size_t(b.bottom-b.top),r,g,blue);
}
static void note_rect(int x, int y, int w, int h, int color) {
    if (!VMS_RS(g_drawacc).on) return;
    VMS_RS(g_drawacc).rects++;
    VMS_RS(g_drawacc).minx = std::min(VMS_RS(g_drawacc).minx, x);
    VMS_RS(g_drawacc).miny = std::min(VMS_RS(g_drawacc).miny, y);
    VMS_RS(g_drawacc).maxx = std::max(VMS_RS(g_drawacc).maxx, int(std::clamp(int64_t(x) + w, int64_t(INT32_MIN), int64_t(INT32_MAX))));
    VMS_RS(g_drawacc).maxy = std::max(VMS_RS(g_drawacc).maxy, int(std::clamp(int64_t(y) + h, int64_t(INT32_MIN), int64_t(INT32_MAX))));
    if (VMS_RS(g_drawacc).sample.empty())
        VMS_RS(g_drawacc).sample = std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(w) + "x" +
                           std::to_string(h) + " #" + std::to_string(color);
}
static void note_text(const std::string& s) {
    if (!s.empty()) mix_frame_sig(uint32_t(s.size()) ^ (uint32_t(static_cast<unsigned char>(s[0])) << 8));
    if (!VMS_RS(g_drawacc).on || s.empty()) return;
    if (!VMS_RS(g_drawacc).textblob.empty()) VMS_RS(g_drawacc).textblob += "|";
    VMS_RS(g_drawacc).textblob += s;
}
static void draw_begin() {
    VMS_RS(g_drawacc) = {};
    VMS_RS(g_drawacc).on = VMS_DIAG(g_log_draw) && (VMS_RS(g_game_frames) % 30) == 0;
}
static void draw_end(const std::string& who) {
    ++VMS_DIAG(g_p1).draw_id;
    VMS_RS(g_game_frames)++;
    VMS_RS(g_frame_metrics).record(paint_clock_us(),VMS_RS(g_fb));
    if (!VMS_RS(g_drawacc).on) return;
    int n = ++VMS_RS(g_paint_serial);
    VMS_RS_LOCAL(prev, draw_end_prev, static std::string prev;)
    bool show = n <= 2 || (n % 15) == 0 || VMS_RS(g_drawacc).textblob != prev || VMS_RS(g_drawacc).images;
    prev = VMS_RS(g_drawacc).textblob;
    VMS_RS(g_drawacc).on = false;
    if (!show) return;
    log_line("PAINT #" + std::to_string(n) + " " + who +
             " rects=" + std::to_string(VMS_RS(g_drawacc).rects) +
             " lines=" + std::to_string(VMS_RS(g_drawacc).lines) +
             " img=" + std::to_string(VMS_RS(g_drawacc).images) +
             " clip=" + std::to_string(VMS_RS(g_drawacc).clips) +
             " text=\"" + VMS_RS(g_drawacc).textblob + "\" bbox=" +
             std::to_string(VMS_RS(g_drawacc).minx) + "," + std::to_string(VMS_RS(g_drawacc).miny) + "-" +
             std::to_string(VMS_RS(g_drawacc).maxx) + "," + std::to_string(VMS_RS(g_drawacc).maxy) +
             " sample=" + VMS_RS(g_drawacc).sample);
}
#include "small_font12.h"
static void draw_string(int gfx, int target, int x, int y, int color, const std::string& s) {
    int tw=0,th=0;auto* px=pixels_of(target,tw,th);if(!px)return;
    auto surface=gfx_surface(gfx,target,*px,tw,th);
    const MidpFont font=midp_font(gfx,true);
    const int h=font.pixels();int64_t cx=x;size_t i=0;
    while(i<s.size()) {
        const int cp=utf8_next(s,i),cell=font.cell(cp),advance=font.advance(cp);
        const uint8_t* cjk=cp>=128?find_cjk(cp):nullptr;
        const uint8_t* native12=h==12?small_font12::find(cp):nullptr;
        const int source_width=cp<128?8:16;
        const unsigned opacity=surface.opacity;
        auto bits_at=[&](int row) {
            return cp<128?(cp>=32&&cp<127?int(latin16[cp-32][row]):0):
                cjk?((cjk[row*2]<<8)|cjk[row*2+1]):(row==0||row==15?0xffff:0x8001);
        };
        const int glyph_width=source_width*h/16,inset=(cell-glyph_width)/2;
        // 0.2.108: the 16-row Latin source has its baseline on source row 12.
        // Plain 20px scaling put that row on display rows 15-16 (1px below the
        // 20px baseline row 15); sampling one row later ends it on row 15.
        // 12px uses its native bitmap and 16px is 1:1; both already match.
        const int latin_row_bias=(cp<128&&h==20)?1:0;
        for(int row=0;row<h;++row) {
            const int shift=(font.style&2)?(h-1-row)/4:0;
            for(int col=0;col<glyph_width;++col) {
                unsigned coverage=0;
                if(native12) {
                    if(((native12[row*2]<<8)|native12[row*2+1])&(0x8000>>col))coverage=255;
                } else if(h<16) {
                    // Integrate the source bitmap area instead of dropping
                    // strokes when a 16px CJK glyph is reduced to 12px.
                    const int x0=col*source_width,x1=x0+source_width,y0=row*16,y1=y0+16;
                    int ink=0;
                    for(int sy=y0/h;sy<=(y1-1)/h;++sy) {
                        const int wy=std::min(y1,(sy+1)*h)-std::max(y0,sy*h),bits=bits_at(sy);
                        for(int sx=x0/glyph_width;sx<=(x1-1)/glyph_width;++sx)
                            if(bits&(0x8000>>sx))ink+=wy*(std::min(x1,(sx+1)*glyph_width)-std::max(x0,sx*glyph_width));
                    }
                    coverage=unsigned((ink*255+source_width*8)/(source_width*16));
                } else if((row+latin_row_bias)*16/h<16&&(bits_at((row+latin_row_bias)*16/h)&(0x8000>>(col*source_width/glyph_width))))coverage=255;
                if(!coverage)continue;
                surface.opacity=(opacity*coverage+127)/255;
                surface.put(cx+inset+col+shift,int64_t(y)+row,uint32_t(color));
                if(font.style&1)surface.put(cx+inset+col+shift+1,int64_t(y)+row,uint32_t(color));
            }
        }
        surface.opacity=opacity;
        // 0.2.108: underline uniformly 1px below the baseline row for every size.
        if(font.style&4)for(int col=0;col<advance;++col)surface.put(cx+col,int64_t(y)+font.baseline()+1,uint32_t(color));
        cx+=advance;
    }
}
static void apply_anchor(int anchor, int& x, int& y, int w, int h) {
    auto sub=[](int value,int delta){uint32_t bits=uint32_t(value)-uint32_t(delta);int32_t result;std::memcpy(&result,&bits,4);return result;};
    if(anchor&1)x=sub(x,w/2);if(anchor&8)x=sub(x,w);
    if(anchor&2)y=sub(y,h/2);if(anchor&32)y=sub(y,h);
    if(anchor&64)y=sub(y,h-2);
}
static bool gfx_anchor_ok(int anchor, bool text) {
    if(anchor==0)return true;
    int horizontal=anchor&(1|4|8), vertical=anchor&(2|16|32|64);
    return !(anchor&~127)&&(horizontal==1||horizontal==4||horizontal==8)&&
        (text?(vertical==16||vertical==32||vertical==64):(vertical==2||vertical==16||vertical==32));
}
static void draw_region(int gfx, int target, int image_ref, int sx, int sy, int w, int h, int transform, int dx, int dy);
// Resolved native ()I queries keep their diagnostic/profiling boundary and
// original invoke charge, without constructing native argument/result Frames.
// Non-void queries are excluded by the existing region-JIT Graphics admission.
static int gfx_query_intrinsic(int kind,int self,Method* method) {
    const auto diagnostic=diag_native_enter(method);
    int result=0;
    {
        P61_SCOPE(2, method);
        const auto target_entry=VMS_RS(g_gfx_target).find(self);
        const int target=target_entry==VMS_RS(g_gfx_target).end()?0:target_entry->second;
        GraphicsGlobalScope global_sample(VMS_RS(g_graphics_profile),target!=0);
        GraphicsProfileScope native_sample(VMS_RS(g_graphics_profile),0);
        auto state=[&](const std::unordered_map<int,int>& values) {
            auto at=values.find(self);return at==values.end()?0:at->second;
        };
        auto difference=[](int a,int b){uint32_t bits=uint32_t(a)-uint32_t(b);int32_t value;std::memcpy(&value,&bits,4);return int(value);};
        if(kind==16)result=state(VMS_RS(g_gfx_color))&0xffffff;
        else if(kind==14)result=state(VMS_RS(g_gfx_tx));
        else if(kind==15)result=state(VMS_RS(g_gfx_ty));
        else {
            auto at=VMS_RS(g_clip).find(self);
            const Clip* clip=at!=VMS_RS(g_clip).end()&&at->second.on?&at->second:nullptr;
            if(kind==10)result=difference(clip?clip->x:0,state(VMS_RS(g_gfx_tx)));
            else if(kind==11)result=difference(clip?clip->y:0,state(VMS_RS(g_gfx_ty)));
            else {
                // The native width/height queries borrow pixels even with an
                // explicit clip; retain that existing image-version barrier.
                int w=0,h=0;pixels_of(target,w,h);
                result=kind==12?(clip?std::max(0,clip->w):w):(clip?std::max(0,clip->h):h);
            }
        }
    }
    diag_native_leave(diagnostic);
    return result;
}
// Hot Graphics calls skip the native-method frame and the class-name chain.
static void gfx_intrinsic(int kind, Value* a) {
    if(kind==9) {
        // Keep the native diagnostic boundary and the declaring Graphics
        // class's stroke field even when a subclass inherits drawLine.
        const int self=a[0].ref;
        Method* method=nullptr;
        if(VMS_EXEC(g_active_stack)&&!VMS_EXEC(g_active_stack)->empty()) {
            const auto& caller=VMS_EXEC(g_active_stack)->back();
            if(caller.method&&caller.op_pc>=0&&size_t(caller.op_pc)<caller.method->quick.size()) {
                const auto& site=caller.method->quick[size_t(caller.op_pc)];
                if(site.intrinsic==9) {
                    if(site.kind==4&&site.vclass==VMS_HEAP(g_heap)[self].cls)
                        method=method_at((uint32_t(site.va)<<16)|uint32_t(site.vb));
                    else if(site.kind==3)
                        method=method_at((uint32_t(site.a)<<16)|uint32_t(site.b));
                }
            }
        }
        if(!method)method=find_method(VMS_HEAP(g_heap)[self].cls,"drawLine","(IIII)V",true);
        const auto diagnostic=diag_native_enter(method);
        {
            const auto it=VMS_RS(g_gfx_target).find(self);
            const int target=it==VMS_RS(g_gfx_target).end()?0:it->second;
            GraphicsGlobalScope global_sample(VMS_RS(g_graphics_profile),target!=0);
            GraphicsProfileScope native_sample(VMS_RS(g_graphics_profile),0);
            auto state=[&](const std::unordered_map<int,int>& values) {auto at=values.find(self);return at==values.end()?0:at->second;};
            auto coord=[](int x,int y){uint32_t bits=uint32_t(x)+uint32_t(y);int32_t result;std::memcpy(&result,&bits,4);return int(result);};
            const int tx=state(VMS_RS(g_gfx_tx)),ty=state(VMS_RS(g_gfx_ty)),color=state(VMS_RS(g_gfx_color));
            int tw=0,th=0;auto* pixels=pixels_of(target,tw,th);
            if(VMS_RS(g_drawacc).on)++VMS_RS(g_drawacc).lines;
            if(pixels) {
                const Field* stroke=find_field(method->class_id,"strokeStyle","I");
                const bool dotted=stroke&&VMS_HEAP(g_heap)[self].fields[stroke->slot].i==1;
                auto surface=gfx_surface(self,target,*pixels,tw,th);
                raster_primitives::line(surface,coord(a[1].i,tx),coord(a[2].i,ty),coord(a[3].i,tx),coord(a[4].i,ty),uint32_t(color),dotted);
            }
        }
        diag_native_leave(diagnostic);return;
    }
    const auto target_entry=VMS_RS(g_gfx_target).find(a[0].ref);
    const int target=target_entry==VMS_RS(g_gfx_target).end()?0:target_entry->second;
    GraphicsGlobalScope global_sample(VMS_RS(g_graphics_profile),target!=0);
    GraphicsProfileScope native_sample(VMS_RS(g_graphics_profile), (kind==6||kind==7)?2:(kind==1||kind==2)?1:0);
    int self=a[0].ref;
    auto state=[&](const std::unordered_map<int,int>& values) {
        auto it=values.find(self);return it==values.end()?0:it->second;
    };
    auto coord=[](int x,int y){uint32_t bits=uint32_t(x)+uint32_t(y);int32_t result;std::memcpy(&result,&bits,4);return (int)result;};
    if(kind==4) {VMS_RS(g_gfx_color)[self]=a[1].i&0xffffff;gfx_opacity_set(self,255);return;}
    if(kind==5) {
        if(a[1].i<0||a[1].i>255||a[2].i<0||a[2].i>255||a[3].i<0||a[3].i>255) {
            throw_new("java/lang/IllegalArgumentException","color component");return;
        }
        VMS_RS(g_gfx_color)[self]=(a[1].i<<16)|(a[2].i<<8)|a[3].i;gfx_opacity_set(self,255);return;
    }
    int tx=state(VMS_RS(g_gfx_tx)), ty=state(VMS_RS(g_gfx_ty));
    if(kind==8) {VMS_RS(g_gfx_tx)[self]=coord(tx,a[1].i);VMS_RS(g_gfx_ty)[self]=coord(ty,a[2].i);return;}
    if(kind==6 || kind==7) {
        int tw=0,th=0;pixels_of(target,tw,th);
        const int64_t x=coord(a[1].i,tx),y=coord(a[2].i,ty);
        auto b=raster_primitives::intersect({0,0,tw,th},{x,y,x+std::max(0,a[3].i),y+std::max(0,a[4].i)});
        if(kind==7)b=raster_primitives::intersect(b,gfx_bounds(self,tw,th));
        const int left=int(std::max(int64_t(0),std::min(int64_t(tw),b.left)));
        const int top=int(std::max(int64_t(0),std::min(int64_t(th),b.top)));
        VMS_RS(g_clip)[self]=Clip{left,top,int(std::max(int64_t(0),b.right-left)),int(std::max(int64_t(0),b.bottom-top)),true};
        if(VMS_RS(g_drawacc).on)++VMS_RS(g_drawacc).clips;return;
    }
    if(kind==3) {
        int color=state(VMS_RS(g_gfx_color));
        int x=coord(a[1].i,tx), y=coord(a[2].i,ty);
        note_rect(x,y,a[3].i,a[4].i,color&0xffffff);
        { p86::Scope p86_scope(p86::FillRect,target!=0,uint64_t(std::max(0,a[3].i))*uint64_t(std::max(0,a[4].i))); fill_rect(self,target,x,y,a[3].i,a[4].i,color); }
        return;
    }
    int img=a[1].ref;
    if(!img){throw_new("java/lang/NullPointerException","image");return;}
    int sx=0,sy=0,w=VMS_HEAP(g_heap)[img].width,h=VMS_HEAP(g_heap)[img].height,transform=0;
    int x=coord(a[2].i,tx), y=coord(a[3].i,ty), anchor=a[4].i;
    if(kind==2) {
        sx=a[2].i;sy=a[3].i;w=a[4].i;h=a[5].i;transform=a[6].i;
        x=coord(a[7].i,tx);y=coord(a[8].i,ty);anchor=a[9].i;
        if(target==img||sx<0||sy<0||w<0||h<0||int64_t(sx)+w>VMS_HEAP(g_heap)[img].width||int64_t(sy)+h>VMS_HEAP(g_heap)[img].height||transform<0||transform>7) {
            throw_new("java/lang/IllegalArgumentException","image region");return;
        }
    }
    if(!gfx_anchor_ok(anchor,false)){throw_new("java/lang/IllegalArgumentException","image anchor");return;}
    apply_anchor(anchor,x,y,transform>=4?h:w,transform>=4?w:h);
    draw_region(self,target,img,sx,sy,w,h,transform,x,y);
}
#include "star_map_bounds_compat.h"
static void draw_region(int gfx, int target, int image_ref, int sx, int sy, int w, int h, int transform, int dx, int dy) {
    if (image_ref <= 0 || image_ref >= (int)VMS_HEAP(g_heap).size()) return;
    Obj& src = VMS_HEAP(g_heap)[image_ref];
    int tw, th;
    auto* dst = pixels_of(target, tw, th);
    if (!dst || src.pixels.empty()) return;
    p86::Scope p86_scope(mutable_argb_target236(target) ? p86::ImgArgbDst : transform ? p86::ImgTransform : !src.alpha.empty() ? p86::ImgAlpha : p86::ImgPlain,
                         target != 0, uint64_t(std::max(0, w)) * uint64_t(std::max(0, h)));
    raster_region::Clip clip;
    auto it = VMS_RS(g_clip).find(gfx);
    if (it != VMS_RS(g_clip).end() && it->second.on) {
        const Clip& c = it->second;
        clip = {true, c.x, c.y, c.w, c.h};
    }
    star_map_bounds_compat_clip(gfx,target,image_ref,transform,clip);
    {
        GraphicsGlobalRasterScope global_raster(VMS_RS(g_graphics_profile));
        GraphicsProfileScope raster_sample(VMS_RS(g_graphics_profile));
        if(mutable_argb_target236(target)) {
            const uint64_t count=uint64_t(std::max(0,src.width))*uint64_t(std::max(0,src.height));
            if(w<=0||h<=0||src.width<=0||src.height<=0||transform<0||transform>7||
               count>src.pixels.size()/3||(!src.alpha.empty()&&count>src.alpha.size()))return;
            auto surface=gfx_surface(gfx,target,*dst,tw,th);
            const int ow=transform>=4?h:w,oh=transform>=4?w:h;
            auto b=raster_primitives::intersect(surface.bounds(),{dx,dy,int64_t(dx)+ow,int64_t(dy)+oh});
            for(int64_t yy=b.top;yy<b.bottom;++yy)for(int64_t xx=b.left;xx<b.right;++xx) {
                int u=0,v=0;pixel_compat236::inverse(transform,w,h,int(xx-dx),int(yy-dy),u,v);
                const int64_t source_x=int64_t(u)+sx,source_y=int64_t(v)+sy;
                if(source_x<0||source_y<0||source_x>=src.width||source_y>=src.height)continue;
                size_t i=size_t(source_y)*src.width+size_t(source_x);
                surface.opacity=src.alpha.empty()?255:src.alpha[i];
                surface.put(xx,yy,(unsigned(src.pixels[i*3])<<16)|(unsigned(src.pixels[i*3+1])<<8)|src.pixels[i*3+2]);
            }
        } else raster_region::draw(src.pixels, src.alpha, src.width, src.height, *dst, tw, th,
                            sx, sy, w, h, transform, dx, dy, clip);
    }
    mix_frame_sig(1);
    if (VMS_RS(g_drawacc).on) VMS_RS(g_drawacc).images++;
}
static void blit_argb(int gfx, int target, int tx, int ty, int array, int offset, int scan, int x, int y, int w, int h, int manip, bool trans) {
    if(array<=0||array>=(int)VMS_HEAP(g_heap).size()||w<=0||h<=0) return;
    int tw=0,th=0;auto* dst=pixels_of(target,tw,th);if(!dst) return;
    auto& e=VMS_HEAP(g_heap)[array].elems;
    int64_t ox=int64_t(x)+tx,oy=int64_t(y)+ty;
    const int t=pixel_compat236::transform(manip);if(t<0)return;
    const int dw=t>=4?h:w,dh=t>=4?w:h;
    auto b=raster_primitives::intersect(gfx_bounds(gfx,tw,th),{ox,oy,ox+dw,oy+dh});
    if(b.left>=b.right||b.top>=b.bottom) return;
    const int32_t* ints=e.int_data();
    int written=0,sample=0;
    auto store=[&](int64_t xx,int64_t yy,uint32_t rgb){
        size_t i=(size_t(yy)*size_t(tw)+size_t(xx))*3;
        (*dst)[i]=uint8_t(rgb>>16);(*dst)[i+1]=uint8_t(rgb>>8);(*dst)[i+2]=uint8_t(rgb);
        if(!written)sample=int(rgb&0xffffff);++written;
    };
    if(mutable_argb_target236(target)) {
        auto surface=gfx_surface(gfx,target,*dst,tw,th);
        for(int64_t yy=b.top;yy<b.bottom;++yy)for(int64_t xx=b.left;xx<b.right;++xx) {
            int sx=0,sy=0;pixel_compat236::inverse(t,w,h,int(xx-ox),int(yy-oy),sx,sy);
            int64_t idx=int64_t(offset)+int64_t(sy)*scan+sx;
            if(idx<0||uint64_t(idx)>=e.size())continue;
            uint32_t c=uint32_t(e.get(size_t(idx)).i);surface.opacity=trans?c>>24:255;
            surface.put(xx,yy,c);++written;
        }
    } else if(ints && manip==0 && (reinterpret_cast<uintptr_t>(ints)&3)==0) {
        if(!trans) {
            for(int64_t yy=b.top;yy<b.bottom;++yy) {
                int64_t sy=yy-oy;
                int64_t first=int64_t(offset)+sy*scan+(b.left-ox);
                int n=int(b.right-b.left);
                if(first<0||n<=0||uint64_t(first)+uint64_t(n)>e.size()) continue;
                const int32_t* row=ints+first;
                uint8_t* out=dst->data()+(size_t(yy)*size_t(tw)+size_t(b.left))*3;
                for(int i=0;i<n;++i,out+=3) {
                    uint32_t c=uint32_t(row[i]);
                    out[0]=uint8_t(c>>16);out[1]=uint8_t(c>>8);out[2]=uint8_t(c);
                }
                if(!written)sample=int(uint32_t(row[0])&0xffffff);
                written+=n;
            }
        } else {
            for(int64_t yy=b.top;yy<b.bottom;++yy) {
                int64_t sy=yy-oy;
                int64_t origin=int64_t(offset)+sy*scan-ox;
                for(int64_t xx=b.left;xx<b.right;) {
                    int64_t idx=origin+xx;
                    if(idx<0||uint64_t(idx)>=e.size()){++xx;continue;}
                    uint32_t argb=uint32_t(ints[idx]);
                    unsigned a=argb>>24;
                    if(!a){++xx;continue;}
                    if(a==255) {
                        int64_t run=xx;
                        while(run<b.right) {
                            int64_t ri=origin+run;
                            if(ri<0||uint64_t(ri)>=e.size()||(uint32_t(ints[ri])>>24)!=255) break;
                            ++run;
                        }
                        uint8_t* out=dst->data()+(size_t(yy)*size_t(tw)+size_t(xx))*3;
                        for(int64_t k=xx;k<run;++k,out+=3) {
                            uint32_t c=uint32_t(ints[origin+k]);
                            out[0]=uint8_t(c>>16);out[1]=uint8_t(c>>8);out[2]=uint8_t(c);
                        }
                        if(!written)sample=int(argb&0xffffff);
                        written+=int(run-xx);
                        xx=run;
                        continue;
                    }
                    uint32_t dst_rgb=uint32_t(get_px(*dst,tw,th,int(xx),int(yy)));
                    store(xx,yy,raster_primitives::source_over(argb,dst_rgb,true));
                    ++xx;
                }
            }
        }
    } else {
        for(int64_t yy=b.top;yy<b.bottom;++yy) for(int64_t xx=b.left;xx<b.right;++xx) {
            int sx=0,sy=0;pixel_compat236::inverse(t,w,h,int(xx-ox),int(yy-oy),sx,sy);
            int64_t idx=int64_t(offset)+sy*scan+sx;
            if(idx<0||uint64_t(idx)>=e.size())continue;
            uint32_t argb=uint32_t(e.get(size_t(idx)).i);
            if(trans&&!(argb>>24))continue;
            store(xx,yy,raster_primitives::source_over(argb,uint32_t(get_px(*dst,tw,th,int(xx),int(yy))),trans));
        }
    }
    if(VMS_RS(g_drawacc).on) note_text("PIX "+std::to_string(written)+" #"+std::to_string(sample));
}
static void read_argb(int target, int tx, int ty, int array, int offset, int scan, int x, int y, int w, int h) {
    if(array<=0||array>=(int)VMS_HEAP(g_heap).size()||w<=0||h<=0) return;
    int tw=0,th=0;auto* src=pixels_of(target,tw,th);if(!src)return;
    auto& e=VMS_HEAP(g_heap)[array].elems;
    for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx) {
        int64_t idx=int64_t(offset)+int64_t(yy)*scan+xx;
        if(idx<0||uint64_t(idx)>=e.size())continue;
        int64_t sx=int64_t(x)+tx+xx,sy=int64_t(y)+ty+yy;
        if(sx<0||sy<0||sx>=tw||sy>=th)continue;
        uint32_t pixel=uint32_t(get_px(*src,tw,th,int(sx),int(sy)));
        if(target>0&&size_t(target)<VMS_HEAP(g_heap).size()&&size_t(sy)*tw+size_t(sx)<VMS_HEAP(g_heap)[target].alpha.size())pixel=(pixel&0xffffff)|(uint32_t(VMS_HEAP(g_heap)[target].alpha[size_t(sy)*tw+size_t(sx)])<<24);
        int32_t value;std::memcpy(&value,&pixel,sizeof(value));
        e.set(size_t(idx),Value::of_i(value));
    }
}
static void gfx_bind(int gid, int target, int color) {
    VMS_RS(g_gfx_target)[gid] = target;
    VMS_RS(g_gfx_color)[gid] = color;
    gfx_opacity_set(gid,255);
    VMS_RS(g_gfx_tx)[gid] = 0;
    VMS_RS(g_gfx_ty)[gid] = 0;
    int w=0,h=0;pixels_of(target,w,h);
    VMS_RS(g_clip)[gid] = Clip{0,0,w,h,true};
}
// Display metadata uses existing J2S5 object payloads, not new Java fields.
struct ScreenMeta { int w=240,h=320;bool full=false,notified=true; };
static ScreenMeta screen_meta(const std::string& tag) {
    if(tag.size()!=10||tag.compare(0,4,"DSB1")!=0)return {};
    return {int(uint8_t(tag[4])|(uint16_t(uint8_t(tag[5]))<<8)),int(uint8_t(tag[6])|(uint16_t(uint8_t(tag[7]))<<8)),tag[8]!=0,tag[9]!=0};
}
static void screen_meta_set(int ref,ScreenMeta m) {
    auto& tag=VMS_HEAP(g_heap)[ref].text;tag="DSB1";
    tag.push_back(char(m.w));tag.push_back(char(m.w>>8));tag.push_back(char(m.h));tag.push_back(char(m.h>>8));
    tag.push_back(char(m.full));tag.push_back(char(m.notified));
}
static void screen_notify_size(int target) {
    ScreenMeta meta=screen_meta(VMS_HEAP(g_heap)[target].text);
    if(meta.notified&&meta.w==VMS_RS(g_screen_w)&&meta.h==VMS_RS(g_screen_h)) {
        screen_meta_set(target,meta);return; // default initial 240x320 keeps its old callback order
    }
    meta.w=VMS_RS(g_screen_w);meta.h=VMS_RS(g_screen_h);meta.notified=true;screen_meta_set(target,meta);
    if(Method* m=find_method(VMS_HEAP(g_heap)[target].cls,"sizeChanged","(II)V",true))run_event_callback(m,{Value::of_ref(target),Value::of_i(meta.w),Value::of_i(meta.h)});
}
static std::string g_canvas_notice;
static void select_screen_sidecar(const std::string& jar,const std::string& identity="") {
    (void)identity;g_canvas_notice.clear();
    int w=240,h=320;const int result=logical_screen::sidecar(jar,w,h);
    const char* source=result==1?"explicit_sidecar":result<0?"invalid_sidecar_default":"default";
    if(result==0){
        const auto hint=canvas_resolution::cold_launch(jar,VMS_CL(g_props));
        const auto fixed=canvas_declared_size::inspect(VMS_CL(g_jar_files));
        w=hint.w;h=hint.h;source=hint.source;
        if(fixed.found){
            if(std::string(hint.source)!="default"&&(hint.w!=fixed.w||hint.h!=fixed.h))g_canvas_notice="尺寸标注冲突，采用游戏固定画布 "+std::to_string(fixed.w)+"×"+std::to_string(fixed.h);
            w=fixed.w;h=fixed.h;source="canvas_constant_getters";
        }else if(fixed.conflict){
            w=240;h=320;source="ambiguous_canvas_default";
        }
        const auto filename=canvas_resolution::filename_size(jar);
        if(g_canvas_notice.empty()&&std::string(source)!="filename_explicit"&&std::string(filename.source)!="default"&&(filename.w!=w||filename.h!=h))g_canvas_notice="尺寸标注冲突，采用游戏声明 "+std::to_string(w)+"×"+std::to_string(h);
    }
    VMS_RS(g_screen_w)=w;VMS_RS(g_screen_h)=h;
    log_line("CANVAS size="+std::to_string(w)+"x"+std::to_string(h)+" source="+source+" auto_size=cold_launch_only resume=saved_dimensions");
    if(!g_canvas_notice.empty())log_line("DISPLAY notice="+g_canvas_notice);
}

static int game_buffer(int canvas) {
    auto it = VMS_RS(g_offscreen).find(canvas);
    if (it != VMS_RS(g_offscreen).end() && it->second > 0 && it->second < (int)VMS_HEAP(g_heap).size() && !VMS_HEAP(g_heap)[it->second].pixels.empty())
        return it->second;
    int cid = ensure_class("javax/microedition/lcdui/Image");
    int ref = alloc_obj(cid);
    int w = VMS_RS(g_screen_w) > 0 ? VMS_RS(g_screen_w) : 240;
    int h = VMS_RS(g_screen_h) > 0 ? VMS_RS(g_screen_h) : 320;
    VMS_HEAP(g_heap)[ref].width = w; VMS_HEAP(g_heap)[ref].height = h;
    VMS_HEAP(g_heap)[ref].pixels.assign((size_t)w * h * 3, 255);
    image_layout_dirty(ref);
    if (Field* f = find_field(cid, "width", "I")) VMS_HEAP(g_heap)[ref].fields[f->slot] = Value::of_i(w);
    if (Field* f = find_field(cid, "height", "I")) VMS_HEAP(g_heap)[ref].fields[f->slot] = Value::of_i(h);
    if (Field* f = find_field(cid, "mutable", "Z")) VMS_HEAP(g_heap)[ref].fields[f->slot] = Value::of_i(1);
    VMS_RS(g_offscreen)[canvas] = ref;
    return ref;
}
static bool show_buffer(int image, int x, int y, int width, int height) {
    if (image <= 0 || image >= (int)VMS_HEAP(g_heap).size() || width <= 0 || height <= 0) return false;
    Obj& src = VMS_HEAP(g_heap)[image];
    if (src.pixels.empty() || src.width <= 0 || src.height <= 0 || VMS_RS(g_screen_w) <= 0 || VMS_RS(g_screen_h) <= 0) return false;
    const int left = int(std::max<int64_t>(0, x)), top = int(std::max<int64_t>(0, y));
    const int64_t right = std::min<int64_t>(std::min(src.width, VMS_RS(g_screen_w)), int64_t(x) + width);
    const int64_t bottom = std::min<int64_t>(std::min(src.height, VMS_RS(g_screen_h)), int64_t(y) + height);
    if (right <= left || bottom <= top) return false;
    if ((int)VMS_RS(g_fb).size() != VMS_RS(g_screen_w) * VMS_RS(g_screen_h) * 3) VMS_RS(g_fb).assign((size_t)VMS_RS(g_screen_w) * VMS_RS(g_screen_h) * 3, 0);
    for (int row = top; row < bottom; ++row)
        std::memcpy(&VMS_RS(g_fb)[(size_t(row) * VMS_RS(g_screen_w) + left) * 3],
                    &src.pixels[(size_t(row) * src.width + left) * 3], size_t(right - left) * 3);
    VMS_RS(g_fb_dirty) = true;
    return true;
}
static int game_key_action(int code) {
    // Keep the physical-key mapping consistent with Canvas.getGameAction.
    if (code == -1 || code == 50 || code == 1) return 1;
    if (code == -2 || code == 56 || code == 6) return 6;
    if (code == -3 || code == 52 || code == 2) return 2;
    if (code == -4 || code == 54 || code == 5) return 5;
    if (code == -5 || code == 53 || code == 8) return 8;
    if (code == 49 || code == 9) return 9;
    if (code == 51 || code == 10) return 10;
    if (code == 55 || code == 11) return 11;
    if (code == 57 || code == 12) return 12;
    return 0;
}
static Value* game_canvas_state(int canvas, const char* name, const char* desc = "I") {
    if (canvas <= 0 || canvas >= int(VMS_HEAP(g_heap).size()) || VMS_HEAP(g_heap)[canvas].cls < 0) return nullptr;
    if (!type_compatible(canvas, "javax/microedition/lcdui/game/GameCanvas")) return nullptr;
    const int owner = VMS_CL(g_class_index).at("javax/microedition/lcdui/game/GameCanvas");
    Field* field = find_field(owner, name, desc);
    return field ? &VMS_HEAP(g_heap)[canvas].fields[field->slot] : nullptr;
}
static void reset_game_keys(int canvas) {
    if (Value* held = game_canvas_state(canvas, "keyHeld")) *held = Value{};
    if (Value* latched = game_canvas_state(canvas, "keyLatched")) *latched = Value{};
}
#define J2ME_WAR_DOUBLETAP_VM
#undef J2ME_WAR_DOUBLETAP_VM
static bool decode_png(const std::vector<uint8_t>& png, std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha, int& w, int& h, std::string& err) {
    static const uint8_t signature[8]={137,80,78,71,13,10,26,10};
    if(png.size()<8||std::memcmp(png.data(),signature,8)!=0){err="not png";return false;}
    int bit_depth = 0, color = 0, interlace = 0;
    std::vector<uint8_t> idat, plte, trns;
    size_t i = 8;
    while (i + 12 <= png.size()) {
        uint32_t n = ru32(&png[i]);
        if (n > png.size() - i - 12) { err = "bad png chunk"; return false; }
        std::string type((const char*)&png[i + 4], 4);
        const uint8_t* body = &png[i + 8];
        if(type=="IHDR") {
            if(n!=13||body[10]!=0||body[11]!=0||body[12]>1){err="bad png header";return false;}
            w=(int)ru32(body);h=(int)ru32(body+4);bit_depth=body[8];color=body[9];interlace=body[12];
        }
        else if (type == "PLTE") plte.assign(body, body + n);
        else if (type == "tRNS") trns.assign(body, body + n);
        else if (type == "IDAT") idat.insert(idat.end(), body, body + n);
        else if (type == "IEND") break;
        i += 12 + n;
    }
    int channels = color == 2 ? 3 : color == 4 ? 2 : color == 6 ? 4 : 1;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || (color != 0 && color != 2 && color != 3 && color != 4 && color != 6)) {
        err = "png format c" + std::to_string(color) + " d" + std::to_string(bit_depth) + " i" + std::to_string(interlace);
        return false;
    }
    if ((color == 2 || color == 4 || color == 6) && bit_depth != 8) { err = "png depth"; return false; }
    if (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8) { err = "png depth"; return false; }
    // PNG permits all zlib window sizes up to 32 KiB, not only CMF 0x78.
    if (idat.size() < 8 || (idat[0] & 15) != 8 || (idat[0] >> 4) > 7 ||
        (((unsigned)idat[0] << 8) | idat[1]) % 31 != 0 || (idat[1] & 32)) {
        err = "png zlib"; return false;
    }
    static const int a7r[7] = {0, 0, 4, 0, 2, 0, 1};
    static const int a7c[7] = {0, 4, 0, 2, 0, 1, 0};
    static const int a7ri[7] = {8, 8, 8, 4, 4, 2, 2};
    static const int a7ci[7] = {8, 8, 4, 4, 2, 2, 1};
    int passes = interlace ? 7 : 1;
    auto pass_dim = [&](int pass, int& rows, int& cols) {
        if (!interlace) { rows = h; cols = w; return; }
        rows = h > a7r[pass] ? (h - a7r[pass] + a7ri[pass] - 1) / a7ri[pass] : 0;
        cols = w > a7c[pass] ? (w - a7c[pass] + a7ci[pass] - 1) / a7ci[pass] : 0;
    };
    size_t raw_size = 0;
    for (int pass = 0; pass < passes; ++pass) {
        int rows = 0, cols = 0; pass_dim(pass, rows, cols);
        if (!rows || !cols) continue;
        raw_size += (size_t)rows * (1 + (cols * channels * bit_depth + 7) / 8);
    }
    std::vector<uint8_t> raw(raw_size ? raw_size : 1);
    unsigned long dlen = raw.size(), slen = idat.size() - 6;
    bool inflated=false;
#if J2ME_OPT_FAST_PNG
    // Reuse the already bundled stb decoder's bounded Huffman fast table.
    // Keep the existing PNG parser (palettes, Adam7, filters and tRNS), and
    // the strict ZIP decompressor unchanged. Adler mismatch/unsupported input
    // falls back to puff rather than silently changing legacy compatibility.
    if(raw_size<=size_t(INT_MAX) && slen<=unsigned(INT_MAX)) {
        const int got=stbi_zlib_decode_noheader_buffer(reinterpret_cast<char*>(raw.data()),int(raw_size),
            reinterpret_cast<const char*>(idat.data()+2),int(slen));
        if(got>=0 && size_t(got)==raw_size) {
            uint32_t a=1,b=0;
            for(size_t off=0;off<raw_size;) {
                const size_t end=std::min(raw_size,off+5552);
                while(off<end){a+=raw[off++];b+=a;}
                a%=65521u;b%=65521u;
            }
            inflated=((b<<16)|a)==ru32(idat.data()+idat.size()-4);
            if(inflated)++VMS_RS(g_png_fast);
        }
    }
#endif
    if(!inflated) {
        ++VMS_RS(g_png_fallback);
        if (puff(raw.data(), &dlen, idat.data() + 2, &slen) != 0 || dlen < raw_size) { err = "png inflate"; return false; }
    }
    rgb.assign((size_t)w * h * 3, 0);
    alpha.assign((size_t)w * h, 255);
    auto paeth = [](int a, int b, int c) {
        int pp = a + b - c, pa = std::abs(pp - a), pb = std::abs(pp - b), pc = std::abs(pp - c);
        if (pa <= pb && pa <= pc) return a;
        return pb <= pc ? b : c;
    };
    size_t p = 0;
    for (int pass = 0; pass < passes; ++pass) {
        int rows = 0, cols = 0; pass_dim(pass, rows, cols);
        if (!rows || !cols) continue;
        int row_bytes = (cols * channels * bit_depth + 7) / 8;
        int filter_bpp = std::max(1, (channels * bit_depth) / 8);
        std::vector<uint8_t> prev(row_bytes, 0), cur(row_bytes, 0);
        for (int ry = 0; ry < rows; ++ry) {
            if (p >= raw.size()) { err = "png short"; return false; }
            int filter = raw[p++];
            for (int x = 0; x < row_bytes; ++x) {
                int v = raw[p++], a = x >= filter_bpp ? cur[x - filter_bpp] : 0, b = prev[x], c = x >= filter_bpp ? prev[x - filter_bpp] : 0;
                if (filter == 1) v += a; else if (filter == 2) v += b; else if (filter == 3) v += (a + b) / 2; else if (filter == 4) v += paeth(a, b, c);
                else if (filter) { err = "png filter"; return false; }
                cur[x] = (uint8_t)v;
            }
            auto sample = [&](int x) {
                int bit = x * bit_depth;
                int byte = cur[bit >> 3];
                int shift = 8 - bit_depth - (bit & 7);
                return (byte >> shift) & ((1 << bit_depth) - 1);
            };
            for (int rx = 0; rx < cols; ++rx) {
                int r = 0, g = 0, b = 0, a = 255, s = sample(rx);
                if (color == 0) {
                    r = g = b = bit_depth == 8 ? s : s * 255 / ((1 << bit_depth) - 1);
                    if (trns.size() >= 2 && s == (int)((trns[0] << 8) | trns[1])) a = 0;
                } else if (color == 2) {
                    r = cur[rx * 3]; g = cur[rx * 3 + 1]; b = cur[rx * 3 + 2];
                    if (trns.size() >= 6 && r == trns[1] && g == trns[3] && b == trns[5]) a = 0;
                } else if (color == 3) {
                    if (s * 3 + 2 < (int)plte.size()) { r = plte[s * 3]; g = plte[s * 3 + 1]; b = plte[s * 3 + 2]; }
                    if (s < (int)trns.size()) a = trns[s];
                } else if (color == 4) {
                    r = g = b = cur[rx * 2]; a = cur[rx * 2 + 1];
                } else {
                    r = cur[rx * 4]; g = cur[rx * 4 + 1]; b = cur[rx * 4 + 2]; a = cur[rx * 4 + 3];
                }
                int x = interlace ? a7c[pass] + rx * a7ci[pass] : rx;
                int y = interlace ? a7r[pass] + ry * a7ri[pass] : ry;
                size_t di = ((size_t)y * w + x);
                rgb[di * 3] = (uint8_t)r; rgb[di * 3 + 1] = (uint8_t)g; rgb[di * 3 + 2] = (uint8_t)b;
                alpha[di] = (uint8_t)a;
            }
            prev.swap(cur);
        }
    }
    return true;
}
static bool gif_subblocks(const std::vector<uint8_t>& g, size_t& p, std::vector<uint8_t>& out, std::string& err) {
    out.clear();
    while (p < g.size()) {
        int n = g[p++];
        if (n == 0) return true;
        if (p + (size_t)n > g.size()) { err = "gif sub"; return false; }
        out.insert(out.end(), g.begin() + p, g.begin() + p + n);
        p += n;
    }
    err = "gif trunc";
    return false;
}
static bool decode_gif(const std::vector<uint8_t>& gif, std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha, int& w, int& h, std::string& err) {
    if (gif.size() < 13 || gif[0] != 'G' || gif[1] != 'I' || gif[2] != 'F') { err = "not gif"; return false; }
    int sw = gif[6] | (gif[7] << 8), sh = gif[8] | (gif[9] << 8);
    int packed = gif[10];
    size_t p = 13;
    std::vector<uint8_t> gct;
    if (packed & 0x80) {
        int n = 1 << ((packed & 7) + 1);
        if (p + (size_t)n * 3 > gif.size()) { err = "gif gct"; return false; }
        gct.assign(gif.begin() + p, gif.begin() + p + n * 3);
        p += (size_t)n * 3;
    }
    int trans = -1, iw = 0, ih = 0, left = 0, top = 0, interlace = 0;
    std::vector<uint8_t> indices, palette = gct;
    bool got = false;
    while (p < gif.size() && !got) {
        uint8_t tag = gif[p++];
        if (tag == 0x3B) break;
        if (tag == 0x21) {
            if (p >= gif.size()) { err = "gif ext"; return false; }
            uint8_t label = gif[p++];
            if (label == 0xF9) {
                if (p >= gif.size()) { err = "gif gce"; return false; }
                int sz = gif[p++];
                if (p + (size_t)sz > gif.size()) { err = "gif gce"; return false; }
                if (sz >= 4 && (gif[p] & 1)) trans = gif[p + 3];
                p += sz;
                if (p < gif.size() && gif[p] == 0) p++;
            } else {
                std::vector<uint8_t> skip;
                if (!gif_subblocks(gif, p, skip, err)) return false;
            }
        } else if (tag == 0x2C) {
            if (p + 9 > gif.size()) { err = "gif img"; return false; }
            left = gif[p] | (gif[p + 1] << 8); top = gif[p + 2] | (gif[p + 3] << 8);
            iw = gif[p + 4] | (gif[p + 5] << 8); ih = gif[p + 6] | (gif[p + 7] << 8);
            int pk = gif[p + 8]; p += 9;
            interlace = (pk & 0x40) ? 1 : 0;
            palette = gct;
            if (pk & 0x80) {
                int n = 1 << ((pk & 7) + 1);
                if (p + (size_t)n * 3 > gif.size()) { err = "gif lct"; return false; }
                palette.assign(gif.begin() + p, gif.begin() + p + n * 3);
                p += (size_t)n * 3;
            }
            if (p >= gif.size() || iw <= 0 || ih <= 0 || palette.size() < 3) { err = "gif img"; return false; }
            int min_code = gif[p++];
            if (min_code < 2 || min_code > 8) { err = "gif lzw"; return false; }
            std::vector<uint8_t> data;
            if (!gif_subblocks(gif, p, data, err)) return false;
            int clear = 1 << min_code, eoi = clear + 1, code_size = min_code + 1, next = eoi + 1;
            std::vector<std::vector<uint8_t>> dict(4096);
            for (int i = 0; i < clear; ++i) dict[i] = {(uint8_t)i};
            uint32_t acc = 0; int bits = 0, di = 0, prev = -1;
            auto read_code = [&]() -> int {
                while (bits < code_size) {
                    if (di >= (int)data.size()) return eoi;
                    acc |= (uint32_t)data[di++] << bits;
                    bits += 8;
                }
                int code = (int)(acc & ((1u << code_size) - 1));
                acc >>= code_size; bits -= code_size;
                return code;
            };
            indices.clear();
            indices.reserve((size_t)iw * ih);
            while ((int)indices.size() < iw * ih) {
                int code = read_code();
                if (code == eoi) break;
                if (code == clear) {
                    code_size = min_code + 1; next = eoi + 1; prev = -1;
                    for (int i = eoi + 1; i < 4096; ++i) dict[i].clear();
                    continue;
                }
                std::vector<uint8_t> entry;
                if (code < next && !dict[code].empty()) entry = dict[code];
                else if (code == next && prev >= 0) { entry = dict[prev]; entry.push_back(dict[prev][0]); }
                else { err = "gif code"; return false; }
                indices.insert(indices.end(), entry.begin(), entry.end());
                if (prev >= 0 && next < 4096) {
                    dict[next] = dict[prev];
                    dict[next].push_back(entry[0]);
                    next++;
                    if (next == (1 << code_size) && code_size < 12) code_size++;
                }
                prev = code;
            }
            got = true;
        } else { err = "gif block"; return false; }
    }
    if (!got) { err = "gif empty"; return false; }
    w = std::max(sw, left + iw); h = std::max(sh, top + ih);
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) { err = "gif size"; return false; }
    rgb.assign((size_t)w * h * 3, 0);
    alpha.assign((size_t)w * h, 0);
    auto put = [&](int x, int y, int index) {
        if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
        size_t di = (size_t)y * w + x;
        if (index == trans) { alpha[di] = 0; return; }
        int pi = index * 3;
        if (pi + 2 < (int)palette.size()) { rgb[di * 3] = palette[pi]; rgb[di * 3 + 1] = palette[pi + 1]; rgb[di * 3 + 2] = palette[pi + 2]; }
        alpha[di] = 255;
    };
    if (!interlace) {
        for (int y = 0; y < ih; ++y)
            for (int x = 0; x < iw; ++x) {
                size_t k = (size_t)y * iw + x;
                put(left + x, top + y, k < indices.size() ? indices[k] : 0);
            }
    } else {
        int starts[4] = {0, 4, 2, 1}, steps[4] = {8, 8, 4, 2}, row = 0;
        for (int pass = 0; pass < 4; ++pass)
            for (int y = starts[pass]; y < ih; y += steps[pass], ++row)
                for (int x = 0; x < iw; ++x) {
                    size_t k = (size_t)row * iw + x;
                    put(left + x, top + y, k < indices.size() ? indices[k] : 0);
                }
    }
    return true;
}
static bool decode_image(const std::vector<uint8_t>& bytes, std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha, int& w, int& h, std::string& err) {
    P7_SCOPE(Decode);
    if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xd8)
        return j2me_image::decode_jpeg(bytes, rgb, alpha, w, h, err);
    std::string png_err, gif_err;
    if (decode_png(bytes, rgb, alpha, w, h, png_err)) return true;
    if (decode_gif(bytes, rgb, alpha, w, h, gif_err)) return true;
    err = png_err + "; " + gif_err;
    return false;
}
static void install_image(Method* m, std::vector<uint8_t> rgb, std::vector<uint8_t> alpha, int w, int h, Frame& fr, bool mutable_image=false) {
    const int cid=ensure_class("javax/microedition/lcdui/Image");
    int ref = alloc_obj(cid);
    VMS_HEAP(g_heap)[ref].width = w; VMS_HEAP(g_heap)[ref].height = h; VMS_HEAP(g_heap)[ref].pixels = std::move(rgb);
    bool solid = true;
    for (uint8_t a : alpha) if (a != 255) { solid = false; break; }
    if (!solid) VMS_HEAP(g_heap)[ref].alpha = std::move(alpha);
    image_layout_dirty(ref);
    if (Field* f = find_field(cid, "width", "I")) VMS_HEAP(g_heap)[ref].fields[f->slot] = Value::of_i(w);
    if (Field* f = find_field(cid, "height", "I")) VMS_HEAP(g_heap)[ref].fields[f->slot] = Value::of_i(h);
    if (Field* f = find_field(cid, "mutable", "Z")) VMS_HEAP(g_heap)[ref].fields[f->slot] = Value::of_i(mutable_image);
    fr.stack.push_back(Value::of_ref(ref));
}
static int32_t int_bits(uint32_t v) { int32_t r; std::memcpy(&r, &v, 4); return r; }
#include "stream_intrinsics.h"
#include "runtime/library_intrinsics.inc"
static int64_t long_bits(uint64_t v) { int64_t r; std::memcpy(&r, &v, 8); return r; }
template<class T> static T java_fp_integer(double v) {
    if (std::isnan(v)) return 0;
    if (v >= (double)std::numeric_limits<T>::max()) return std::numeric_limits<T>::max();
    if (v <= (double)std::numeric_limits<T>::min()) return std::numeric_limits<T>::min();
    return (T)v;
}
static int64_t as_long(const Value& v) { return v.is_long ? v.l : (int64_t)v.i; }
static float bits_f(int32_t i) { float f; std::memcpy(&f, &i, 4); return f; }
static int32_t f_bits(float f) { int32_t i; std::memcpy(&i, &f, 4); return i; }
static double bits_d(int64_t v) { double d; std::memcpy(&d, &v, 8); return d; }
static int64_t d_bits(double d) { int64_t v; std::memcpy(&v, &d, 8); return v; }
static int fcmp(float a, float b, int nan) {
    if (a != a || b != b) return nan;
    return a > b ? 1 : a < b ? -1 : 0;
}
static int dcmp(double a, double b, int nan) {
    if (a != a || b != b) return nan;
    return a > b ? 1 : a < b ? -1 : 0;
}

static const std::vector<uint8_t>* jar_lookup(std::string path) {
    auto scheme = path.find("://");
    if (scheme != std::string::npos) path = path.substr(scheme + 3);
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    auto it = VMS_CL(g_jar_files).find(path);
    if (it == VMS_CL(g_jar_files).end()) {
        auto slash = path.rfind('/');
        if (slash != std::string::npos) it = VMS_CL(g_jar_files).find(path.substr(slash + 1));
    }
    if (it == VMS_CL(g_jar_files).end()) return nullptr;
    return &it->second;
}
static int stream_from(const std::vector<uint8_t>& data) {
    int bytes = alloc_array("[B", data.size()), ref = 0;
    RootScope roots(ReentrySite::StreamFrom, {&bytes, &ref}); // S5: bytes is held across ensure_class (<clinit> may run Java)
    if (!data.empty()) std::memcpy(VMS_HEAP(g_heap)[bytes].elems.byte_data(), data.data(), data.size());
    int cid = ensure_class("java/io/ByteArrayInputStream");
    ref = alloc_obj(cid);
    if (Method* init = find_method(cid, "<init>", "([B)V", false)) run(init, {Value::of_ref(ref), Value::of_ref(bytes)});
    return ref;
}
#include "threading.h"
#include "m3g_support.h"
#include "m3g_geometry.h"
#include "m3g_texture238.h"
#include "m3g_span239.h"
#include "m3g_row240.h"
#include "m3g_image241.h"
#include "m3g_sprite241.h"
#include "m3g_mesh242.h"
#include "m3g_lighting242.h"
#include "m3g_assembly242.h"

#include "runtime/midp_bridge.inc"

static void place_args(std::vector<Value>& locals, const std::vector<Value>& a) {
    int slot = 0;
    for (auto& v : a) {
        if (slot < (int)locals.size()) locals[slot] = v;
        slot += v.is_long ? 2 : 1;
    }
}
#include "runtime/native_jit.inc"
#include "paint_hotspots241.h"
#include "runtime/deterministic_cost.inc"
#include "runtime/interpreter.inc"
#include "runtime_diagnostics.inc"
#include "causal_diagnostics.inc"

#include "runtime/class_loader.inc"

#include "p7_present_measure.h"
#include "gc_roots.h"
// Mark phase (roots from every module's roots(), then trace). Also the test hook for root-set comparisons.
static void gc_mark_phase(std::vector<char>& mark, std::vector<int>& work, bool allow_parallel = true) {
    auto root = [&](int ref) {
        if (ref <= 0 || ref >= (int)VMS_HEAP(g_heap).size() || mark[ref]) return;
        mark[ref] = 1; work.push_back(ref);
    };
    auto root_val = [&](const Value& v) { if (v.ref > 0) root(v.ref); };
    struct Visitor {
        decltype(root)& r; decltype(root_val)& rv;
        void ref(int x) { r(x); } void val(const Value& x) { rv(x); }
    } visitor{root, root_val};
    j2me_gc::all_roots(visitor);
    const bool p7_adapt = allow_parallel && p7gc::g_enabled && VMS_HEAP(g_heap).size() >= p7gc::MinSlots;
    const int p7_mode = p7_adapt ? (r4::state.active?1:p7gc::g_adapt.choose()) : 0;
    const auto p7_t0 = std::chrono::steady_clock::now();
    auto p7_done = [&](int mode) { if (!p7_adapt) return; const uint64_t ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - p7_t0).count());
        p7gc::g_adapt.observe(mode, ns, VMS_HEAP(g_heap).size()); p7gc::g_trace_ns[mode] += ns; ++p7gc::g_trace_n[mode]; };
    if (p7_adapt && p7_mode == 1) { // P7.4 trace on the workers; P7.5 only when the adaptive rule says it is faster
        const size_t slots = VMS_HEAP(g_heap).size();
        const auto& heap = VMS_HEAP(g_heap); const auto& targets = VMS_RS(g_gfx_target); const auto& buffers = VMS_RS(g_offscreen);
        auto children = [&heap, &targets, &buffers](int ref, auto& push) -> uint64_t { // same edges as the serial loop below
            auto target = targets.find(ref); if (target != targets.end()) push(target->second);
            auto buffer = buffers.find(ref); if (buffer != buffers.end()) push(buffer->second);
            const Obj& o = heap[ref];
            for (auto& v : o.fields) if (v.ref > 0) push(v.ref);
            if (o.is_array && o.elems.references()) { for (size_t i = 0; i < o.elems.size(); ++i) push(o.elems.get(i).ref); return o.elems.size(); }
            return 0;
        };
        p7gc::Shared shared; shared.pool = work; char* m = mark.data();
        uint64_t t[p7::Pool::Workers];
        for (int w = 0; w < p7::Pool::Workers; ++w) t[w] = p7::pool().submit([&shared, m, slots, &children] { p7gc::trace_worker(shared, m, slots, children); }, w);
        { P7_SCOPE(GcWait); for (int w = 0; w < p7::Pool::Workers; ++w) p7::pool().wait(t[w]); }
        VMS_HEAP(g_gc_array_refs_scanned) += shared.scanned.load();
        ++p7gc::g_parallel; p7_done(1);
        if (p7gc::g_verify) { // debug compare: serial mark of the same heap, slot by slot
            const uint64_t scanned_before = VMS_HEAP(g_gc_array_refs_scanned);
            std::vector<char> serial(mark.size(), 0); std::vector<int> swork;
            gc_mark_phase(serial, swork, false);
            VMS_HEAP(g_gc_array_refs_scanned) = scanned_before;
            ++p7gc::g_verify_runs; uint64_t bad = 0; int first = -1;
            for (size_t i = 0; i < mark.size(); ++i) if (mark[i] != serial[i]) { if (first < 0) first = int(i); ++bad; }
            if (bad) { p7gc::g_verify_bad_slots += bad; log_line("P7GC_VERIFY mismatch slots=" + std::to_string(bad) + " first=" + std::to_string(first)); }
        }
        return;
    }
    if (allow_parallel && p7gc::g_enabled) ++p7gc::g_serial_small;
    struct P7SerialDone { decltype(p7_done)& f; bool on; ~P7SerialDone() { if (on) f(0); } } p7_serial_done{p7_done, p7_adapt};
    for (size_t sp = 0; sp < work.size(); ++sp) {
        const int ref = work[sp];
        // Native associations are references owned by their Java object, not
        // independent roots: a live Graphics retains its Image, and a live
        // GameCanvas retains its buffer.
        auto target = VMS_RS(g_gfx_target).find(ref);
        if (target != VMS_RS(g_gfx_target).end()) root(target->second);
        auto buffer = VMS_RS(g_offscreen).find(ref);
        if (buffer != VMS_RS(g_offscreen).end()) root(buffer->second);
        Obj& o = VMS_HEAP(g_heap)[ref];
        for (auto& v : o.fields) root_val(v);
        if (o.is_array && o.elems.references()) {
            VMS_HEAP(g_gc_array_refs_scanned) += o.elems.size();
            for (size_t i = 0; i < o.elems.size(); ++i) root(o.elems.get(i).ref);
        }
    }
}
static void gc_collect(const std::string& why) { P61_SCOPE(3, nullptr); P7_SCOPE(Gc);
    const auto diagnostic_phase=diag_phase_enter(runtime_diag::Phase::GC);
    RuntimeProfileScope gc(VMS_EXEC(g_runtime_profile),RuntimeProfile::Kind::GC);
    ++VMS_HEAP(g_gc_cycles);
    const int64_t gc_started_us=paint_clock_us();
#if J2ME_OPT_GC_SCRATCH
    auto& mark=VMS_HEAP(g_gc_mark_scratch);
    mark.assign(VMS_HEAP(g_heap).size(),0);
    auto& work=VMS_HEAP(g_gc_work_scratch);
    work.clear();
    if(work.capacity()<std::min<size_t>(VMS_HEAP(g_heap).size(),8192))work.reserve(std::min<size_t>(VMS_HEAP(g_heap).size(),8192));
#else
    std::vector<char> mark(VMS_HEAP(g_heap).size(),0);
    std::vector<int> work;
#endif
    gc_mark_phase(mark, work);
    // Derived indexes are weak: discard dead array identities BEFORE reuse.
    VMS_NATIVE(g_library_indexes).sweep([&](int ref) { return ref>0 && size_t(ref)<mark.size() && mark[ref]; });
    int freed = 0, live = 0;
    for (int i = 1; i < (int)VMS_HEAP(g_heap).size(); ++i) {
        if (mark[i]) { live++; continue; }
        bool vacant = VMS_HEAP(g_heap)[i].cls < 0 && !VMS_HEAP(g_heap)[i].is_array && VMS_HEAP(g_heap)[i].text.empty() && VMS_HEAP(g_heap)[i].pixels.empty() &&
                      VMS_HEAP(g_heap)[i].fields.empty() && VMS_HEAP(g_heap)[i].elems.empty();
        if (vacant) continue;
        VMS_HEAP(g_array_payload_live) -= VMS_HEAP(g_heap)[i].elems.payload_bytes();
        VMS_HEAP(g_heap)[i].elems.recycle_dead_primitive(VMS_HEAP(g_primitive_buffers));
        VMS_DIAG(g_exception_traces).erase(i);
        { const uint32_t gen = VMS_HEAP(g_heap)[i].generation + 1; VMS_HEAP(g_heap)[i] = Obj{}; VMS_HEAP(g_heap)[i].generation = gen; ++resver::g_stats.generations; } // P2.5: freed slot gets a new generation
        VMS_EXEC(g_monitors).erase(i);
        VMS_RS(g_offscreen).erase(i);
        VMS_RS(g_gfx_color).erase(i); VMS_RS(g_gfx_target).erase(i); VMS_RS(g_gfx_tx).erase(i); VMS_RS(g_gfx_ty).erase(i); VMS_RS(g_clip).erase(i);
        VMS_HEAP(g_free).push_back(i); freed++;
    }
    if (freed > 0 && (++VMS_HEAP(g_gc_logs) <= 2 || (VMS_HEAP(g_gc_logs) % 20) == 0))
        log_line("GC " + why + " live=" + std::to_string(live) + " freed=" + std::to_string(freed) +
                 " slots=" + std::to_string(VMS_HEAP(g_heap).size()) + " freelist=" + std::to_string(VMS_HEAP(g_free).size()) +
                 " array_bytes=" + std::to_string(VMS_HEAP(g_array_payload_live)) + " array_peak=" + std::to_string(VMS_HEAP(g_array_payload_peak)));
    VMS_HEAP(g_alloc_since) = 0;
    VMS_HEAP(g_array_alloc_since_gc) = 0;
    VMS_RS(g_frame_metrics).pending.gc_pauses.add(uint64_t(std::max<int64_t>(0,paint_clock_us()-gc_started_us)));
    work.clear(); // No stale object ids can become roots of a later collection.
    diag_native_leave(diagnostic_phase);
}
// P2.2-S6: the GC safepoint, formalised (was maybe_gc). Same two call sites, same thresholds, same decision:
// GC frequency and memory behaviour are unchanged. Interpreter slice points remain poll-only (no GC there).
// Added: always-on counters and the formal GC-safe check (depth, executing threads, unowned root stacks,
// open RootScopes) right before a collection; a violation is counted and logged, never acted on.
enum class GcPoint : uint8_t { Threads, Paint, Count };
struct GcSafepointStats { uint64_t calls[2]{}, skipped_depth[2]{}, below_threshold[2]{}, collections[2]{}, violations=0, v_depth=0, v_executing=0, v_root_owner=0, v_rootscope=0; size_t peak_slots=0; };
static GcSafepointStats g_gc_sp;
static bool gc_safepoint_conditions() {
    bool ok = true;
    if (VMS_EXEC(g_run_depth) != 0 || VMS_EXEC(g_pump_depth) != 0 || VMS_RS(g_paint_depth) != 0) { ok = false; ++g_gc_sp.v_depth; }
    for (auto& t : VMS_EXEC(g_threads)) if (t.executing) { ok = false; ++g_gc_sp.v_executing; break; }
    for (auto& p : VMS_HEAP(g_root_stacks)) if (!p.second) { ok = false; ++g_gc_sp.v_root_owner; break; }
    if (!g_native_roots.entries.empty()) { ok = false; ++g_gc_sp.v_rootscope; }
    return ok;
}
// P2.4: after a collection at a legal safepoint, check the ACTIVE frames independently of the root enumeration:
// every reference in every frame (main context, all threads in any state, every registered run stack) and every open
// RootScope entry must still name a live object. Read-only, count-only. 0.2.107: env switch J2ME_GC_VERIFY removed; tests call gc_verify_frames() directly.
struct GcVerifyStats { uint64_t runs=0, frames=0, refs=0, dead=0; std::string first_dead; };
static GcVerifyStats g_gc_verify;
static bool gc_slot_live(int r) {
    if (r <= 0 || size_t(r) >= VMS_HEAP(g_heap).size()) return false;
    const Obj& o = VMS_HEAP(g_heap)[r];
    return !(o.cls < 0 && !o.is_array && o.text.empty() && o.pixels.empty() && o.fields.empty() && o.elems.empty());
}
static void gc_verify_frames(const char* where) {
    ++g_gc_verify.runs;
    auto check = [&](int r, const char* what) { if (r <= 0) return; ++g_gc_verify.refs;
        if (!gc_slot_live(r)) { if (!g_gc_verify.dead++) g_gc_verify.first_dead = std::string(where) + ":" + what + ":" + std::to_string(r); } };
    auto frames = [&](const std::vector<Frame>* st, const char* what) { if (!st) return;
        for (auto& fr : *st) { ++g_gc_verify.frames; check(fr.sync_monitor, what); check(fr.constructed.ref, what);
            for (auto& x : fr.locals) check(x.ref, what); for (auto& x : fr.stack) check(x.ref, what); } };
    frames(&VMS_EXEC(g_main_thread).frames, "main");
    for (auto& t : VMS_EXEC(g_threads)) frames(&t.frames, "thread");
    for (auto& p : VMS_HEAP(g_root_stacks)) frames(p.first, "run_stack");
    for (auto& e : g_native_roots.entries) { if (e.ref) check(*e.ref, "rootscope"); if (e.vals) for (auto& x : *e.vals) check(x.ref, "rootscope_vals"); }
}
static void gc_verify_report() {
    if (!g_gc_verify.runs) return;
    log_line("P24OBS gc_verify runs=" + std::to_string(g_gc_verify.runs) + " frames=" + std::to_string(g_gc_verify.frames) + " refs=" + std::to_string(g_gc_verify.refs) +
        " dead=" + std::to_string(g_gc_verify.dead) + " first_dead=" + (g_gc_verify.first_dead.empty() ? "-" : g_gc_verify.first_dead) + " mode=count_only");
}
// P2.5: ResourceKey / stamps (CPU side). Consumers (JIT, background tasks) take a stamp and must revalidate it.
[[maybe_unused]] static ResourceKey resource_key_of(int ref) {
    ResourceKey k; if (ref <= 0 || size_t(ref) >= VMS_HEAP(g_heap).size()) return k;
    k.epoch = resver::g_epoch; k.slot = ref; k.generation = VMS_HEAP(g_heap)[ref].generation; return k;
}
[[maybe_unused]] static ResourceStamp resource_stamp_of(int ref) {
    ResourceStamp st; st.key = resource_key_of(ref); if (!st.key.slot) return st;
    const Obj& o = VMS_HEAP(g_heap)[ref];
    if (o.is_array) { st.layout = o.elems.layout_version(); st.content = o.elems.content_version(); }
    else { st.layout = o.image_layout_version; st.content = o.image_content_version; }
    return st;
}
[[maybe_unused]] static bool resource_key_valid(const ResourceKey& k) {
    return k.slot > 0 && k.epoch == resver::g_epoch && size_t(k.slot) < VMS_HEAP(g_heap).size() &&
           VMS_HEAP(g_heap)[k.slot].generation == k.generation && gc_slot_live(k.slot);
}
[[maybe_unused]] static bool resource_stamp_current(const ResourceStamp& st) {
    if (!resource_key_valid(st.key)) return false;
    const ResourceStamp now = resource_stamp_of(st.key.slot);
    return now.layout == st.layout && now.content == st.content;
}
// P2.5 barrier verification (count-only; 0.2.107 removed the J2ME_RES_VERIFY env switch, tests call res_verify_scan() directly): at each legal GC safepoint hash every live
// array payload and image (pixels+alpha). Same key + same content_version but different bytes = a write that
// bypassed the barrier ("missed"). Never acts; never on Switch.
struct ResVerifyShadow { uint32_t epoch = 0, generation = 0; uint64_t content = 0, layout = 0, hash = 0; };
struct ResVerifyStats { uint64_t runs = 0, checked = 0, unchanged = 0, bumped_changed = 0, bumped_same = 0, missed = 0; std::string first_missed; };
static ResVerifyStats g_res_verify;
static std::unordered_map<int, ResVerifyShadow> g_res_shadow;
static uint64_t res_hash(const uint8_t* p, size_t n, uint64_t h = 1469598103934665603ull) { for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; } return h; }
static void res_verify_scan(const char* where) {
    ++g_res_verify.runs;
    for (int r = 1; r < (int)VMS_HEAP(g_heap).size(); ++r) {
        const Obj& o = VMS_HEAP(g_heap)[r];
        if (!gc_slot_live(r) || (!o.is_array && o.pixels.empty())) { g_res_shadow.erase(r); continue; }
        uint64_t h; ResourceStamp st = resource_stamp_of(r);
        if (o.is_array) h = res_hash(o.elems.raw_payload(), o.elems.payload_bytes());
        else h = res_hash(o.alpha.data(), o.alpha.size(), res_hash(o.pixels.data(), o.pixels.size()));
        ++g_res_verify.checked;
        auto it = g_res_shadow.find(r);
        if (it != g_res_shadow.end() && it->second.epoch == st.key.epoch && it->second.generation == st.key.generation) {
            const auto& sh = it->second;
            if (sh.content == st.content && sh.layout == st.layout) {
                if (sh.hash == h) ++g_res_verify.unchanged;
                else if (!g_res_verify.missed++) g_res_verify.first_missed = std::string(where) + ":" + (o.is_array ? o.array_desc : std::string("image")) + ":" + std::to_string(r);
            } else if (sh.hash != h) ++g_res_verify.bumped_changed; else ++g_res_verify.bumped_same;
        }
        g_res_shadow[r] = ResVerifyShadow{st.key.epoch, st.key.generation, st.content, st.layout, h};
    }
}
static void resver_report() {
    const auto& s = resver::g_stats;
    log_line("P25OBS resver epoch=" + std::to_string(resver::g_epoch) + " clock=" + std::to_string(resver::g_clock) + " array_layout=" + std::to_string(s.array_layout) +
        " array_content=" + std::to_string(s.array_content) + " array_view=" + std::to_string(s.array_view) + " image_layout=" + std::to_string(s.image_layout) +
        " image_content=" + std::to_string(s.image_content) + " generations=" + std::to_string(s.generations) + " epochs=" + std::to_string(s.epochs) + " mode=count_only");
    if (g_res_verify.runs) log_line("P25OBS res_verify runs=" + std::to_string(g_res_verify.runs) + " checked=" + std::to_string(g_res_verify.checked) +
        " unchanged=" + std::to_string(g_res_verify.unchanged) + " bumped_changed=" + std::to_string(g_res_verify.bumped_changed) + " bumped_same=" + std::to_string(g_res_verify.bumped_same) +
        " missed=" + std::to_string(g_res_verify.missed) + " first_missed=" + (g_res_verify.first_missed.empty() ? "-" : g_res_verify.first_missed) + " mode=count_only");
}
static void gc_safepoint_report() {
    gc_verify_report();
    resver_report();
    const auto& g = g_gc_sp;
    log_line("P22OBS S6 gc_safepoint threads=" + std::to_string(g.calls[0]) + "/skip_depth:" + std::to_string(g.skipped_depth[0]) + "/below:" + std::to_string(g.below_threshold[0]) + "/gc:" + std::to_string(g.collections[0]) +
        " paint=" + std::to_string(g.calls[1]) + "/skip_depth:" + std::to_string(g.skipped_depth[1]) + "/below:" + std::to_string(g.below_threshold[1]) + "/gc:" + std::to_string(g.collections[1]) +
        " violations=" + std::to_string(g.violations) + " (depth=" + std::to_string(g.v_depth) + " executing=" + std::to_string(g.v_executing) + " root_owner=" + std::to_string(g.v_root_owner) +
        " rootscope=" + std::to_string(g.v_rootscope) + ") gc_cycles=" + std::to_string(VMS_HEAP(g_gc_cycles)) + " peak_slots=" + std::to_string(g.peak_slots) +
        " array_peak=" + std::to_string(VMS_HEAP(g_array_payload_peak)) + " mode=count_only");
}
static void gc_safepoint(GcPoint point) {
    const size_t pi = size_t(point); const char* why = point == GcPoint::Threads ? "threads" : "paint";
    ++g_gc_sp.calls[pi]; g_gc_sp.peak_slots = std::max(g_gc_sp.peak_slots, VMS_HEAP(g_heap).size());
    if (VMS_EXEC(g_run_depth) != 0) { ++g_gc_sp.skipped_depth[pi]; S0_HOOK(++s0.gc_skipped_run_depth); return; } // Host native continuations may still hold unregistered temporary Values.
    // GC stress (test only): J2ME_GC_STRESS=N forces a collection at the next legal safepoint (this function,
    // VMS_EXEC(g_run_depth)==0) once N objects were allocated. 0/unset = production thresholds. GC stays non-moving.
    static const int stress = [] { const char* e = std::getenv("J2ME_GC_STRESS"); const int n = e ? std::atoi(e) : 0; return n > 0 ? n : 0; }();
    if (stress > 0) {
        if (VMS_HEAP(g_alloc_since) < stress) { ++g_gc_sp.below_threshold[pi]; return; }
        if (VMS_HEAP(g_gc_cycles) % 1000 == 0) std::fprintf(stderr, "GC_STRESS every=%d forced_cycles_so_far=%llu\n", stress, (unsigned long long)VMS_HEAP(g_gc_cycles));
    }
    else
    if (VMS_HEAP(g_alloc_since) < 256 && VMS_HEAP(g_array_alloc_since_gc) < 4 * 1024 * 1024) { ++g_gc_sp.below_threshold[pi]; return; }
    S0_HOOK(s0_assert_gc_safe());
    if (!gc_safepoint_conditions() && ++g_gc_sp.violations <= 8) log_line("SAFEPOINT gc_safepoint_violation n=" + std::to_string(g_gc_sp.violations) + " point=" + why + " run_depth=" + std::to_string(VMS_EXEC(g_run_depth)) + " pump_depth=" + std::to_string(VMS_EXEC(g_pump_depth)) + " paint_depth=" + std::to_string(VMS_RS(g_paint_depth)) + " open_rootscopes=" + native_roots_open_sites() + " (counted only)");
    ++g_gc_sp.collections[pi];
    gc_collect(why);
    VMS_HEAP(g_alloc_since) = 0;
}
static void thread_resume() {
    if(r4::state.active) {
        if(VMS_PLAT(g_host_paused)||VMS_EXEC(g_midlet_state)==MidletState::Destroyed||VMS_EXEC(g_notify_destroyed))return;
        while(r4::state.remaining>0&&r4::state.worker_left>0&&pump_one_thread()){}
        update_thread_live();gc_safepoint(GcPoint::Threads);
        if(VMS_EXEC(g_pump_depth)==0&&VMS_EXEC(g_run_depth)==0)
            VMS_EXEC(g_threads).remove_if([](const JavaThreadContext& t){return t.state==ThreadState::Terminated&&!t.executing;});
        return;
    }
    P7_SCOPE(Interp);
    ++VMS_EXEC(g_save_clock).exec_epoch;
    if (VMS_PLAT(g_host_paused) || VMS_EXEC(g_midlet_state) == MidletState::Destroyed || VMS_EXEC(g_notify_destroyed)) return;
#ifndef __SWITCH__
    audio_pump();
#endif
    int64_t deadline = monotonic_ms() + VMS_EXEC(g_slice_ms);
    const int64_t prior_native_deadline=VMS_EXEC(g_exec_ctx).native_poll_host_deadline_us;
    const int64_t native_now=paint_clock_us();
    const int64_t native_deadline=native_now+int64_t(std::max(0,VMS_EXEC(g_slice_ms)))*1000;
    VMS_EXEC(g_exec_ctx).native_poll_host_deadline_us=prior_native_deadline>native_now?
        std::min(prior_native_deadline,native_deadline):native_deadline;
    VMS_EXEC(g_repaint_yield_handoff)=false;
    // A Java yield is a scheduling opportunity, not a request to wait for the
    // next displayed frame. Keep rotating runnable workers inside this host
    // slice until they block or the shared time budget expires.
    // A host callback may already have painted. Still grant one worker turn
    // so a self-rescheduling UI callback cannot starve background logic.
    while (true) {
        if (!pump_one_thread() || VMS_EXEC(g_speed_frame_open) || VMS_EXEC(g_repaint_yield_handoff) || monotonic_ms() >= deadline) break;
    }
    VMS_EXEC(g_exec_ctx).native_poll_host_deadline_us=prior_native_deadline;
    if(VMS_EXEC(g_repaint_yield_handoff))runtime_early_repaint_handoff();
    update_thread_live();
    gc_safepoint(GcPoint::Threads);
    if (VMS_EXEC(g_pump_depth) == 0 && VMS_EXEC(g_run_depth) == 0) VMS_EXEC(g_threads).remove_if([](const JavaThreadContext& t) { return t.state == ThreadState::Terminated && !t.executing; });
}
static void thread_pump() { thread_resume(); }
static void stop_thread(int p1_reason=2) {
    VMS_EXEC(g_exec_ctx).native_poll_host_deadline_us=0;
    g_timer_clock_owner=nullptr;
    if(p7::g_window_start.load(std::memory_order_relaxed)){ log_line(p7::counters_summary(p1_reason==3?"destroy":"stop")); p7::g_window_start.store(0,std::memory_order_relaxed); }
    p8_caught_report(); p8::g_caught.clear(); p8::g_caught_total=0; // P8.2: per-launch caught-exception totals
    startup_trace("stop/pool-before");
    p7::pool().drain(); // P7.2: no worker task may outlive the heap it was submitted against
    startup_trace("stop/pool-after");
    if(p7gc::g_enabled)log_line("P7GC_STATS parallel="+std::to_string(p7gc::g_parallel)+" serial="+std::to_string(p7gc::g_serial_small)+" trace_us_serial="+std::to_string(p7gc::g_trace_ns[0]/1000)+" n_serial="+std::to_string(p7gc::g_trace_n[0])+" trace_us_parallel="+std::to_string(p7gc::g_trace_ns[1]/1000)+" n_parallel="+std::to_string(p7gc::g_trace_n[1])+" probes="+std::to_string(p7gc::g_adapt.probes)+" donations="+std::to_string(p7gc::g_donations)+" verify_runs="+std::to_string(p7gc::g_verify_runs)+" verify_bad_slots="+std::to_string(p7gc::g_verify_bad_slots));
    if(p7present::g_enabled)log_line(p7present::stats());
    if(p7audio::g_enabled&&p7audio::g_jobs)log_line("P7AUDIO_STATS jobs="+std::to_string(p7audio::g_jobs)+" fences="+std::to_string(p7audio::g_fences)+" ran_inline_on_fence="+std::to_string(p7audio::g_inline)+" idle_pumps="+std::to_string(p7audio::g_idle_pumps)+" fast_query_hits="+std::to_string(p7audio::g_fast_hits)
#ifndef __SWITCH__
        +" batches="+std::to_string(p7audio::g_batches)+" frames_worker="+std::to_string(p7audio::g_frames_worker)+" frames_inline_flush="+std::to_string(p7audio::g_frames_inline)
#endif
        );
    for(auto& t:VMS_EXEC(g_threads))if(t.state!=ThreadState::Terminated){
        if(!t.frames.empty())p1_snapshot(t,t.frames.back(),t.frames.size());
        t.p1_end_reason=p1_reason;p1_thread_end(t);t.state=ThreadState::Terminated;
    }
    if(!VMS_DIAG(g_p1).caught_end_dumped){VMS_DIAG(g_p1).caught_end_dumped=true;VMS_DIAG(g_p1).caught_zero_pending=false;p1_dump_caught_last(p1_reason==3?"destroy":"reset");}
    startup_trace("stop/report-before");
    p1_report();p1_flush("reset");
    startup_trace("stop/report-after");
    VMS_DIAG(g_p1).serial_times.clear();VMS_DIAG(g_p1).repaint_since=0;
    startup_trace("stop/jit-before");
    VMS_NATIVE(g_native_jit).clear(); // host code addresses never survive suite or snapshot resets
    jit_region_reset();
    startup_trace("stop/jit-after");
    VMS_NATIVE(g_library_indexes).clear(); VMS_NATIVE(g_index_key_epoch)=0;
    VMS_CL(g_literal_cache) = LiteralCache{};
    VMS_RS(g_decoded_images).clear();
    VMS_EXEC(g_farm_startup) = FarmStartup{};
    startup_trace("stop/player-before");
    auto player_class=VMS_CL(g_class_index).find("javax/microedition/media/NativePlayer");
    if(VMS_EXEC(g_run_depth)==0 && VMS_EXEC(g_pump_depth)==0 && player_class!=VMS_CL(g_class_index).end() && VMS_CL(g_classes)[player_class->second].init_state==2) {
        if(Method* m=find_method(player_class->second,"resetAll","()V",false))run(m,{});
    }
    startup_trace("stop/player-after");
    startup_trace("stop/audio-before");
    audio_reset();
    startup_trace("stop/audio-after");
    VMS_EXEC(g_midlet_state)=MidletState::Empty; VMS_PLAT(g_host_paused)=VMS_EXEC(g_lifecycle_busy)=false;
    VMS_PLAT(g_host_frozen236)=false;VMS_PLAT(g_host_freeze_started236)=0;
    VMS_EXEC(g_notify_destroyed)=VMS_EXEC(g_notify_paused)=VMS_EXEC(g_midlet_resume)=false;
    VMS_RS(g_shown)=VMS_RS(g_alert_return)=0; VMS_RS(g_alert_deadline)=0; VMS_RS(g_alert_remaining)=-1; VMS_EXEC(g_serial_calls).clear(); cont_new_epoch(); VMS_PLAT(g_pressed_keys).clear(); VMS_PLAT(g_command_keys).clear(); VMS_PLAT(g_key_states)=0;
    VMS_PLAT(g_rms_suite).clear(); VMS_PLAT(g_rms_import_legacy) = false;
    VMS_EXEC(g_threads).clear(); VMS_EXEC(g_monitors).clear(); VMS_CL(g_class_objects).clear(); VMS_EXEC(g_main_thread) = JavaThreadContext{};
    VMS_EXEC(g_exec_ctx).event_callback_owner=nullptr;
    VMS_EXEC(g_exec_ctx).active_paint_canvas=0;
    VMS_EXEC(g_running_thread) = nullptr; VMS_HEAP(g_root_stacks).clear(); native_roots_reset(); VMS_EXEC(g_active_stack) = nullptr;
    VMS_EXEC(g_thread_live) = false; VMS_EXEC(g_resume_request) = VMS_EXEC(g_can_suspend) = false; VMS_EXEC(g_round_robin) = 0; VMS_EXEC(g_pump_depth) = 0;
    VMS_EXEC(g_repaint_owner_ref) = 0; VMS_EXEC(g_speed_frame_open) = false; VMS_EXEC(g_speed_serial_serviced) = false;
    VMS_EXEC(g_exec_ctx).frame_wait_request_owner=0;
    VMS_EXEC(g_repaint_yield_canvas)=VMS_EXEC(g_repaint_owner_yields)=0;VMS_EXEC(g_repaint_yield_handoff)=false;
    VMS_EXEC(g_repaint_yield_method)=nullptr;VMS_EXEC(g_repaint_yield_pc)=-1;
    VMS_EXEC(g_speed_budget_owner)=nullptr;VMS_EXEC(g_speed_budget_window_frames)=VMS_EXEC(g_speed_group_timeouts)=0;
    VMS_EXEC(g_speed_last_paint_us) = VMS_EXEC(g_speed_next_us) = 0;
    VMS_EXEC(g_game_clock)={}; VMS_EXEC(g_game_clock_mode)=-1;VMS_EXEC(g_game_clock_inherited)=false;
    VMS_EXEC(g_poll_calls)=VMS_EXEC(g_poll_gate_reads)=VMS_EXEC(g_poll_waits)=VMS_EXEC(g_poll_wait_us)=0;
    VMS_EXEC(g_completed_paint241).reset();
    paint_hotspots241::reset();
    VMS_RS(g_repaint) = false; VMS_EXEC(g_run_depth) = 0; VMS_RS(g_paint_depth) = 0; VMS_RS(g_offscreen).clear(); VMS_EXEC(g_yield) = VMS_EXEC(g_thrown) = false; VMS_EXEC(g_cuts) = 0;
    reset_runtime_stats();
}
static void reset_runtime_stats() {
    diag_causal_reset();
    g_device_diag.detector.reset(); // Launch/pause/restore must not inherit an old shortfall streak.
    g_stat_present_seen=VMS_RS(g_present_frames);   // 0.2.113 F4: STAT canvas_fps window starts here
    g_stat_jit_ops_seen=VMS_NATIVE(g_native_jit).stats.ops;
    g_frame_sleep_stats={};
    VMS_EXEC(g_fast_ops)=VMS_EXEC(g_fast_blocks)=VMS_EXEC(g_slow_ops)=VMS_EXEC(g_fast_float241_ops)=0;
    VMS_NATIVE(g_library_indexes).reset_stats();VMS_NATIVE(g_library_leaf_calls)=0;
    VMS_RS(g_decoded_images).reset_counters();VMS_RS(g_png_fast)=VMS_RS(g_png_fallback)=VMS_RS(g_image_decode_us)=VMS_RS(g_image_copy_us)=0;
    VMS_EXEC(g_execution_profile).reset();
    for(auto& c:VMS_CL(g_classes))for(auto& m:c.methods)m.cpu_profile={};
    VMS_RS(g_frame_metrics).reset();
    VMS_HEAP(g_frame_buffer_pool).reset_counters();
    VMS_PLAT(g_cpu_window_start_ns)=j2me_cpu_clock::now_ns();
    VMS_RS(g_paint_stats).reset(paint_clock_us(),VMS_RS(g_game_frames));
    VMS_PLAT(g_present_stats)={};
    VMS_EXEC(g_runtime_profile).reset();
#ifdef __SWITCH__
    VMS_RS(g_graphics_profile).reset(true);
#else
    VMS_RS(g_graphics_profile).reset(VMS_RS(g_graphics_profile).enabled);
#endif
    VMS_EXEC(g_wake_stats)={};VMS_EXEC(g_wake_stats).ignore_before_ms=monotonic_ms();VMS_EXEC(g_pace_aligns)=0;
    VMS_EXEC(g_frame_wait_budget)={};VMS_EXEC(g_frame_work_us)=VMS_EXEC(g_frame_work_finished_us)=0;VMS_EXEC(g_last_slice_us)=0;
    VMS_EXEC(g_poll_calls)=VMS_EXEC(g_poll_gate_reads)=VMS_EXEC(g_poll_waits)=VMS_EXEC(g_poll_wait_us)=0;
    VMS_EXEC(g_speed_slept_us)=VMS_EXEC(g_speed_cut_us)=VMS_EXEC(g_speed_add_us)=0;
    VMS_EXEC(g_speed_budget_window_frames)=VMS_EXEC(g_speed_group_timeouts)=0;
}
// Reporting is only called outside interpreter/native scopes. CPU and wall
// intervals are deliberately separate; content changes are not logic updates.
static void report_optimization_window(int64_t elapsed_us,const RuntimeProfileWindow& w) {
    const int64_t now=j2me_cpu_clock::now_ns();
    const int64_t host_cpu=j2me_cpu_clock::difference(VMS_PLAT(g_cpu_window_start_ns),now);
    VMS_PLAT(g_cpu_window_start_ns)=now;
    char line[1536];
    std::snprintf(line,sizeof(line),"CPUSTAT schema=1 available=%d source=%s window_ms=%.3f host_thread_cpu_ms=%.3f worker_inclusive_cpu_ms=%.3f paint_inclusive_cpu_ms=%.3f worker_paint_overlap_cpu_ms=%.3f gc_cpu_ms=%.3f worker_gc_overlap_cpu_ms=%.3f worker_paint_gc_overlap_cpu_ms=%.3f cpu_scopes=%llu unavailable_scopes=%llu method_profile=%d scope=calling_OS_thread not_device_utilization=1",
        host_cpu>=0?1:0,j2me_cpu_clock::source(),elapsed_us/1000.0,host_cpu>=0?host_cpu/1000000.0:-1.0,
        w.worker_cpu_ns/1000000.0,w.paint_cpu_ns/1000000.0,w.worker_paint_cpu_ns/1000000.0,
        w.gc_cpu_ns/1000000.0,w.worker_gc_cpu_ns/1000000.0,w.worker_paint_gc_cpu_ns/1000000.0,
        (unsigned long long)w.cpu_samples,(unsigned long long)w.cpu_unavailable,VMS_EXEC(g_execution_profile).enabled?1:0);
    log_line(line);
    const auto frames=VMS_RS(g_frame_metrics).take();
    std::snprintf(line,sizeof(line),"FRAMESTAT schema=1 window_ms=%.3f completed_draws=%llu content_changes=%llu unchanged_draws=%llu content_unavailable=%llu interval_count=%llu interval_p50_upper_ms=%.3f interval_p95_upper_ms=%.3f interval_p99_upper_ms=%.3f interval_max_ms=%.3f gc_pause_count=%llu gc_pause_p99_upper_ms=%.3f gc_pause_max_ms=%.3f scope=raw_RGB_draw_completions not_presented_frames=1 logic_fps=unavailable percentile_bucket_us=250",
        elapsed_us/1000.0,(unsigned long long)frames.completed_draws,(unsigned long long)frames.content_frames,
        (unsigned long long)frames.unchanged_draws,(unsigned long long)frames.content_unavailable,
        (unsigned long long)frames.draw_intervals.count,frames.draw_intervals.quantile_upper(50)/1000.0,
        frames.draw_intervals.quantile_upper(95)/1000.0,frames.draw_intervals.quantile_upper(99)/1000.0,
        frames.draw_intervals.max_us/1000.0,(unsigned long long)frames.gc_pauses.count,
        frames.gc_pauses.quantile_upper(99)/1000.0,frames.gc_pauses.max_us/1000.0);
    log_line(line);
    std::snprintf(line,sizeof(line),"MEMSTAT schema=1 frame_buffer_reuses=%llu frame_buffer_reserves=%llu pool_retained_bytes=%zu pool_peak_bytes=%zu pool_budget_bytes=%zu pool_discards=%llu gc_mark_capacity_bytes=%zu gc_work_capacity_bytes=%zu frame_snapshot_capacity_bytes=%zu array_payload_bytes=%llu heap_slots=%zu scope=runtime_caches_and_array_payload not_total_RAM=1",
        (unsigned long long)VMS_HEAP(g_frame_buffer_pool).hits,(unsigned long long)VMS_HEAP(g_frame_buffer_pool).misses,
        VMS_HEAP(g_frame_buffer_pool).retained_bytes(),VMS_HEAP(g_frame_buffer_pool).peak_bytes,VMS_HEAP(g_frame_buffer_pool).byte_budget(),
        (unsigned long long)VMS_HEAP(g_frame_buffer_pool).discarded,VMS_HEAP(g_gc_mark_scratch).capacity(),VMS_HEAP(g_gc_work_scratch).capacity()*sizeof(int),
        VMS_RS(g_frame_metrics).snapshot_bytes(),(unsigned long long)VMS_HEAP(g_array_payload_live),VMS_HEAP(g_heap).size());
    log_line(line);VMS_HEAP(g_frame_buffer_pool).reset_counters();
    {const auto& p=VMS_HEAP(g_primitive_buffers);log_line("ARRAYPOOL schema=1 hits="+std::to_string(p.hits)+" misses="+std::to_string(p.misses)+
        " returns="+std::to_string(p.returns)+" discarded="+std::to_string(p.discarded)+" retained_bytes="+std::to_string(p.retained_bytes())+
        " peak_bytes="+std::to_string(p.peak_bytes)+" budget_bytes="+std::to_string(p.byte_budget())+" max_buffer_bytes="+std::to_string(p.MaxBuffer)+" scope=since_launch_or_restore primitive_dead_payload_only=1");}
    std::snprintf(line,sizeof(line),"VMSTAT schema=2 fast_ops=%llu slow_ops=%llu fast_blocks=%llu max_batch_ops=1024 safepoints=preserved",
        (unsigned long long)VMS_EXEC(g_fast_ops),(unsigned long long)VMS_EXEC(g_slow_ops),(unsigned long long)VMS_EXEC(g_fast_blocks));
    log_line(line);
    log_line("VMFLOAT schema=1 enabled="+std::to_string(VMS_EXEC(g_fast_float241_enabled))+" ops="+std::to_string(VMS_EXEC(g_fast_float241_ops))+" scope=window max_batch_ops=1024 safepoints=preserved");
    VMS_EXEC(g_fast_ops)=VMS_EXEC(g_fast_blocks)=VMS_EXEC(g_slow_ops)=VMS_EXEC(g_fast_float241_ops)=0;
    std::snprintf(line,sizeof(line),"IMAGECACHE schema=1 hits=%llu misses=%llu inserts=%llu evictions=%llu bypasses=%llu entries=%zu retained_bytes=%zu budget_bytes=%zu png_fast=%llu png_fallback=%llu decode_ms=%.3f copy_ms=%.3f scope=immutable_JAR_resources retained_excludes_inflight_and_Java_images=1",
        (unsigned long long)VMS_RS(g_decoded_images).hits,(unsigned long long)VMS_RS(g_decoded_images).misses,
        (unsigned long long)VMS_RS(g_decoded_images).inserts,(unsigned long long)VMS_RS(g_decoded_images).evictions,(unsigned long long)VMS_RS(g_decoded_images).bypasses,
        VMS_RS(g_decoded_images).entries(),VMS_RS(g_decoded_images).retained_bytes(),VMS_RS(g_decoded_images).byte_budget(),
        (unsigned long long)VMS_RS(g_png_fast),(unsigned long long)VMS_RS(g_png_fallback),VMS_RS(g_image_decode_us)/1000.0,VMS_RS(g_image_copy_us)/1000.0);
    log_line(line);VMS_RS(g_decoded_images).reset_counters();VMS_RS(g_png_fast)=VMS_RS(g_png_fallback)=VMS_RS(g_image_decode_us)=VMS_RS(g_image_copy_us)=0;
    const auto li=VMS_NATIVE(g_library_indexes).stats();
    std::snprintf(line,sizeof(line),"LIBSTAT schema=1 indexed_lookups=%llu hits=%llu rebuilds=%llu appended=%llu fallbacks=%llu probes=%llu retained_slot_bytes=%zu byte_budget=%zu leaf_calls=%llu value_bytes=%zu weak_indexes=1",
        (unsigned long long)li.lookups,(unsigned long long)li.hits,(unsigned long long)li.rebuilds,
        (unsigned long long)li.appended,(unsigned long long)li.fallbacks,(unsigned long long)li.probes,
        VMS_NATIVE(g_library_indexes).retained_bytes(),VMS_NATIVE(g_library_indexes).byte_budget(),(unsigned long long)VMS_NATIVE(g_library_leaf_calls),sizeof(Value));
    log_line(line);VMS_NATIVE(g_library_indexes).reset_stats();VMS_NATIVE(g_library_leaf_calls)=0;
    log_line("M3GSTAGE schema=1 texture_native="+std::to_string(VMS_RS(g_m3g_texture238_enabled))+
        " calls="+std::to_string(VMS_RS(g_m3g_texture238_calls))+" units="+std::to_string(VMS_RS(g_m3g_texture238_units))+
        " mip_units="+std::to_string(VMS_RS(g_m3g_texture238_mips))+" declines="+std::to_string(VMS_RS(g_m3g_texture238_declined))+
        " triangle_fallback_mip="+std::to_string(VMS_RS(g_m3g_fallback_mip))+" triangle_fallback_fog="+std::to_string(VMS_RS(g_m3g_fallback_fog))+
        " triangle_fallback_dither="+std::to_string(VMS_RS(g_m3g_fallback_dither))+" scope=process_cumulative texture_only=1");
    log_line("PAINTHANDOFF schema=1 enabled="+std::to_string(VMS_EXEC(g_completed_paint241).enabled)+
        " turns="+std::to_string(VMS_EXEC(g_completed_paint241).turns)+" handoffs="+std::to_string(VMS_EXEC(g_completed_paint241).handoffs)+
        " serial_deferrals="+std::to_string(VMS_EXEC(g_completed_paint241).serial_deferrals)+" worker_priorities="+std::to_string(VMS_EXEC(g_completed_paint241).worker_priorities)+
        " budget_us=8000 scope=suite_cumulative boundary=completed_callbacks not_paint_preemption=1");
    paint_hotspots241::report();
    m3g_image241::report();m3g_sprite241::report();m3g_mesh242::report();m3g_lighting242::report();m3g_assembly242::report();
    log_line("M3GCOVERAGE schema=1 triangles="+std::to_string(VMS_RS(g_m3g_native_triangles))+" triangle_pixels="+std::to_string(VMS_RS(g_m3g_native_pixels))+
        " vectors="+std::to_string(VMS_RS(g_m3g_native_vectors))+" vertices="+std::to_string(VMS_RS(g_m3g_native_vertices))+" matrices="+std::to_string(VMS_RS(g_m3g_native_matrices))+" scope=process_cumulative");
    const auto& sp=m3g_span239::stats;
    log_line("M3GSPAN schema=1 enabled="+std::to_string(VMS_RS(g_m3g_span239_enabled))+
        " attempts="+std::to_string(sp.attempts)+" calls="+std::to_string(sp.calls)+" pixels="+std::to_string(sp.pixels)+
        " texture_samples="+std::to_string(sp.texture_samples)+" mip_samples="+std::to_string(sp.mip_samples)+
        " depth_rejects="+std::to_string(sp.depth_rejects)+" alpha_rejects="+std::to_string(sp.alpha_rejects)+
        " color_writes="+std::to_string(sp.color_writes)+" depth_writes="+std::to_string(sp.depth_writes)+
        " decline_shape="+std::to_string(sp.declines[0])+" decline_callback="+std::to_string(sp.declines[1])+
        " decline_alias="+std::to_string(sp.declines[2])+" decline_coordinate="+std::to_string(sp.declines[3])+
        " timed_calls="+std::to_string(sp.timed_calls)+" sample_prepare_us="+std::to_string(sp.prepare_us)+
        " sample_raster_us="+std::to_string(sp.raster_us)+" sample_commit_us="+std::to_string(sp.commit_us)+
        " max_sample_us="+std::to_string(sp.max_sample_us)+" sample_period=64 max_pixels=8 scope=process_cumulative");
    const auto& rw=m3g_row240::stats;
    log_line("M3GROW schema=1 enabled="+std::to_string(VMS_RS(g_m3g_row240_enabled))+
        " attempts="+std::to_string(rw.attempts)+" calls="+std::to_string(rw.calls)+" pixels="+std::to_string(rw.pixels)+
        " subspans="+std::to_string(rw.subspans)+" max_pixels="+std::to_string(rw.max_pixels)+
        " texture_samples="+std::to_string(rw.texture_samples)+" mip_samples="+std::to_string(rw.mip_samples)+
        " depth_rejects="+std::to_string(rw.depth_rejects)+" alpha_rejects="+std::to_string(rw.alpha_rejects)+
        " color_writes="+std::to_string(rw.color_writes)+" depth_writes="+std::to_string(rw.depth_writes)+
        " decline_shape="+std::to_string(rw.declines[0])+" decline_callback="+std::to_string(rw.declines[1])+
        " decline_alias="+std::to_string(rw.declines[2])+" decline_coordinate="+std::to_string(rw.declines[3])+
        " timed_calls="+std::to_string(rw.timed_calls)+" sample_prepare_us="+std::to_string(rw.prepare_us)+
        " sample_raster_us="+std::to_string(rw.raster_us)+" sample_commit_us="+std::to_string(rw.commit_us)+
        " max_sample_us="+std::to_string(rw.max_sample_us)+" sample_period=64 bounded_pixels=1024 scope=process_cumulative");
    const auto& js=VMS_NATIVE(g_native_jit).stats;
    log_line("JITSTAT schema=1 enabled="+std::to_string(VMS_NATIVE(g_native_jit).enabled)+" available="+std::to_string(VMS_NATIVE(g_native_jit).available)+
        " arch="+jit236::architecture()+" self_test="+std::to_string(VMS_NATIVE(g_native_jit).self_test)+
        " compiled="+std::to_string(js.compiled)+" rejected="+std::to_string(js.rejected)+
        " calls="+std::to_string(js.executed)+" bytecodes="+std::to_string(js.ops)+
        " budget_fallbacks="+std::to_string(js.budget_fallbacks)+" permission_failures="+std::to_string(js.permission_failures)+
        " code_bytes="+std::to_string(VMS_NATIVE(g_native_jit).bytes())+" budget_bytes="+std::to_string(jit236::Engine::BudgetBytes)+
        " allocation_paused="+std::to_string(VMS_NATIVE(g_native_jit).admission_closed)+" resource_reopens="+std::to_string(VMS_NATIVE(g_native_jit).stats.resource_reopens)+" allocation_generation="+std::to_string(VMS_NATIVE(g_native_jit).admission_generation())+" allocation_error="+jit236::Executable::last_error()+" allocation_result="+std::to_string(jit236::Executable::last_result())+" scope=suite_lifetime subset=bounded_static_int_leaf safepoints=preserved");
    log_line("JITREGION schema=1 overlap_deferred="+std::to_string(g_jit_region_stats.overlap_deferred)+" overlap_hits="+std::to_string(g_jit_region_stats.overlap_hits)+" overlap_reopened="+std::to_string(g_jit_region_stats.overlap_reopened)+" overlap_pressure_bytes="+std::to_string(JitOverlapPressureBytes)+" retired_short="+std::to_string(g_jit_region_stats.retired_short)+" retired_invoke="+std::to_string(g_jit_region_stats.retired_invoke)+" skip_marked="+std::to_string(g_jit_region_stats.skip_marked)+" full_skipped="+std::to_string(g_jit_region_stats.full_skipped)+" compiled="+std::to_string(g_jit_region_stats.compiled)+" rejected="+std::to_string(g_jit_region_stats.rejected)+" banned_wait="+std::to_string(g_jit_region_stats.banned_wait)+" entries="+std::to_string(g_jit_region_stats.entries)+" zero_progress="+std::to_string(g_jit_region_stats.zero)+" ops="+std::to_string(g_jit_region_stats.ops)+" code_bytes="+std::to_string(g_jit_region_stats.bytes)+" budget_bytes="+std::to_string(JitRegionMaxBytes)+" resident_regions="+std::to_string(g_jit_region_stats.resident)+" max_regions="+std::to_string(JitRegionMax)+" ic_sites="+std::to_string(g_jit_region_t3.ic_sites)+" cast_sites="+std::to_string(g_jit_region_t3.cast_sites)+" stack_entries="+std::to_string(g_jit_region_t3.stack_entries)+" recompiles="+std::to_string(g_jit_region_t3.recompiles)+" gfx_ok="+std::to_string(g_jitr_gfx_ok)+" gfx_bail_cls="+std::to_string(g_jitr_gfx_bail_cls)+" gfx_bail_slots="+std::to_string(g_jitr_gfx_bail_slots)+" bail=interpreter_same_pc"+
        " stale_id="+std::to_string(g_jit_region_stats.stale_id)+" stale_exit="+std::to_string(g_jit_region_stats.stale_exit)+" stale_recompiles="+std::to_string(g_jit_region_stats.stale_recompiles)+
        " leaf_stale="+std::to_string(js.stale)+" leaf_stale_reinstalls="+std::to_string(js.stale_reinstalls)+" jitstale_lines="+std::to_string(g_jitstale_lines)+
        " jitstale_shown="+std::to_string(g_jitstale_limit.shown)+" jitstale_suppressed="+std::to_string(g_jitstale_limit.suppressed)+" jitstale_summaries="+std::to_string(g_jitstale_limit.summaries)+" stale_fatal="+std::to_string(g_jit_region_stats.stale_fatal));
    log_line("JITLOOP schema=1 backedge_handoffs="+std::to_string(g_jit_region_stats.backedge_handoffs)+" scope=suite_lifetime same_pc_entry=1 safepoints=preserved");
    {const auto& pool=jit236::code_pool_stats();log_line("JITPOOL block_bytes="+std::to_string(jit236::CodeBlockBytes)+" dedicated_block_bytes="+std::to_string(jit236::CodeDedicatedBytes)+" max_slice_bytes="+std::to_string(jit236::CodeMaxSliceBytes)+" blocks="+std::to_string(pool.blocks)+" peak_blocks="+std::to_string(pool.peak_blocks)+" slices="+std::to_string(pool.slices)+" committed_bytes="+std::to_string(pool.reserved_bytes)+" peak_committed_bytes="+std::to_string(pool.peak_reserved_bytes)+" used_bytes="+std::to_string(pool.used_bytes)+" cap_bytes="+std::to_string(jit236::CodePoolMaxBytes)+
        " quarantined_pages="+std::to_string(pool.quarantined_pages)+" quarantine_releases="+std::to_string(pool.quarantine_releases)+" fresh_installs="+std::to_string(pool.fresh_installs)+
        " reused_installs="+std::to_string(pool.reused_installs)+" dedicated_blocks="+std::to_string(pool.dedicated_blocks)+" jit_type="+jit236::jit_type_name());}
    if(VMS_EXEC(g_execution_profile).enabled) {
        std::vector<Method*> hot;
        for(auto& c:VMS_CL(g_classes))for(auto& m:c.methods)if(m.cpu_profile.calls||m.cpu_profile.self_cpu_ns)hot.push_back(&m);
        std::sort(hot.begin(),hot.end(),[](Method* a,Method* b){return a->cpu_profile.self_cpu_ns>b->cpu_profile.self_cpu_ns;});
        for(size_t i=0;i<std::min<size_t>(12,hot.size());++i) {
            const auto* m=hot[i];
            std::snprintf(line,sizeof(line),"HOTSPOT rank=%zu class=%s method=%s descriptor=%s self_cpu_ms=%.6f calls=%llu scope=exclusive_method_CPU diagnostic_overhead_present=1",
                i+1,VMS_CL(g_classes)[m->class_id].name.c_str(),m->name.c_str(),m->desc.c_str(),m->cpu_profile.self_cpu_ns/1000000.0,
                (unsigned long long)m->cpu_profile.calls);log_line(line);
        }
        std::snprintf(line,sizeof(line),"HOTSPOT_SUMMARY methods=%zu transitions=%llu unavailable=%llu",hot.size(),
            (unsigned long long)VMS_EXEC(g_execution_profile).transitions,(unsigned long long)VMS_EXEC(g_execution_profile).clock_failures);
        log_line(line);
        for(auto* m:hot)m->cpu_profile={};
        VMS_EXEC(g_execution_profile).transitions=VMS_EXEC(g_execution_profile).clock_failures=0;
    }
}
struct P1M3GEpoch { uint64_t identity=0,skin=0,quantize=0,light=0,gathered=0,emitted=0,texture=0,span=0,row=0; } ; VMS_DIAG_ACCESSOR(g_p1_m3g_base, P1M3GEpoch)
static P1M3GEpoch p1_m3g_values(){return {m3g_mesh242::stats.identities,m3g_mesh242::stats.skin_calls,m3g_mesh242::stats.quantize_calls,m3g_lighting242::calls,m3g_assembly242::gathered,m3g_assembly242::emitted,VMS_RS(g_m3g_texture238_calls),m3g_span239::stats.calls,m3g_row240::stats.calls};}
static void p1_begin_m3g_epoch(){VMS_DIAG(g_p1_m3g_base)=p1_m3g_values();}
static void p1_report_m3g_epoch(){auto v=p1_m3g_values();const auto& b=VMS_DIAG(g_p1_m3g_base);
    log_line("P1M3G scope=launch_delta launch="+std::to_string(VMS_DIAG(g_p1).launch_id)+" identity="+std::to_string(v.identity-b.identity)+" skin="+std::to_string(v.skin-b.skin)+" quantize="+std::to_string(v.quantize-b.quantize)+" light="+std::to_string(v.light-b.light)+" gathered="+std::to_string(v.gathered-b.gathered)+" emitted="+std::to_string(v.emitted-b.emitted)+" texture="+std::to_string(v.texture-b.texture)+" span="+std::to_string(v.span-b.span)+" row="+std::to_string(v.row-b.row));
}
// P8.10: jit/p7-worker/p7-audio/p7-gc/p7-present default ON; X.disable (Switch) or env=0 (desktop) turns one off.
// Old X.enable files are ignored (never an error). Source is recorded for the P810DEFAULTS line.
static bool p810_switch(const char* name,const char* env,std::string& src){
#ifdef __SWITCH__
    (void)env; struct stat st{}; const std::string p=std::string("sdmc:/switch/j2me/")+name+".disable";
    if(stat(p.c_str(),&st)==0){src=std::string(name)+".disable";return false;}
#else
    (void)name; if(const char* v=std::getenv(env)){ if(!std::strcmp(v,"0")){src=std::string("env:")+env+"=0";return false;}
        if(!std::strcmp(v,"1")){src=std::string("env:")+env+"=1";return true;} }
#endif
    src="default"; return true;
}
static void p1_configure(){
    city_diag_configure();
#ifdef __SWITCH__
    const std::string path="sdmc:/switch/j2me/p1-profile.cfg";
#else
    const char* env=std::getenv("J2ME_P1_PROFILE");const std::string path=env?env:"";
#endif
    VMS_DIAG(g_p1_profile)=path.empty()?p1::Profile{}:p1::read_profile(path);
    log_line(std::string("BOOT build_id=")+kVer+" baseline=0.2.42-s1 base_sha256=baeddb153ab72e23301ed33fa25cd0a2574515e840e6c6fd78ebb5dd5f532237 session="+std::to_string(VMS_DIAG(g_p1).session_id));
    if(!VMS_DIAG(g_p1_profile).error.empty())log_line("P1CONFIG rejected source="+path+" error="+VMS_DIAG(g_p1_profile).error+" effective=legacy");
    if(!VMS_DIAG(g_p1_profile).active){log_line("P1CONFIG active=0 source=legacy defaults_preserved=1");return;}
    log_line("P1CONFIG active=1 profile="+VMS_DIAG(g_p1_profile).id+" source="+path+" explicit_mask="+std::to_string(VMS_DIAG(g_p1_profile).override_mask));
#ifdef __SWITCH__
    for(const char* name:{"m3g_reference.enable","m3g_span_reference.enable","m3g_row_reference.enable","m3g_stage241_reference.enable","paint_handoff_reference.enable","float_reference.enable","m3g_sprite_reference.enable","m3g_prep_reference.enable","m3g_stage242_reference.enable","jit.enable","profile.enable"}){
        struct stat st{};std::string legacy="sdmc:/switch/j2me/"+std::string(name);if(stat(legacy.c_str(),&st)==0)log_line("P1CONFIG ignored_source="+legacy+" reason=explicit_profile");
    }
#else
    for(const char* name:{"J2ME_M3G_REFERENCE","J2ME_M3G_SPAN_REFERENCE","J2ME_M3G_ROW_REFERENCE","J2ME_M3G_STAGE241_REFERENCE","J2ME_PAINT_HANDOFF_REFERENCE","J2ME_FLOAT_REFERENCE","J2ME_M3G_SPRITE_REFERENCE","J2ME_M3G_PREP_REFERENCE","J2ME_M3G_STAGE242_REFERENCE","J2ME_JIT","J2ME_PROFILE"})if(std::getenv(name))log_line("P1CONFIG ignored_source="+std::string(name)+" reason=explicit_profile");
#endif
    const auto& p=VMS_DIAG(g_p1_profile);
    auto field=[&](const char* name,bool value,unsigned bit){log_line("P1CONFIG field="+std::string(name)+" requested="+std::to_string(value)+" effective="+std::to_string(value)+" source="+((p.override_mask&bit)?"explicit_config":"builtin_profile"));};
    field("geometry_native",p.geometry_native,p1::Geometry);field("host_boundary",p.host_boundary,p1::Boundary);field("completed_handoff",p.completed_handoff,p1::Handoff);
    field("sprite_native",p.sprite_native,p1::Sprite);field("prep_native",p.prep_native,p1::Prep);field("float_batch",p.float_batch,p1::Float);field("diagnostics",p.diagnostics,p1::Diagnostics);
    log_line("P1CONFIG fixed texture_native=1 span_native=1 row_native=1 jit_requested=0 renderer=cpu speed=native source=profile_fixed");
    VMS_RS(g_m3g_mesh242_enabled)=p.geometry_native;VMS_RS(g_host_boundary242_enabled)=p.host_boundary;
    VMS_EXEC(g_completed_paint241).enabled=p.completed_handoff;VMS_RS(g_m3g_sprite241_enabled)=p.sprite_native;
    VMS_RS(g_m3g_prep241_enabled)=p.prep_native;VMS_EXEC(g_fast_float241_enabled)=p.float_batch;
    VMS_RS(g_m3g_texture238_enabled)=J2ME_OPT_M3G_TEXTURE238;VMS_RS(g_m3g_span239_enabled)=J2ME_OPT_M3G_SPAN239;VMS_RS(g_m3g_row240_enabled)=J2ME_OPT_M3G_ROW240;
    VMS_DIAG(g_p1).enabled=p.diagnostics;VMS_EXEC(g_speed_cap)=0;
    log_line("P1CONFIG effective geometry_native="+std::to_string(p.geometry_native)+" host_boundary="+std::to_string(p.host_boundary)+" completed_handoff="+std::to_string(p.completed_handoff)+" sprite_native="+std::to_string(p.sprite_native)+" prep_native="+std::to_string(p.prep_native)+" float_batch="+std::to_string(p.float_batch)+" diagnostics="+std::to_string(p.diagnostics)+" texture_native="+std::to_string(VMS_RS(g_m3g_texture238_enabled))+" span_native="+std::to_string(VMS_RS(g_m3g_span239_enabled))+" row_native="+std::to_string(VMS_RS(g_m3g_row240_enabled))+" speed=native renderer=cpu jit_requested=0");
}
static void configure_runtime_diagnostics() {
    log_line("OFFLINE_POLICY schema=2 sms=always_sent payment=always_success log=SMSSKIP dialogs=0 incoming_sms=none network=unavailable bluetooth=unavailable");
    s0_configure_hook();cont_configure();
#ifdef __SWITCH__
    struct stat m3g_reference{};VMS_RS(g_m3g_texture238_enabled)=J2ME_OPT_M3G_TEXTURE238 && stat("sdmc:/switch/j2me/m3g_reference.enable",&m3g_reference)!=0;
    struct stat span_reference{};VMS_RS(g_m3g_span239_enabled)=J2ME_OPT_M3G_SPAN239 &&
        stat("sdmc:/switch/j2me/m3g_reference.enable",&span_reference)!=0 &&
        stat("sdmc:/switch/j2me/m3g_span_reference.enable",&span_reference)!=0;
#else
    const char* reference=std::getenv("J2ME_M3G_REFERENCE");VMS_RS(g_m3g_texture238_enabled)=J2ME_OPT_M3G_TEXTURE238 && !(reference&&std::strcmp(reference,"1")==0);
#endif
#ifndef __SWITCH__
    const char* span_reference=std::getenv("J2ME_M3G_SPAN_REFERENCE");
    VMS_RS(g_m3g_span239_enabled)=J2ME_OPT_M3G_SPAN239 && !(reference&&std::strcmp(reference,"1")==0) &&
        !(span_reference&&std::strcmp(span_reference,"1")==0);
#endif
#ifdef __SWITCH__
    struct stat row_reference{};VMS_RS(g_m3g_row240_enabled)=J2ME_OPT_M3G_ROW240 && VMS_RS(g_m3g_span239_enabled) &&
        stat("sdmc:/switch/j2me/m3g_row_reference.enable",&row_reference)!=0;
#else
    const char* row_reference=std::getenv("J2ME_M3G_ROW_REFERENCE");
    VMS_RS(g_m3g_row240_enabled)=J2ME_OPT_M3G_ROW240 && VMS_RS(g_m3g_span239_enabled) &&
        !(row_reference&&std::strcmp(row_reference,"1")==0);
#endif
#ifdef __SWITCH__
    struct stat stage241{};
    bool reference241=stat("sdmc:/switch/j2me/m3g_stage241_reference.enable",&stage241)==0;
    VMS_EXEC(g_completed_paint241).enabled=!reference241 && stat("sdmc:/switch/j2me/paint_handoff_reference.enable",&stage241)!=0;
    VMS_EXEC(g_fast_float241_enabled)=!reference241 && stat("sdmc:/switch/j2me/float_reference.enable",&stage241)!=0;
    VMS_RS(g_m3g_sprite241_enabled)=!reference241 && stat("sdmc:/switch/j2me/m3g_sprite_reference.enable",&stage241)!=0;
    VMS_RS(g_m3g_prep241_enabled)=!reference241 && stat("sdmc:/switch/j2me/m3g_prep_reference.enable",&stage241)!=0;
#else
    auto flag241=[](const char* s){const char* v=std::getenv(s);return v&&std::strcmp(v,"1")==0;};
    VMS_EXEC(g_completed_paint241).enabled=!flag241("J2ME_M3G_STAGE241_REFERENCE")&&!flag241("J2ME_PAINT_HANDOFF_REFERENCE");
    VMS_EXEC(g_fast_float241_enabled)=!flag241("J2ME_M3G_STAGE241_REFERENCE")&&!flag241("J2ME_FLOAT_REFERENCE");
    VMS_RS(g_m3g_sprite241_enabled)=!flag241("J2ME_M3G_STAGE241_REFERENCE")&&!flag241("J2ME_M3G_SPRITE_REFERENCE");
    VMS_RS(g_m3g_prep241_enabled)=!flag241("J2ME_M3G_STAGE241_REFERENCE")&&!flag241("J2ME_M3G_PREP_REFERENCE");
#endif
#ifdef __SWITCH__
    struct stat stage242{};
    VMS_RS(g_m3g_mesh242_enabled)=stat("sdmc:/switch/j2me/m3g_stage242_reference.enable",&stage242)!=0;
#else
    VMS_RS(g_m3g_mesh242_enabled)=!flag241("J2ME_M3G_STAGE242_REFERENCE");
#endif
    VMS_RS(g_host_boundary242_enabled)=VMS_RS(g_m3g_mesh242_enabled);
    p1_configure();
    log_line("M3G_STAGE242 geometry_native="+std::to_string(VMS_RS(g_m3g_mesh242_enabled))+" host_boundary="+std::to_string(VMS_RS(g_host_boundary242_enabled))+" max_skin_vertices=128 max_bones=256");
    log_line("M3G_STAGE241 sprite_native="+std::to_string(VMS_RS(g_m3g_sprite241_enabled))+" prep_native="+std::to_string(VMS_RS(g_m3g_prep241_enabled))+" completed_handoff="+std::to_string(VMS_EXEC(g_completed_paint241).enabled)+" float_batch="+std::to_string(VMS_EXEC(g_fast_float241_enabled))+" max_sprite_row=1024 max_prep_block=1024");
    log_line(std::string("M3G_STAGE version=")+kVer+" stage=0.2.81-p8-r3b texture_native="+std::to_string(VMS_RS(g_m3g_texture238_enabled))+
        " span_native="+std::to_string(VMS_RS(g_m3g_span239_enabled))+" row_native="+std::to_string(VMS_RS(g_m3g_row240_enabled))+
        " max_span=8 max_row=1024 render_model=reference_scanline source_baseline=0.2.41-s1");
#ifdef __SWITCH__
    struct stat st{};VMS_EXEC(g_execution_profile).enabled=stat("sdmc:/switch/j2me/profile.enable",&st)==0;
#else
    const char* enabled=std::getenv("J2ME_PROFILE");
    VMS_EXEC(g_execution_profile).enabled=enabled?std::strcmp(enabled,"1")==0:J2ME_PROFILE_DEFAULT!=0;
#endif
    std::string jit_src,w_src,a_src,g_src,pr_src; const bool jit_requested=p810_switch("jit","J2ME_JIT",jit_src);
    if(VMS_DIAG(g_p1_profile).active)VMS_EXEC(g_execution_profile).enabled=false;
#ifndef __SWITCH__
    // Desktop/qemu-only fault injection for the fallback path (never compiled into the Switch NRO).
    if(const char* f=std::getenv("J2ME_JIT_FAULT")){ if(!std::strcmp(f,"install"))jit236::Executable::fault()=1;
        else if(!std::strcmp(f,"result"))VMS_NATIVE(g_native_jit).result_fault=1; else if(!std::strcmp(f,"region"))g_jit_region_selftest_fault=1;
        else if(!std::strcmp(f,"leafguard"))VMS_NATIVE(g_native_jit).guard_fault=1; }   // 0.2.114: leaf guard self-test failure
    // 0.2.113 desktop/test-only stale-code fault (off unless J2ME_JIT_FAULT_STALE=1; not compiled into the Switch NRO)
    if(const char* f=std::getenv("J2ME_JIT_FAULT_STALE")) jit236::fault_stale()=(f[0]=='1');
#endif
    VMS_NATIVE(g_native_jit).configure(jit_requested&&!VMS_EXEC(g_execution_profile).enabled);
    {   // P6.5 startup self-test: jit236 leaf (already run in configure) + region backend; any failure -> interpreter only.
        auto& eng=VMS_NATIVE(g_native_jit); std::string region_why=eng.enabled?"pending":"skipped";
        const bool jit236_ok=eng.enabled&&eng.self_test;
        bool region_ok=false; if(jit236_ok){ region_ok=jit_region_self_test(region_why); if(!region_ok){eng.enabled=false;eng.available=false;eng.reason="region_"+region_why;} }
        const char* create=!jit_requested?"not_attempted":(jit236_ok||eng.reason.find("install_failed")==std::string::npos?"ok":"failed");
        log_line(std::string("JITSELFTEST schema=1 requested=")+std::to_string(jit_requested)+" jitCreate="+create+" create_error="+jit236::Executable::last_error()+
            " wx="+jit236::wx_mode()+" budget_leaf_bytes="+std::to_string(jit236::Engine::BudgetBytes)+" budget_region_bytes="+std::to_string(JitRegionMaxBytes)+
            " leaf_selftest="+(jit_requested?(jit236_ok?"pass":"fail"):"skipped")+" region_selftest="+(jit236_ok?(region_ok?"pass":"fail"):"skipped")+
            " result="+(eng.enabled?"jit_enabled":"interpreter_only")+" reason="+(jit_requested?eng.reason:"disabled_by_"+jit_src)+
            " jit_type="+jit236::jit_type_name()+" guard=install_id quarantine_pages="+std::to_string(jit236::CodeQuarantinePages)+" selftest_block=dedicated");
    }
    log_line("JIT mode=default_on requested="+std::to_string(jit_requested)+" enabled="+std::to_string(VMS_NATIVE(g_native_jit).enabled)+" source="+jit_src+
        " self_test="+std::to_string(VMS_NATIVE(g_native_jit).self_test)+" arch="+jit236::architecture()+" tier=T3_region_ic_switch_p68 loops=compiled ic=monomorphic_trivial_inline heap=read_and_checked_int_array_store wait_loops=never putstatic=interpreter");
    {   // P7.2 worker base: default off; no work is moved onto the workers in this version.
        const bool p7_requested=p810_switch("p7-worker","J2ME_P7_WORKER",w_src);
        log_line(p7::configure(p7_requested,resver::g_epoch));
#ifdef __SWITCH__
        struct stat p7c_file{};p7::g_count_on=stat("sdmc:/switch/j2me/p7-count.enable",&p7c_file)==0;
#else
        const char* p7c_env=std::getenv("J2ME_P7_COUNT");p7::g_count_on=p7c_env&&std::strcmp(p7c_env,"1")==0;
        if(const char* dump=std::getenv("J2ME_P7_PCM_DUMP")){ // desktop test-only capture of every rendered frame
            if(!p7audio::g_dump){ p7audio::g_dump=std::fopen(dump,"wb"); if(!p7audio::g_dump)log_line(std::string("P7AUDIO pcm_dump open_failed path=")+dump); }
        }
#endif
        log_line(std::string("P7COUNT_CONFIG enabled=")+(p7::g_count_on?"1":"0"));
        const bool p7a_requested=p810_switch("p7-audio","J2ME_P7_AUDIO",a_src);
        log_line(p7audio::configure(p7a_requested));
#ifdef __SWITCH__
        const bool p7g_verify=false;
#else
        const char* p7gv_env=std::getenv("J2ME_P7_GC_VERIFY");const bool p7g_verify=p7gv_env&&std::strcmp(p7gv_env,"1")==0;
#endif
        const bool p7g_requested=p810_switch("p7-gc","J2ME_P7_GC",g_src);
        log_line(p7gc::configure(p7g_requested,p7g_verify));
        const bool p7p_requested=p810_switch("p7-present","J2ME_P7_PRESENT",pr_src);
        log_line(p7present::configure(p7p_requested));
        {   auto on=[](bool b){return b?"on":"off";};
            log_line(std::string("P810DEFAULTS schema=1 jit=")+on(VMS_NATIVE(g_native_jit).enabled)+"(req="+on(jit_requested)+",src="+jit_src+")"+
                " p7-worker="+on(p7::pool().enabled())+"(req="+on(p7_requested)+",src="+w_src+")"+
                " p7-audio="+on(p7audio::g_enabled)+"(req="+on(p7a_requested)+",src="+a_src+")"+
                " p7-gc="+on(p7gc::g_enabled)+"(req="+on(p7g_requested)+",src="+g_src+")"+
                " p7-present="+on(p7present::g_enabled)+"(req="+on(p7p_requested)+",src="+pr_src+")"+
                " fallback="+(p7::pool().enabled()?"none":"single_thread")+" legacy_enable_files=ignored"); }
#ifdef __SWITCH__
        struct stat p8f_file{};p8::g_frame_on=stat("sdmc:/switch/j2me/p8-frame.enable",&p8f_file)==0;
#else
        const char* p8f_env=std::getenv("J2ME_P8_FRAME");p8::g_frame_on=p8f_env&&std::strcmp(p8f_env,"1")==0;
#endif
        p7::g_scope_on=p7::g_count_on||p8::g_frame_on; // P8FRAME reads the same exclusive-time scopes
        {
#ifdef __SWITCH__
          struct stat ns_file{}; p86w::g_spin=stat("sdmc:/switch/j2me/p86-spin.enable",&ns_file)==0;
#else
          const char* ns_env=std::getenv("J2ME_P86_SPIN"); p86w::g_spin=ns_env&&std::strcmp(ns_env,"1")==0;
#endif
          log_line(std::string("P86WAKE_CONFIG spin=")+(p86w::g_spin?"1":"0")+" early_us=2000"); }
        p86::g_on=p8::g_frame_on; log_line(std::string("P8FRAME_CONFIG enabled=")+(p8::g_frame_on?"1":"0"));
        p8_enable_report();
        static bool p8_atexit=false; if(!p8_atexit){p8_atexit=true;std::atexit(p8_exit_summary);} // P8.2: exit summary
    }
    log_line("OPT schema=3 frame_pool="+std::to_string(J2ME_OPT_FRAME_POOL)+" gc_scratch="+std::to_string(J2ME_OPT_GC_SCRATCH)+
        " native_class_cache="+std::to_string(J2ME_OPT_NATIVE_CLASS_CACHE)+" gfx_state="+std::to_string(J2ME_OPT_GFX_STATE)+
        " rgb_fill="+std::to_string(J2ME_OPT_RGB_FILL)+" audio_poll="+std::to_string(J2ME_OPT_AUDIO_POLL)+
        " first_repaint_handoff="+std::to_string(J2ME_OPT_ZXX_HANDOFF)+" poll_margin="+std::to_string(J2ME_OPT_POLL_MARGIN)+
        " ascii_scan="+std::to_string(J2ME_OPT_ASCII_SCAN)+" fast_dispatch="+std::to_string(J2ME_OPT_FAST_DISPATCH)+" fast_png="+std::to_string(J2ME_OPT_FAST_PNG)+
        " image_cache="+std::to_string(J2ME_OPT_IMAGE_CACHE)+" gate_recovery="+std::to_string(J2ME_OPT_GATE_RECOVERY)+
        " library_index="+std::to_string(J2ME_OPT_LIBRARY_INDEX)+" value_bytes="+std::to_string(sizeof(Value))+
        " method_profile="+std::to_string(VMS_EXEC(g_execution_profile).enabled?1:0)+" game_timing_policy=preserved");
}
#ifdef __SWITCH__
// P7.3: Switch pump on worker 0. Same pumpThrottled() call, same arguments; failures are carried back and logged here.
static void diag_audio_sample(const j2me_audio::SwitchAudioSink& sink){
    const auto& stats=sink.metrics();runtime_diag::AudioSnapshot s;
    s.request_us=uint64_t(paint_clock_us());s.service_us=sink.lastServiceUs();s.queued=sink.queuedBuffers();s.pumps=stats.pump_count;s.empty_polls=stats.queue_empty_polls;s.blocks=stats.render_blocks;s.nonzero_blocks=stats.nonzero_blocks;s.errors=stats.errors;s.max_gap_us=stats.max_pump_gap_us;s.max_mix_us=stats.max_mix_us;
    g_device_diag.audio.publish(s); // Caller owns the sink, including on its worker.
}
static void p7_audio_report_failure() {
    const int64_t rc=p7audio::g_failure_rc.exchange(0);
    if(rc) {const auto message="AUDIO output failure op="+std::string(p7audio::g_failure_op.load()?p7audio::g_failure_op.load():"?")+" result="+std::to_string(rc)+" thread=worker";log_line(message);g_device_diag.flight.note(paint_clock_us(),"audio_failure",message.c_str());diag_fault("AUDIO_OUTPUT_FAILURE",message.c_str(),false);}
}
static void p7_audio_submit_pump(bool frame) {
    j2me_audio::Engine& engine=VMS_PLAT(g_audio);              // fences the previous job
    j2me_audio::SwitchAudioSink& sink=VMS_PLAT(g_audio_sink);
    p7_audio_snapshot_if_needed(j2me_audio::SwitchAudioSink::BufferCount*j2me_audio::SwitchAudioSink::BufferFrames); // max one pump renders
    if(p7audio::q_idle){ ++p7audio::g_idle_pumps; Result rc=sink.pumpThrottled(engine,frame); // idle: same inline pump as switch-off, no handoff
        diag_audio_sample(sink);
        if(R_FAILED(rc)){ p7audio::g_failure_op.store(sink.errorOperation()); p7audio::g_failure_rc.store(int64_t(rc)); } return; }
    p7audio::submit([&engine,&sink,frame]{ Result rc=sink.pumpThrottled(engine,frame);
        diag_audio_sample(sink);
        if(R_FAILED(rc)){ p7audio::g_failure_op.store(sink.errorOperation()); p7audio::g_failure_rc.store(int64_t(rc)); } });
}
#endif
namespace j2me_vmstate { namespace acc { static inline j2me_audio::Engine& platform_g_audio_unfenced() noexcept; } }
#ifndef __SWITCH__
// Desktop render of `frames` frames in <=1024-frame chunks (the pre-P7.3 loop, unchanged). With p7-audio the same loop
// runs on worker 0; the frame count and chunking are decided here on the main thread either way.
static void p7_render_counts(const std::vector<uint32_t>& counts) { // same loop as below, per recorded pump
    j2me_audio::Engine& engine=::j2me_vmstate::acc::platform_g_audio_unfenced(); // caller owns the engine (fence or worker job)
    thread_local int16_t wpcm[2048];
    for(uint32_t frames:counts){ size_t left=frames;
        while(left){size_t n=std::min<size_t>(left,1024);engine.render(wpcm,n);p7audio::dump(wpcm,n);left-=n;} }
}
#endif
// P7.3 snapshot for exact pendingEnds()/paused() answers while renders are deferred or on the worker.
static void p7_audio_snapshot_if_needed(uint64_t frames) {
    if(!p7audio::q_valid){ // only reached right after a fence: no job in flight, nothing pending, main owns the engine
        const j2me_audio::Engine& e=::j2me_vmstate::acc::platform_g_audio_unfenced();
        p7audio::q_ends=e.pendingEnds(); p7audio::q_paused=e.paused(); p7audio::q_idle=e.idle(); p7audio::q_headroom=e.endHeadroom(); p7audio::q_frames=0; p7audio::q_valid=true;
    }
    p7audio::q_frames+=frames;
}
static bool p7_pending_ends() {
    if(p7audio::fast_ends_ok()){ ++p7audio::g_fast_hits; return p7audio::q_ends; }
    return VMS_PLAT(g_audio).pendingEnds();
}
static bool p7_poll_end(int& handle) {
    // Exact: snapshot proves no end is queued and none can occur before pending frames render.
    if(p7audio::fast_ends_ok() && !p7audio::q_ends){ ++p7audio::g_fast_hits; return false; }
    return VMS_PLAT(g_audio).pollEnd(handle);
}
static bool p7_audio_paused() {
    if(p7audio::fast_paused_ok()){ ++p7audio::g_fast_hits; return p7audio::q_paused; }
    return VMS_PLAT(g_audio).paused();
}
#ifndef __SWITCH__
static void audio_render_desktop(size_t frames) {
    if (p7audio::g_enabled) {
        if (!frames) return;
        p7_audio_snapshot_if_needed(frames);
        if (p7audio::q_idle) { // exact: idle since the last fence, nothing pending, no job in flight -> zero-fill inline
            ++p7audio::g_idle_pumps; p7audio::g_frames_inline+=frames; j2me_audio::Engine& e=::j2me_vmstate::acc::platform_g_audio_unfenced(); int16_t zp[2048];
            while(frames){size_t n=std::min<size_t>(frames,1024);e.render(zp,n);p7audio::dump(zp,n);frames-=n;} return;
        }
        p7audio::g_render_counts=&p7_render_counts;
        p7audio::g_pending.push_back(uint32_t(frames)); p7audio::g_pending_frames+=frames;
        if (p7audio::g_pending_frames>=p7audio::BatchFrames) {
            std::vector<uint32_t> counts; counts.swap(p7audio::g_pending);
            p7audio::g_frames_worker+=p7audio::g_pending_frames; p7audio::g_pending_frames=0; ++p7audio::g_batches;
            p7audio::submit([counts=std::move(counts)]{ p7_render_counts(counts); }); // submit fences only the previous job
        }
        return;
    }
    int16_t pcm[2048];
    while(frames) {size_t n=std::min<size_t>(frames,1024);VMS_PLAT(g_audio).render(pcm,n);p7audio::dump(pcm,n);frames-=n;}
}
#endif
// Audio servicing never calls Java; it is also safe inside a long interpreter slice.
static void audio_pump_impl();
static void audio_pump(){const auto phase=diag_phase_enter(runtime_diag::Phase::Audio);audio_pump_impl();diag_native_leave(phase);}
static void audio_pump_impl() {
    if(r4::state.active)return;
#ifdef __SWITCH__
    P7_SCOPE(AudioOut); // desktop has no audio output; its per-slice pump is not timed (mixing is, in Engine::render)
    if (VMS_PLAT(g_audio_sink).available()) {
        if (p7audio::g_enabled) { p7_audio_report_failure(); p7_audio_submit_pump(false); return; }
        auto& sink=VMS_PLAT(g_audio_sink);Result rc=sink.pumpThrottled(VMS_PLAT(g_audio));
        diag_audio_sample(sink); // Inline owner; no additional accessor/fence.
        if (R_FAILED(rc)) log_line("AUDIO output failure op="+std::string(VMS_PLAT(g_audio_sink).errorOperation())+" result="+std::to_string(rc));
    }
#else
    VMS_PLAT_LOCAL(previous, audio_pump_previous, static int64_t previous=0;)
    int64_t now=paint_clock_us();
    if (!previous) { previous=now; p7audio::g_pump_first_us=uint64_t(now); return; }
    int64_t elapsed=std::max<int64_t>(0,std::min<int64_t>(now-previous,200000));
    size_t frames=(size_t)(elapsed*48/1000);
    ++p7audio::g_pump_calls; p7audio::g_pump_frames+=frames; if(now-previous>200000)p7audio::g_pump_clamped_us+=uint64_t(now-previous-200000); p7audio::g_pump_elapsed_us+=uint64_t(elapsed); p7audio::g_pump_last_us=uint64_t(now);
    previous=now;
    audio_render_desktop(frames);
#endif
}
static void audio_pump_frame_impl();
static void audio_pump_frame(){const auto phase=diag_phase_enter(runtime_diag::Phase::Audio);audio_pump_frame_impl();diag_native_leave(phase);}
static void audio_pump_frame_impl() {
    if(r4::state.active)return;
    P23_HOOK(p23_service(1));
#ifdef __SWITCH__
    P7_SCOPE(AudioOut);
    if (!VMS_PLAT(g_audio_sink).available()) return;
    if (p7audio::g_enabled) { p7_audio_report_failure(); p7_audio_submit_pump(true); return; }
    auto& sink=VMS_PLAT(g_audio_sink);Result rc = sink.pumpThrottled(VMS_PLAT(g_audio),true);
    diag_audio_sample(sink);
    if (R_FAILED(rc)) log_line("AUDIO output failure op="+std::string(VMS_PLAT(g_audio_sink).errorOperation())+" result="+std::to_string(rc));
#endif
}
static void audio_suspend(bool paused) {
    if(r4::state.active){VMS_PLAT(g_audio).pause(paused);return;}
#ifdef __SWITCH__
    if(VMS_PLAT(g_audio_sink).available()) {
        Result rc=VMS_PLAT(g_audio_sink).suspend(VMS_PLAT(g_audio),paused);
        if(R_FAILED(rc))log_line("AUDIO lifecycle failure op="+std::string(VMS_PLAT(g_audio_sink).errorOperation())+" result="+std::to_string(rc));
    }
    else VMS_PLAT(g_audio).pause(paused);
#else
    VMS_PLAT(g_audio).pause(paused);
#endif
}
static void audio_reset() {
#ifdef __SWITCH__
    if(VMS_PLAT(g_audio_sink).available()) {
        Result rc=VMS_PLAT(g_audio_sink).reset(VMS_PLAT(g_audio));
        if(R_FAILED(rc))log_line("AUDIO reset failure op="+std::string(VMS_PLAT(g_audio_sink).errorOperation())+" result="+std::to_string(rc));
    }
#endif
    VMS_PLAT(g_audio).reset(); VMS_PLAT(g_tones).clear();
    VMS_PLAT(g_audio_event_window)=0;VMS_PLAT(g_audio_event_count)=0;VMS_PLAT(g_audio_event_suppressed)=0;
#ifdef __SWITCH__
    VMS_PLAT(g_audio_sink).takeStats();
#endif
}
static void align_short_sleep(JavaThreadContext& t, int64_t now) {
    refresh_thread(t);
    if (t.executing || t.state == ThreadState::Terminated) return;
    if (!pace_align_sleep(t.sleep_request, t.state == ThreadState::Sleeping, t.wake, now)) return;
    t.sleep_sample_due = t.wake;
    t.state = ThreadState::Runnable;
    t.wake = 0;
    t.sleep_request = 0;
    ++VMS_EXEC(g_pace_aligns);
}

static int64_t idle_sleep_ms() {
    // Frame sleeps of 8..20ms stay on the vsync path so they can still align.
    // Anything farther away must block the host; otherwise the display loop
    // polls that timer as fast as the CPU allows.
    int64_t now = monotonic_ms(), delay = INT64_MAX;
    bool frame_pace = false;
    for (auto& t : VMS_EXEC(g_threads)) {
        refresh_thread(t);
        if (t.state == ThreadState::Runnable && (!t.begun || !t.frames.empty())) return 0;
        if (t.sleep_request >= 8 && t.sleep_request <= 20 && t.state == ThreadState::Sleeping) frame_pace = true;
        // A paced wait's compatibility millisecond timer can expire before
        // its precise deadline. Keep that pending timer visible; otherwise
        // the no-timer fallback adds an unrelated 4ms nap near every boundary.
        if ((t.state == ThreadState::Sleeping || t.state == ThreadState::Waiting) && t.wake_us)
            delay = std::min(delay, std::max<int64_t>(0, (t.wake_us - paint_clock_us()) / 1000));
        else if (t.wake > now && (t.state == ThreadState::Sleeping || t.state == ThreadState::Waiting || t.state == ThreadState::Joining))
            delay = std::min(delay, t.wake - now);
    }
    if (frame_pace || delay <= 2) return 0;
    if (delay == INT64_MAX) return 4;
    return std::min<int64_t>(delay, 8);
}
static bool game_runnable() {
    // Only a Java thread with a stack counts. The host main context is
    // permanently Runnable and must not keep the display loop spinning.
    for (auto& t : VMS_EXEC(g_threads)) {
        refresh_thread(t);
        if (t.state != ThreadState::Runnable) continue;
        if (!t.begun || !t.frames.empty()) return true;
    }
    return false;
}
static int64_t game_wake_delay() {
    if(VMS_PLAT(g_host_paused) || VMS_EXEC(g_notify_destroyed) || VMS_EXEC(g_midlet_state)==MidletState::Destroyed)return 0;
    const int64_t now=monotonic_ms();int64_t delay=INT64_MAX;
    for(auto& t:VMS_EXEC(g_threads)) {
        align_short_sleep(t, now);
        if(t.executing || t.state==ThreadState::Terminated)continue;
        if(t.state==ThreadState::Runnable)return 0;
        if((t.state==ThreadState::Sleeping||t.state==ThreadState::Waiting) && t.wake_us)continue;
        if(t.state==ThreadState::Sleeping && t.wake>now)delay=std::min(delay,t.wake-now);
    }
    return delay==INT64_MAX ? 0 : delay;
}
// P8.6-r2: wake ~2ms before the deadline, then spin (yield) to it. The deadline itself is unchanged, so Java sleep
// semantics and game speed stay identical; only the OS oversleep (2-8ms on Switch) is removed. Default OFF.
// Opt-in: sdmc:/switch/j2me/p86-spin.enable (desktop J2ME_P86_SPIN=1) enables early-wake+spin.
static void p86_wait_until_us(int64_t due) {
    constexpr int64_t kEarly=2000;
    int64_t left=due-paint_clock_us();
    if(left<=0)return;
    if(!p86w::g_spin){ P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::microseconds(left)); }
    else {
        if(left>kEarly){ P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::microseconds(left-kEarly)); }
        const int64_t spin_from=paint_clock_us();
        { P7_SCOPE(Idle); while(paint_clock_us()<due)std::this_thread::yield(); }
        p86w::spin_us+=uint64_t(std::max<int64_t>(0,paint_clock_us()-spin_from));
    }
    const uint64_t late=uint64_t(std::max<int64_t>(0,paint_clock_us()-due));
    ++p86w::waits;p86w::late_sum_us+=late;p86w::late_max_us=std::max(p86w::late_max_us,late);p86w::late_1ms+=late>=1000;
}
static void wait_game_wake(int64_t milliseconds) {
    const int64_t started=paint_clock_us();
#ifndef __SWITCH__
    audio_pump();
#endif
    p86_wait_until_us(started+milliseconds*1000);
    VMS_EXEC(g_wake_stats).waited(paint_clock_us()-started);
}
// The Switch prologue is outside advance_game_frame's completed-paint budget.
// Only lifecycle/visibility work may run there in Native mode; ordinary queued
// callbacks must run inside the budget after input is sampled. Preserve an
// already completed dirty frame rather than overwriting it before presentation.
static bool host_boundary242_active(){return VMS_RS(g_host_boundary242_enabled)&&speed_period_us()==0;}
// A post-present speculative turn can finish the next frame in any speed mode.
// Keep it until Present instead of running another paint over the same buffer.
// Incomplete grouped frames still need Java work, and lifecycle work is always
// serviced by host_events(false). Native's paint-budget policy stays separate.
static bool host_frame_ready242(){return VMS_RS(g_host_boundary242_enabled)&&VMS_RS(g_fb_dirty)&&!speed_group_incomplete();}
static void host_lifecycle_poll242(){host_events(!host_boundary242_active()&&!host_frame_ready242());}
static bool host_speculation_allowed242(){
    return !host_boundary242_active()||VMS_EXEC(g_completed_paint241).completed_us<CompletedPaintBudget241::BudgetUs;
}
static int sched_worker_slice_ms();
static void advance_game_frame() {
    CityHostSpan city_host("advance");
    audio_pump_frame();
    const int64_t frame_started=paint_clock_us();
    VMS_EXEC(g_speed_frame_open) = false;
    VMS_EXEC(g_speed_serial_serviced) = false;
    VMS_EXEC(g_completed_paint241).begin(speed_period_us()==0);
    S0_HOOK(s0_turn_begin());
    bool worker_serviced=false;
    auto service_worker=[&](){
        worker_serviced=true;VMS_EXEC(g_completed_paint241).worker_owed=false;
        const int64_t started=paint_clock_us();
        const auto token=VMS_EXEC(g_runtime_profile).begin_worker(started);
        if(VMS_EXEC(g_thread_live)) {
            const int saved_slice=VMS_EXEC(g_slice_ms);VMS_EXEC(g_slice_ms)=sched_worker_slice_ms(); // P2.6 core policy
            thread_resume();
            VMS_EXEC(g_slice_ms)=saved_slice;
            ++VMS_EXEC(g_slices);
        }
        const int64_t finished=paint_clock_us();
        if(VMS_EXEC(g_runtime_profile).end_worker(token,finished)){
            VMS_RS(g_paint_stats).worker_elapsed(finished-started);VMS_EXEC(g_last_slice_us)=finished-started;
        }
    };
    // An event-driven canvas which repaints itself must not starve its updater.
    // On the following turn the owed worker runs first, except lifecycle work
    // (pause/destroy/resume) always retains priority.
    if(VMS_EXEC(g_completed_paint241).active && VMS_EXEC(g_completed_paint241).worker_owed && VMS_EXEC(g_thread_live) &&
       !VMS_EXEC(g_notify_destroyed) && !VMS_EXEC(g_notify_paused) && !VMS_EXEC(g_midlet_resume) && !VMS_PLAT(g_host_paused) &&
       !VMS_PLAT(g_host_frozen236) && VMS_RS(g_shown)==VMS_RS(g_current) &&
       (!VMS_RS(g_alert_deadline) || monotonic_ms()<VMS_RS(g_alert_deadline)) && VMS_EXEC(g_midlet_state)==MidletState::Active){
        ++VMS_EXEC(g_completed_paint241).worker_priorities;service_worker();
    }
    if(!VMS_EXEC(g_completed_paint241).exhausted())host_events();
    if(!VMS_EXEC(g_completed_paint241).exhausted() && !worker_serviced)service_worker();
    if(!VMS_EXEC(g_completed_paint241).exhausted())host_events();
    if(!VMS_EXEC(g_completed_paint241).exhausted() && VMS_RS(g_repaint) && (!VMS_EXEC(g_speed_frame_open) || speed_pending_group())){
        S0_ENTRY(S0Trace::E_ADVANCE);VMS_RS(g_repaint)=false;repaint_current();
    }
    S0_HOOK(s0_turn_end());
    VMS_EXEC(g_completed_paint241).end(VMS_RS(g_current),!VMS_EXEC(g_serial_calls).empty(),VMS_EXEC(g_thread_live)&&!worker_serviced);
    if(VMS_EXEC(g_midlet_state)==MidletState::Destroyed || VMS_EXEC(g_midlet_state)==MidletState::Empty){
        VMS_EXEC(g_frame_work_us)=VMS_EXEC(g_frame_work_finished_us)=0;
    }else{
        VMS_EXEC(g_frame_work_finished_us)=paint_clock_us();VMS_EXEC(g_frame_work_us)=VMS_EXEC(g_frame_work_finished_us)-frame_started;
    }
}
// The actual Switch work dispatch with host I/O injected for deterministic tests.
static bool farm_startup_paint_waiting243();
static bool p1_pending_work(){
    return !VMS_PLAT(g_host_paused)&&!VMS_PLAT(g_host_frozen236)&&VMS_EXEC(g_midlet_state)!=MidletState::Paused&&
        VMS_EXEC(g_midlet_state)!=MidletState::Destroyed&&(!VMS_EXEC(g_serial_calls).empty()||(VMS_RS(g_repaint)&&!farm_startup_paint_waiting243())||p7_pending_ends());
}
template<class Schedule,class Sleep>
static p1::Work p1_host_work_turn(bool gpu,bool heartbeat,Schedule schedule,Sleep sleep){
    CityHostSpan city_host("host_work");
    const bool runnable=game_runnable(),pending=p1_pending_work();
    p1::Turn t{gpu,VMS_RS(g_fb_dirty),heartbeat,runnable,pending,host_frame_ready242(),speed_group_incomplete(),VMS_EXEC(g_last_slice_us),
        (!VMS_RS(g_fb_dirty)&&!heartbeat&&!runnable&&!pending)?idle_sleep_ms():0};
    const auto work=p1::decide(t);p1_host_decision(int(work),t);
    switch(work){
    case p1::Work::Ready:audio_pump_frame();break;
    case p1::Work::Advance:advance_game_frame();break;
    case p1::Work::Worker:advance_game_frame();if(p1::second_worker(speed_ahead(),VMS_RS(g_fb_dirty),game_runnable()))advance_game_frame();break;
    case p1::Work::Idle:audio_pump_frame();sleep(t.nap_ms);break;
    case p1::Work::GpuScheduled:schedule(true);break;
    case p1::Work::CpuScheduled:schedule(false);break;
    }
    p1_flush("host_turn");return work;
}
static void p1_host_post_present(bool in_game,bool exit_sheet){
    CityHostSpan city_host("post_present");
    if(in_game&&!exit_sheet){p1_present();S0_HOOK(s0_present());P23_HOOK(p23_present());}if(in_game)VMS_RS(g_fb_dirty)=false;
    if(p1::speculate(in_game,exit_sheet,game_runnable(),speed_ahead(),host_speculation_allowed242()))advance_game_frame();
    p1_flush("present");
}
static void release_game_keys() {
    // Startup keys belong to the active display session. A pause cancels them
    // just as it releases keys already delivered to Java.
    VMS_EXEC(g_farm_startup).keys.clear();
    const int target=VMS_RS(g_current);
    std::vector<int> keys(VMS_PLAT(g_pressed_keys).begin(),VMS_PLAT(g_pressed_keys).end());
    VMS_PLAT(g_pressed_keys).clear();VMS_PLAT(g_key_states)=0;
    if(!target || VMS_EXEC(g_farm_startup).waiting())return;
    RootScope roots(ReentrySite::ReleaseKeys,{&target}); // S5: target is held in C++ across several keyReleased calls
    reset_game_keys(target);
    const Value* suppress = game_canvas_state(target, "suppressKeyEvents", "Z");
    const bool suppress_actions = suppress && suppress->i;
    Method* m=find_method(VMS_HEAP(g_heap)[target].cls,"keyReleased","(I)V",true);
    if(m)for(int key:keys) {
        if(suppress_actions && game_key_action(key))continue;
        run_event_callback(m,{Value::of_ref(target),Value::of_i(key)});
    }
}
static bool farm_startup_ready();
static bool startup_defer_visibility(int target) {
    // Only exact reviewed wrapper builds: their prematurely published splash
    // objects override showNotify without constructing the referenced engine.
    return target && VMS_EXEC(g_farm_startup).waiting() &&
        (VMS_EXEC(g_farm_startup).profile==FarmStartup::Profile::Alps ||
         VMS_EXEC(g_farm_startup).profile==FarmStartup::Profile::Demon) && !farm_startup_ready();
}
static void display_visibility() {
    if(VMS_PLAT(g_host_frozen236))return; // emulator freeze is not MIDP hide/show

    int target=(!VMS_PLAT(g_host_paused) && (VMS_EXEC(g_midlet_state)==MidletState::Active || VMS_EXEC(g_midlet_state)==MidletState::Empty)) ? VMS_RS(g_current) : 0;
    int old=0; RootScope roots(ReentrySite::ShowHide,{&target,&old}); // S5: old is unreferenced by VM state while hideNotify runs
    if(VMS_RS(g_shown)==target) {
        if(target && !startup_defer_visibility(target))screen_notify_size(target);
        if(VMS_RS(g_current)!=target || VMS_EXEC(g_notify_paused) || VMS_EXEC(g_notify_destroyed)) {
            if(target&&VMS_EXEC(g_farm_startup).pending_show==target){VMS_RS(g_shown)=0;VMS_EXEC(g_farm_startup).pending_show=0;}
            return;
        }
        if(target && VMS_EXEC(g_farm_startup).pending_show==target && !startup_defer_visibility(target)) {
            VMS_EXEC(g_farm_startup).pending_show=0;
            if(Method* m=find_method(VMS_HEAP(g_heap)[target].cls,"showNotify","()V",true))run_event_callback(m,{Value::of_ref(target)});
        }
        return;
    }
    old=VMS_RS(g_shown); VMS_RS(g_shown)=0;
    const bool old_unnotified=old && VMS_EXEC(g_farm_startup).pending_show==old;
    VMS_EXEC(g_farm_startup).pending_show=0;
    if(old && !old_unnotified) if(Method* m=find_method(VMS_HEAP(g_heap)[old].cls,"hideNotify","()V",true))run_event_callback(m,{Value::of_ref(old)});
    // hideNotify may itself have changed the requested screen.
    target=(!VMS_PLAT(g_host_paused) && (VMS_EXEC(g_midlet_state)==MidletState::Active || VMS_EXEC(g_midlet_state)==MidletState::Empty)) ? VMS_RS(g_current) : 0;
    VMS_RS(g_shown)=target;
    if(target) {
        // A newly shown GameCanvas starts with no key history. Host held masks
        // cannot reintroduce a key until a new press event is delivered.
        reset_game_keys(target);
        if(startup_defer_visibility(target)) VMS_EXEC(g_farm_startup).pending_show=target;
        else {
            VMS_RS(g_shown)=0; // sizeChanged runs before the new screen is visible
            screen_notify_size(target);
            if(VMS_RS(g_current)!=target || VMS_EXEC(g_notify_paused) || VMS_EXEC(g_notify_destroyed) || VMS_PLAT(g_host_paused) ||
               (VMS_EXEC(g_midlet_state)!=MidletState::Active&&VMS_EXEC(g_midlet_state)!=MidletState::Empty)) {VMS_RS(g_repaint)=true;return;}
            VMS_RS(g_shown)=target;
            if(Method* m=find_method(VMS_HEAP(g_heap)[target].cls,"showNotify","()V",true))run_event_callback(m,{Value::of_ref(target)});
        }
        VMS_RS(g_repaint)=true;
    }
}
static void lifecycle_pause(bool host) {
    if(VMS_EXEC(g_midlet_state)==MidletState::Destroyed || VMS_EXEC(g_midlet_state)==MidletState::Empty)return;
    if(host)VMS_PLAT(g_host_paused)=true;
    speed_pause_clock(true);
    if(VMS_EXEC(g_midlet_state)==MidletState::Active) {
        VMS_EXEC(g_farm_startup).pause(monotonic_ms());
        release_game_keys();
        if(VMS_RS(g_alert_deadline)) {VMS_RS(g_alert_remaining)=std::max<int64_t>(0,VMS_RS(g_alert_deadline)-monotonic_ms());VMS_RS(g_alert_deadline)=0;}
        VMS_EXEC(g_midlet_state)=MidletState::Paused; VMS_PLAT(g_key_states)=0;
        audio_suspend(true); display_visibility();
        if(Method* m=find_method(VMS_HEAP(g_heap)[VMS_EXEC(g_midlet)].cls,"__vmPause","()I",true)) {
            if(run(m,{Value::of_ref(VMS_EXEC(g_midlet))}).i==2)VMS_EXEC(g_notify_destroyed)=true;
        }
        log_line("LIFECYCLE paused");
    }
    reset_runtime_stats();
}
static void lifecycle_resume() {
    P23_HOOK(p23_reset_service_clock()); // pause time is not a game service gap
    if(!VMS_EXEC(g_midlet) || VMS_EXEC(g_midlet_state)!=MidletState::Paused || VMS_EXEC(g_notify_destroyed))return;
    VMS_PLAT(g_host_paused)=false;
    speed_pause_clock(false);
    // Reviewed packages keep their game loop in startApp (one in a constructor
    // called by startApp). Preserve that Java stack in an ordinary resumable
    // worker instead of blocking the native launcher/event loop indefinitely.
    const bool asynchronous_start =
        VMS_PLAT(g_rms_suite)=="75aaf194cbd01715d4eaa99720e6876ff2355e494d3ed5f09c33d85cae81b100" ||
        VMS_PLAT(g_rms_suite)=="e999aef37c3d294463b56d18a441cc6ebf0848cb69922f517e5c37f598ed21ff";
    p1_event("startApp enter async=%d rule=%s midlet=%d",int(asynchronous_start),asynchronous_start?"exact_suite_hash":"synchronous",VMS_EXEC(g_midlet));
    VMS_DIAG(g_p1).start_outcome=-1;
    if(asynchronous_start) {
        VMS_EXEC(g_midlet_state)=MidletState::Active;
        run(find_method(VMS_HEAP(g_heap)[VMS_EXEC(g_midlet)].cls,"__vmStartAsync","()V",true),{Value::of_ref(VMS_EXEC(g_midlet))});
        VMS_EXEC(g_farm_startup).resume(monotonic_ms());audio_suspend(false);VMS_RS(g_repaint)=true;
        if(VMS_RS(g_alert_remaining)>=0){VMS_RS(g_alert_deadline)=monotonic_ms()+VMS_RS(g_alert_remaining);VMS_RS(g_alert_remaining)=-1;}
        reset_runtime_stats();log_line("LIFECYCLE active async_start=1 completion=pending");return;
    }
    int outcome=run(find_method(VMS_HEAP(g_heap)[VMS_EXEC(g_midlet)].cls,"__vmStart","()I",true),{Value::of_ref(VMS_EXEC(g_midlet))}).i;
    VMS_DIAG(g_p1).start_outcome=outcome;p1_event("startApp complete async=0 outcome=%d",outcome);
    if(outcome==0 && !VMS_EXEC(g_notify_paused) && !VMS_EXEC(g_notify_destroyed)) {
        VMS_EXEC(g_farm_startup).resume(monotonic_ms());
        VMS_EXEC(g_midlet_state)=MidletState::Active; audio_suspend(false);VMS_RS(g_repaint)=true;
        if(VMS_RS(g_alert_remaining)>=0) {VMS_RS(g_alert_deadline)=monotonic_ms()+VMS_RS(g_alert_remaining);VMS_RS(g_alert_remaining)=-1;}
        log_line("LIFECYCLE active");
    } else if(outcome==2)VMS_EXEC(g_notify_destroyed)=true;
    reset_runtime_stats();
}
// User-controlled emulator freeze: no MIDlet pauseApp/startApp or visibility
// callbacks. Real MIDP lifecycle transitions above remain available unchanged.
static void host_freeze236(bool freeze) {
    if(VMS_EXEC(g_midlet_state)==MidletState::Destroyed || VMS_EXEC(g_midlet_state)==MidletState::Empty)return;
    if(freeze) {
        g_device_diag.owner.paused=true;g_device_diag.live.publish(g_device_diag.owner);g_device_diag.detector.reset();
        if(VMS_PLAT(g_host_frozen236))return;
        if(VMS_EXEC(g_midlet_state)!=MidletState::Active){lifecycle_pause(true);return;}
        release_game_keys();
        if(VMS_EXEC(g_notify_destroyed)){host_events();return;}
        speed_sync_clock();
        if(!VMS_EXEC(g_game_clock_inherited)) {
            VMS_EXEC(g_game_clock).start(paint_clock_us(),now_ms());VMS_EXEC(g_game_clock_inherited)=true;
        }
        VMS_PLAT(g_host_frozen236)=true;VMS_PLAT(g_host_paused)=true;VMS_PLAT(g_host_freeze_started236)=paint_clock_us();
        speed_pause_clock(true);VMS_EXEC(g_farm_startup).pause(monotonic_ms());
        if(VMS_RS(g_alert_deadline)){VMS_RS(g_alert_remaining)=std::max<int64_t>(0,VMS_RS(g_alert_deadline)-monotonic_ms());VMS_RS(g_alert_deadline)=0;}
        audio_suspend(true);VMS_PLAT(g_key_states)=0;reset_runtime_stats();
        log_line("HOSTFREEZE frozen=1 callbacks=none");
        return;
    }
    if(!VMS_PLAT(g_host_frozen236)){lifecycle_resume();return;}
    const int64_t us=std::max<int64_t>(0,paint_clock_us()-VMS_PLAT(g_host_freeze_started236));
    const int64_t ms=(us+999)/1000;
    auto shift=[](int64_t& value,int64_t delta){if(value>0)value=value>INT64_MAX-delta?INT64_MAX:value+delta;};
    auto resume_deadlines=[&](JavaThreadContext& t){shift(t.wake,ms);shift(t.wake_us,us);t.sleep_sample_due=0;};
    resume_deadlines(VMS_EXEC(g_main_thread));for(auto& t:VMS_EXEC(g_threads))resume_deadlines(t);
    VMS_PLAT(g_host_frozen236)=false;VMS_PLAT(g_host_paused)=false;VMS_PLAT(g_host_freeze_started236)=0;
    g_device_diag.owner.paused=false;g_device_diag.live.publish(g_device_diag.owner);g_device_diag.detector.reset();
    speed_pause_clock(false);VMS_EXEC(g_farm_startup).resume(monotonic_ms());
    if(VMS_RS(g_alert_remaining)>=0){VMS_RS(g_alert_deadline)=monotonic_ms()+VMS_RS(g_alert_remaining);VMS_RS(g_alert_remaining)=-1;}
    if(VMS_EXEC(g_midlet_state)==MidletState::Active)audio_suspend(false);
    VMS_RS(g_fb_dirty)=true;reset_runtime_stats();
    log_line("HOSTFREEZE frozen=0 callbacks=none shifted_deadlines_ms="+std::to_string(ms));
}
static void lifecycle_destroy(bool notify_only) {
    if(VMS_EXEC(g_run_depth) || VMS_EXEC(g_pump_depth)) {VMS_EXEC(g_notify_destroyed)=true;return;}
    if(VMS_EXEC(g_midlet_state)==MidletState::Destroyed || VMS_EXEC(g_midlet_state)==MidletState::Empty)return;
    VMS_PLAT(g_host_frozen236)=false;VMS_PLAT(g_host_freeze_started236)=0;
    VMS_EXEC(g_midlet_state)=MidletState::Destroyed; VMS_PLAT(g_key_states)=0;
    audio_suspend(true); display_visibility();
    if(!notify_only && VMS_EXEC(g_midlet)) {
        if(Method* m=find_method(VMS_HEAP(g_heap)[VMS_EXEC(g_midlet)].cls,"__vmDestroy","()V",true))run(m,{Value::of_ref(VMS_EXEC(g_midlet))});
    }
    stop_thread(3); VMS_EXEC(g_midlet_state)=MidletState::Destroyed;
    VMS_EXEC(g_midlet)=VMS_RS(g_current)=VMS_RS(g_display)=0;
    log_line(std::string("LIFECYCLE destroyed source=")+(notify_only?"notifyDestroyed":"host"));
    if(notify_only) {   // 0.2.112 F2: the MIDlet ended itself; the host menu shows one neutral hint line
        VMS_EXEC(g_self_exit)=true;
        log_line("SELFEXIT last_msg="+(VMS_EXEC(g_last_console_line).empty()?std::string("-"):self_exit_hint::clean(VMS_EXEC(g_last_console_line))));
    }
}
static bool farm_startup_ready() {
    if (!VMS_RS(g_current) || VMS_RS(g_current)>=int(VMS_HEAP(g_heap).size()) || VMS_HEAP(g_heap)[VMS_RS(g_current)].cls < 0 ||
        VMS_CL(g_classes)[VMS_HEAP(g_heap)[VMS_RS(g_current)].cls].name != VMS_EXEC(g_farm_startup).canvas_class()) return false;
    const int cid = VMS_HEAP(g_heap)[VMS_RS(g_current)].cls;
    if(VMS_EXEC(g_farm_startup).profile!=FarmStartup::Profile::Farm) {
        const char* name="",*desc="";
        switch(VMS_EXEC(g_farm_startup).profile) {
        case FarmStartup::Profile::GTA:name="cn";desc="[B";break;
        case FarmStartup::Profile::Journey:name="a";desc="Lc;";break;
        case FarmStartup::Profile::Alps:name="sound";desc="LAlpsFarmCanvas$Sound;";break;
        case FarmStartup::Profile::Demon:name="e";desc="Le;";break;
        default:return false;
        }
        Field* field=find_field(cid,name,desc);if(!field)return false;
        const int ref=field->is_static?VMS_CL(g_classes)[field->owner].statics[field->slot].ref:
            VMS_HEAP(g_heap)[VMS_RS(g_current)].fields[field->slot].ref;
        if(ref<=0 || ref>=int(VMS_HEAP(g_heap).size()))return false;
        if(VMS_EXEC(g_farm_startup).profile==FarmStartup::Profile::GTA)
            return VMS_HEAP(g_heap)[ref].is_array && VMS_HEAP(g_heap)[ref].elems.size()>=1;
        return VMS_HEAP(g_heap)[ref].cls>=0;
    }
    Field* settings = find_field(cid,"e","[B");
    Field* images = find_field(cid,"v","Ld;");
    Field* stage = find_field(cid,"_s","I");
    if (!settings || !images || !stage || !stage->is_static || VMS_CL(g_classes)[stage->owner].statics[stage->slot].i < 1) return false;
    int settings_ref = VMS_HEAP(g_heap)[VMS_RS(g_current)].fields[settings->slot].ref;
    int image_owner = VMS_HEAP(g_heap)[VMS_RS(g_current)].fields[images->slot].ref;
    if (!settings_ref || !VMS_HEAP(g_heap)[settings_ref].is_array || VMS_HEAP(g_heap)[settings_ref].elems.size() < 3 || !image_owner) return false;
    Field* title = find_field(VMS_HEAP(g_heap)[image_owner].cls,"ta","[Ljavax/microedition/lcdui/Image;");
    if (!title) return false;
    int title_ref = VMS_HEAP(g_heap)[image_owner].fields[title->slot].ref;
    if (!title_ref || !VMS_HEAP(g_heap)[title_ref].is_array || VMS_HEAP(g_heap)[title_ref].elems.size() < 3) return false;
    for (int i = 0; i < 3; ++i) if (!VMS_HEAP(g_heap)[title_ref].elems.get(i).ref) return false;
    return true;
}
static void farm_startup_update() {
    if (!VMS_EXEC(g_farm_startup).waiting() || VMS_EXEC(g_farm_startup).paused()) return;
    if (VMS_EXEC(g_farm_startup).update(monotonic_ms(),farm_startup_ready())) {
        log_line("COMPAT reviewed startup ready; releasing input=" + std::to_string(VMS_EXEC(g_farm_startup).keys.size()));
        auto keys = std::move(VMS_EXEC(g_farm_startup).keys);
        VMS_EXEC(g_farm_startup).keys.clear();
        VMS_RS(g_repaint) = true;
        for (auto key : keys) dispatch_key(key.code,key.release);
    } else if (!VMS_EXEC(g_farm_startup).waiting()) {
        log_line("COMPAT reviewed startup timeout; ordinary callbacks restored");
    }
}
// Pure eligibility query used by both repaint and the shared host dispatch.
// Do not deliver queued startup keys/callbacks while selecting an idle branch.
static bool farm_startup_paint_waiting243() {
    if(!VMS_EXEC(g_farm_startup).waiting())return false;
    if(!VMS_EXEC(g_farm_startup).paused()&&(farm_startup_ready()||monotonic_ms()>=VMS_EXEC(g_farm_startup).deadline))return false;
    auto f = VMS_CL(g_class_index).find(VMS_EXEC(g_farm_startup).canvas_class());
    if (f == VMS_CL(g_class_index).end()) return true;
    Field* stage = find_field(f->second,"_s","I");
    // The splash drawing is valid until the shared class stage switches to the
    // game branch. Both splash and game use class f but only game owns arrays.
    return !stage || !stage->is_static || VMS_CL(g_classes)[stage->owner].statics[stage->slot].i != 0;
}
static bool farm_startup_defer_paint(){
    farm_startup_update();return farm_startup_paint_waiting243();
}
static void host_events(bool callbacks) {
    P23_HOOK(p23_service(0));
#ifndef __SWITCH__
    audio_pump();
#endif
    if(VMS_EXEC(g_run_depth) || VMS_EXEC(g_pump_depth) || VMS_EXEC(g_lifecycle_busy))return;
    VMS_EXEC(g_lifecycle_busy)=true;
    if(VMS_EXEC(g_notify_destroyed)) { lifecycle_destroy(true);VMS_EXEC(g_lifecycle_busy)=false;return; }
    if(VMS_PLAT(g_host_frozen236)){VMS_EXEC(g_lifecycle_busy)=false;return;}
    if(VMS_EXEC(g_notify_paused)) {
        VMS_EXEC(g_notify_paused)=false;
        if(VMS_EXEC(g_midlet_state)==MidletState::Active) {
            speed_pause_clock(true);
            VMS_EXEC(g_farm_startup).pause(monotonic_ms());
            release_game_keys();
            if(VMS_RS(g_alert_deadline)) {VMS_RS(g_alert_remaining)=std::max<int64_t>(0,VMS_RS(g_alert_deadline)-monotonic_ms());VMS_RS(g_alert_deadline)=0;}
            VMS_EXEC(g_midlet_state)=MidletState::Paused;VMS_PLAT(g_key_states)=0;audio_suspend(true);
            log_line("LIFECYCLE paused source=notifyPaused");
            reset_runtime_stats();
        }
    }
    if(VMS_EXEC(g_midlet_resume) && !VMS_PLAT(g_host_paused)) {VMS_EXEC(g_midlet_resume)=false;lifecycle_resume();}
    display_visibility();
    if(callbacks && VMS_EXEC(g_midlet_state)!=MidletState::Paused && VMS_EXEC(g_midlet_state)!=MidletState::Destroyed && !VMS_PLAT(g_host_paused)) {
        farm_startup_update();
        if(VMS_RS(g_alert_deadline) && monotonic_ms()>=VMS_RS(g_alert_deadline)) {
            VMS_RS(g_alert_deadline)=0;
            if(VMS_RS(g_current))if(Method* m=find_method(VMS_HEAP(g_heap)[VMS_RS(g_current)].cls,"dismiss","()V",true))run(m,{Value::of_ref(VMS_RS(g_current))});
            display_visibility();
        }
        if(!VMS_EXEC(g_completed_paint241).exhausted() && !VMS_EXEC(g_completed_paint241).skip_prepaint(VMS_RS(g_current)) &&
           !VMS_EXEC(g_serial_calls).empty() && VMS_RS(g_repaint) && (!VMS_EXEC(g_speed_frame_open) || speed_pending_group())) {S0_ENTRY(S0Trace::E_SERIAL_PREPAINT);VMS_RS(g_repaint)=false;repaint_current();}
        // A callback owed after the prior turn's completed paint runs before
        // a new self-repaint. No callback is dropped, reordered in its queue,
        // or interrupted; synchronous serviceRepaints still completes inline.
        size_t count=(VMS_EXEC(g_completed_paint241).exhausted() || (speed_period_us() && VMS_EXEC(g_speed_serial_serviced))) ? 0 : std::min<size_t>(VMS_EXEC(g_serial_calls).size(),64);
        if(count)VMS_EXEC(g_completed_paint241).serviced_serial();
        if(count && speed_period_us())VMS_EXEC(g_speed_serial_serviced)=true;
        while(count-- && !VMS_EXEC(g_serial_calls).empty() && !VMS_EXEC(g_notify_destroyed) && !VMS_EXEC(g_notify_paused)) {
            p1_serial_pop();const size_t queued=VMS_EXEC(g_serial_calls).size();int ref=VMS_EXEC(g_serial_calls).front();VMS_EXEC(g_serial_calls).pop_front();
            { RootScope roots(ReentrySite::Serial,{&ref}); cont_run_serial(ref,queued); } // S5: popped Runnable is only in C++ now
            if(VMS_EXEC(g_speed_frame_open) || VMS_EXEC(g_completed_paint241).exhausted())break;
        }
        int handle;
        for(int n=0;n<64 && !VMS_EXEC(g_notify_destroyed) && p7_poll_end(handle);++n) {
            audio_event("end handle="+std::to_string(handle)+" media_us="+std::to_string(VMS_PLAT(g_audio).time(handle)));
            if(VMS_PLAT(g_tones).erase(handle)) {VMS_PLAT(g_audio).close(handle);continue;}
            auto found=VMS_CL(g_class_index).find("javax/microedition/media/NativePlayer");
            if(found!=VMS_CL(g_class_index).end())if(Method* m=find_method(found->second,"dispatchEnd","(I)V",false))run(m,{Value::of_i(handle)});
        }
    }
    VMS_EXEC(g_lifecycle_busy)=false;
    if(VMS_EXEC(g_notify_destroyed))lifecycle_destroy(true);
}

static int speed_period_us() {
    if(r4::state.active)return 0;
    if (VMS_EXEC(g_speed_cap) == 30) return 33333;
    if (VMS_EXEC(g_speed_cap) == 60) return 16667;
    return 0;
}
static bool speed_ahead() {
    const int period = speed_period_us();
    if (!period || VMS_EXEC(g_speed_last_paint_us) <= 0) return false;
    return paint_clock_us() < VMS_EXEC(g_speed_last_paint_us) + period;
}
// Keep the mixer serviced during a frame-cap wait. A 30fps deadline can
// exceed an audio block; no individual blocking sleep needs to span it.
static void speed_wait_until_us(int64_t due, bool account_added) {
    const int64_t before=paint_clock_us();
    if(account_added && due>before)VMS_EXEC(g_speed_add_us)+=due-before;
    for(;;) {
        audio_pump_frame();
        const int64_t left=due-paint_clock_us();
        if(left<=0)break;
        const int64_t sleep_started=paint_clock_us();
        { RuntimeProfileScope wait(VMS_EXEC(g_runtime_profile),RuntimeProfile::Kind::CapWait);
          if(!p86w::g_spin){ P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::microseconds(std::min<int64_t>(2000,left))); }
          else if(left>2000){ P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::microseconds(std::min<int64_t>(2000,left-2000))); }
          else p86_wait_until_us(due); }
        VMS_EXEC(g_speed_slept_us)+=paint_clock_us()-sleep_started;
    }
}
// Runtime-only Timer evidence is separate from the existing learner.
#include "timer_pacing.inc"
#include "wait_frame_runtime.inc"
#include "residual_sleep_runtime.inc"
#include "rechecked_deadline_runtime.inc"
#include "cached_elapsed_runtime.inc"
#include "cooperative_poll_runtime.inc"
#include "compensated_sleep_runtime.inc"
#include "event_frame_sleep_runtime.inc"
static bool speed_group_helper(JavaThreadContext* owner) {
    if(owner&&speed_timer_helper(*owner))return true;
    const int period=speed_period_us();
    if(!period || !owner || !owner->auto_fps.helper_drawing())return false;
    // A scene may stop visiting its primary gate but continue painting. Bound
    // grouping by the real host clock so such a scene cannot freeze presentation
    // or retain unlimited helper slots, even after the virtual clock changes.
    const int64_t limit=std::max<int64_t>(120000,int64_t(period)*4);
    if(owner->frame_group_started_us>0 && paint_clock_us()-owner->frame_group_started_us>limit) {
        speed_invalidate_group(*owner,true);
        ++VMS_EXEC(g_speed_group_timeouts);return false;
    }
    return owner==VMS_EXEC(g_speed_budget_owner);
}
static bool speed_pending_group() {
    auto* owner=VMS_EXEC(g_repaint_owner_ref)?thread_context(VMS_EXEC(g_repaint_owner_ref)):&VMS_EXEC(g_main_thread);
    return speed_group_helper(owner);
}
static bool speed_group_incomplete() {
    speed_timer_expire_clock();
    // A terminated list node can disappear between host slices. Resolve the
    // recorded owner by live address before accessing its group state.
    JavaThreadContext* owner=VMS_EXEC(g_speed_budget_owner)==&VMS_EXEC(g_main_thread)?&VMS_EXEC(g_main_thread):nullptr;
    if(!owner)for(auto& t:VMS_EXEC(g_threads))if(&t==VMS_EXEC(g_speed_budget_owner) && t.state!=ThreadState::Terminated){owner=&t;break;}
    return speed_group_helper(owner) && !owner->auto_fps.group_complete && owner->state==ThreadState::Runnable;
}
static int64_t speed_begin_frame(JavaThreadContext* owner) {
    const int period = speed_period_us();
    if(owner) {
        if(!period || speed_group_helper(owner))owner->frame_prep.reset();
        else owner->frame_prep.before_draw(VMS_RS(g_current),period,paint_clock_us());
    }
    if(period && !speed_group_helper(owner) && VMS_EXEC(g_speed_next_us)>0)
        speed_wait_until_us(speed_paint_wait_deadline(period,VMS_EXEC(g_speed_next_us),VMS_EXEC(g_speed_last_paint_us)),true);
    return paint_clock_us();
}
static void speed_note_paint(int64_t started_us, JavaThreadContext* owner) {
    speed_sync_clock();
    if(owner)++owner->owned_draws;
    const int period = speed_period_us();
    if (!period) return;
    const bool helper=speed_group_helper(owner);
    const int64_t anchor = started_us > 0 ? started_us : paint_clock_us();
    VMS_EXEC(g_speed_frame_open) = true;
    if(!helper) {
        if(owner && (owner->auto_fps.verified_period_us>0 || !owner->auto_fps.group_drawn))owner->frame_group_started_us=anchor;
        VMS_EXEC(g_speed_last_paint_us) = anchor;
        VMS_EXEC(g_speed_next_us) = speed_next_paint_deadline(period,VMS_EXEC(g_speed_next_us),anchor);
        VMS_EXEC(g_speed_budget_owner)=owner;
        ++VMS_EXEC(g_speed_budget_frames);++VMS_EXEC(g_speed_budget_window_frames);
        if(owner)owner->frame_due_us=VMS_EXEC(g_speed_next_us);
    }
    const int64_t game_us=VMS_EXEC(g_game_clock).value_us(paint_clock_us());
    const bool legacy_timing=owner&&owner->auto_fps.stable>=5&&owner->auto_fps.polling_cycle(game_us,VMS_EXEC(g_game_clock).scale);
    const bool slept=owner && (owner->sleep_paced || owner->frame_sleep_bridge);
    const bool sleep_gap=slept && owner->auto_fps.bridge_sleep_gap(game_us,VMS_EXEC(g_game_clock).scale,owner->mixed_wait);
    const bool mixed=slept && (sleep_gap || owner->auto_fps.polling_cycle(game_us,VMS_EXEC(g_game_clock).scale));
    if(owner) {
        owner->sleep_paced=owner->frame_sleep_bridge=false;
        if(mixed && !owner->mixed_wait)log_line("SPEED mode=mixed owner="+std::to_string(owner->ref)+
            " proof="+(owner->auto_fps.verified_period_us>0?std::string("certified_gate"):std::string("poll_cycle")));
        if(slept)owner->mixed_wait=mixed;
    }
    if (slept && !mixed) {
        speed_timer_block(*owner);
        if(VMS_EXEC(g_game_clock).scale>1.01)log_line("SPEED sleep_reset owner="+std::to_string(owner->ref)+
            " polls="+std::to_string(owner->auto_fps.polls)+" yields="+std::to_string(owner->auto_fps.yields)+
            " stable="+std::to_string(owner->auto_fps.stable)+" gaps="+std::to_string(owner->auto_fps.sleep_gaps)+
            " cycle_game_us="+std::to_string(game_us-owner->auto_fps.cycle_game_us));
        owner->auto_fps.reset();owner->gate_recovery.reset();
        if (VMS_EXEC(g_game_clock).scale > 1.01 && (!g_timer_clock_owner||g_timer_clock_owner==owner)) {
            VMS_EXEC(g_game_clock).rate(paint_clock_us(), 1.0);
            log_line("SPEED clock_scale=1 path=sleep");
        }
    } else if (owner && !sleep_gap) {
        // Only the owner's gate evidence may change Java time. Paint, present,
        // loading and host scheduling gaps are not a requested game period.
        double learned=owner->auto_fps.drawing(game_us,period,VMS_EXEC(g_game_clock).scale);
        const double timer_learned=speed_timer_drawing(*owner,anchor,slept||legacy_timing);
        if(learned>=1.0){g_timer_clock_owner=nullptr;}else learned=timer_learned;
        if(learned>=1.0 && learned<=8.0 && std::fabs(learned-VMS_EXEC(g_game_clock).scale)>0.03) {
            const double recovered=J2ME_OPT_GATE_RECOVERY?owner->gate_recovery.consume(learned,period,paint_clock_us()):0;
            const double next=recovered>VMS_EXEC(g_game_clock).scale?recovered:
                std::max(VMS_EXEC(g_game_clock).scale-0.2,std::min(VMS_EXEC(g_game_clock).scale+0.2,learned));
            VMS_EXEC(g_game_clock).rate(paint_clock_us(),next);
            log_line("SPEED clock_scale="+std::to_string(next)+" path="+(recovered>0?"certified-recovery":"poll")+" owner="+std::to_string(owner->ref));
        }
    }
}
static int64_t speed_frame_wake_us(int64_t requested_ms) {
    const int period = speed_period_us();
    auto& t = current_context();
    if (!period) { t.frame_due_us = 0; t.sleep_paced=false; t.frame_prep.reset(); return 0; }
    if (requested_ms < 8 || requested_ms > 120) {
        if(requested_ms<8)++g_frame_sleep_stats.short_calls;
        else ++g_frame_sleep_stats.long_calls;
        if (requested_ms > 120) {
            t.frame_due_us = 0; t.sleep_paced=false; t.auto_fps.reset(); t.gate_recovery.reset();
            t.frame_prep.reset();
        }
        return 0;
    }
    // Only a completed drawing earns one paced sleep. Background timers and
    // additional sleeps without another drawing retain their Java duration.
    if(!t.frame_due_us){++g_frame_sleep_stats.unowned;return 0;}
    ++g_frame_sleep_stats.owned;
    const int64_t due=t.frame_due_us;
    t.frame_due_us=0;
    t.sleep_paced=true;
    const Frame* caller=VMS_EXEC(g_active_stack) && !VMS_EXEC(g_active_stack)->empty()?&VMS_EXEC(g_active_stack)->back():nullptr;
    t.frame_prep.arm(VMS_RS(g_current),period,caller?caller->method:nullptr,caller?caller->op_pc:-1);
    const int64_t now=paint_clock_us();
    const int64_t wake_due=t.frame_prep.wake_deadline(due,now,period);
    return speed_owned_frame_deadline(wake_due,now,requested_ms,VMS_EXEC(g_speed_cut_us),VMS_EXEC(g_speed_add_us));
}
static void speed_wait_paced_sleep() {
    if (!speed_period_us() || VMS_RS(g_fb_dirty) || VMS_RS(g_repaint) || !VMS_EXEC(g_serial_calls).empty() || game_runnable()) return;
    int64_t now = paint_clock_us(), due = 0;
    for (const auto& t : VMS_EXEC(g_threads)) {
        if ((t.state == ThreadState::Sleeping || t.state == ThreadState::Waiting) && t.wake_us > now && (!due || t.wake_us < due)) due = t.wake_us;
    }
    if (!due) return;
    // Return to the host loop after one short wait so input and pending work
    // can be checked, rather than parking it for a full frame period.
    speed_wait_until_us(std::min<int64_t>(due,paint_clock_us()+2000),false);
}
#include "scheduler_core.inc"
static void repaint_current() {
    if(VMS_EXEC(g_notify_paused)&&VMS_RS(g_shown)!=VMS_RS(g_current))return; // sizeChanged paused before first visibility
    if (VMS_RS(g_current) <= 0 || VMS_RS(g_paint_depth) || VMS_PLAT(g_host_paused) || VMS_EXEC(g_notify_destroyed) || VMS_EXEC(g_midlet_state) == MidletState::Paused || VMS_EXEC(g_midlet_state) == MidletState::Destroyed) return;
    if (farm_startup_defer_paint()) { VMS_RS(g_repaint) = true; return; }
    CityHostSpan city_host("paint");
    const int canvas = VMS_RS(g_current);
    Method* paint = find_method(VMS_HEAP(g_heap)[canvas].cls, "paint", "(Ljavax/microedition/lcdui/Graphics;)V", true);
    if (!paint) return;
    // Zero is a host/main request, even when a worker services it synchronously.
    JavaThreadContext* frame_owner = VMS_EXEC(g_repaint_owner_ref)?thread_context(VMS_EXEC(g_repaint_owner_ref)):&VMS_EXEC(g_main_thread);
    if (VMS_EXEC(g_speed_frame_open) && VMS_EXEC(g_can_suspend) && !speed_group_helper(frame_owner)) {
        VMS_RS(g_repaint) = true;VMS_EXEC(g_yield) = true;return;
    }
    const uint64_t wait_request=speed_frame_wait_paint_token(frame_owner,canvas);
    VMS_EXEC(g_repaint_owner_ref) = 0;
    VMS_EXEC(g_repaint_yield_canvas)=VMS_EXEC(g_repaint_owner_yields)=0;VMS_EXEC(g_repaint_yield_handoff)=false;
    const int64_t paint_started_us = speed_begin_frame(frame_owner);
    p8_paint_begin();
    p1_paint_start();
    S0_HOOK(s0_paint_begin());
    const auto paint_token=VMS_EXEC(g_runtime_profile).begin_paint(paint_started_us);
    int gid = alloc_obj(ensure_class("javax/microedition/lcdui/Graphics"));
    gfx_bind(gid, 0, 0xffffff);
    draw_begin();
    const auto graphics_token=VMS_RS(g_graphics_profile).begin_paint();
    // Paint runs to completion unless Java explicitly blocks/yields. Ordinary
    // worker stacks retain their budgets; native paint continuations cannot be
    // saved, and pumping an updater here can invalidate the frame being drawn.
    paint_hotspots241::begin();
    const int previous_paint_canvas=VMS_EXEC(g_exec_ctx).active_paint_canvas;
    VMS_EXEC(g_exec_ctx).active_paint_canvas=canvas;
    ++VMS_RS(g_paint_depth);
    { RootScope roots(ReentrySite::Paint,{&canvas,&gid}); run(paint, {Value::of_ref(canvas), Value::of_ref(gid)}); } // S5: scope ends before the paint GC safepoint
    --VMS_RS(g_paint_depth);
    VMS_EXEC(g_exec_ctx).active_paint_canvas=previous_paint_canvas;
    VMS_RS(g_graphics_profile).end_paint(graphics_token);
    const int64_t paint_finished_us=paint_clock_us();
    p1_paint_complete(paint_finished_us-paint_started_us);
    if(r4::state.active)++r4::state.paints;else VMS_EXEC(g_completed_paint241).painted(paint_finished_us-paint_started_us);
    RuntimePaintAttribution paint_attribution;
    if(VMS_EXEC(g_runtime_profile).end_paint(paint_token,paint_finished_us,&paint_attribution))
        VMS_RS(g_paint_stats).completed(paint_finished_us-paint_started_us);
    paint_hotspots241::finish(paint_finished_us-paint_started_us,paint_attribution);
    speed_note_paint(paint_started_us, frame_owner);
    speed_frame_wait_painted(frame_owner,canvas,wait_request);
    if (!r4::state.active && VMS_EXEC(g_can_suspend) && (VMS_EXEC(g_completed_paint241).exhausted() ||
        (speed_period_us() && !(frame_owner && frame_owner->auto_fps.grouped_update())))) VMS_EXEC(g_yield) = true;
    draw_end(VMS_CL(g_classes)[VMS_HEAP(g_heap)[canvas].cls].name);
    p8_frame_end(paint_clock_us(),paint_finished_us-paint_started_us);
    VMS_RS(g_fb_dirty) = true;
    S0_HOOK(s0_surface());
    gc_safepoint(GcPoint::Paint);
}
static bool read_whole(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}
static uint32_t crc32(const uint8_t* data, size_t n);
static bool unzip(const std::vector<uint8_t>& zip, std::unordered_map<std::string, std::vector<uint8_t>>& out, std::string& err) {
    auto fail = [&](const std::string& why) { err=why; return false; };
    err.clear();
    if(zip.size()>=20&&ru32le(zip.data())==0x10201a7a&&ru32le(zip.data()+16)==12)return fail("Symbian SIS/SISX package is not a Java JAR (renamed extension)");
    if(zip.size()<22)return fail("zip end directory missing");
    size_t eocd=zip.size(),first=zip.size()>65557?zip.size()-65557:0;
    // Find an end record whose comment and directory end at the declared bounds.
    for(size_t pos=zip.size()-22;;--pos) {
        if(ru32le(&zip[pos])==0x06054b50 && pos+22+ru16le(&zip[pos+20])==zip.size()) {
            uint64_t end=(uint64_t)ru32le(&zip[pos+16])+ru32le(&zip[pos+12]);
            if(end==pos){eocd=pos;break;}
        }
        if(pos==first)break;
    }
    if(eocd==zip.size())return fail("zip end directory invalid or ZIP64 unsupported");
    uint16_t count=ru16le(&zip[eocd+10]);
    if(ru16le(&zip[eocd+4]) || ru16le(&zip[eocd+6]) || count!=ru16le(&zip[eocd+8]))return fail("zip split archive unsupported");
    if(count==0xffff)return fail("zip ZIP64 unsupported");
    size_t pos=ru32le(&zip[eocd+16]);
    std::unordered_map<std::string,std::vector<uint8_t>> extracted;
    for(unsigned entry=0;entry<count;++entry) {
        if(pos>eocd || eocd-pos<46 || ru32le(&zip[pos])!=0x02014b50)return fail("zip central header truncated");
        uint16_t flags=ru16le(&zip[pos+8]),method=ru16le(&zip[pos+10]);
        uint32_t crc=ru32le(&zip[pos+16]),csize=ru32le(&zip[pos+20]),usize=ru32le(&zip[pos+24]);
        size_t nlen=ru16le(&zip[pos+28]),elen=ru16le(&zip[pos+30]),clen=ru16le(&zip[pos+32]);
        if(nlen+elen+clen>eocd-pos-46)return fail("zip central name truncated");
        std::string name((const char*)&zip[pos+46],nlen);
        if(flags & (1|64))return fail("zip encryption unsupported: "+name);
        if(csize==0xffffffff || usize==0xffffffff || ru32le(&zip[pos+42])==0xffffffff)return fail("zip ZIP64 unsupported");
        if(ru16le(&zip[pos+34]))return fail("zip split entry unsupported");
        size_t local=ru32le(&zip[pos+42]),central=ru32le(&zip[eocd+16]);
        if(local>central || central-local<30 || ru32le(&zip[local])!=0x04034b50)return fail("zip local header invalid: "+name);
        size_t ln=ru16le(&zip[local+26]),le=ru16le(&zip[local+28]);
        if(ln+le>central-local-30)return fail("zip local name truncated: "+name);
        uint16_t flag_diff=ru16le(&zip[local+6])^flags;
        // ASCII filenames have identical bytes in UTF-8 and legacy ZIP encodings.
        // Some writers add bit 11 only to the central directory when repacking.
        bool ascii_name=std::all_of(name.begin(),name.end(),[](unsigned char c){return c<128;});
        if(ru16le(&zip[local+8])!=method || (flag_diff & ~0x0800) || ((flag_diff & 0x0800) && !ascii_name) ||
           std::string((const char*)&zip[local+30],ln)!=name)return fail("zip header mismatch: "+name);
        size_t data=local+30+ln+le;
        if(csize>central-data)return fail("zip data truncated: "+name);
        // Central sizes locate data; descriptors may have either 12 or 16 bytes.
        std::vector<uint8_t> raw;
        if(method==0) {
            if(csize!=usize)return fail("zip stored size mismatch: "+name);
            raw.assign(zip.begin()+data,zip.begin()+data+csize);
        } else if(method==8) {
            raw.resize(usize?usize:1);
            unsigned long dlen=raw.size(),slen=csize;
            int rc=puff(raw.data(),&dlen,zip.data()+data,&slen);
            if(rc || dlen!=usize || slen!=csize)return fail("zip inflate mismatch: "+name);
            raw.resize(usize);
        } else return fail("zip method unsupported: "+std::to_string(method));
        if(crc32(raw.data(),raw.size())!=crc)return fail("zip CRC mismatch: "+name);
        if(!name.empty() && name.back()!='/')extracted[name]=std::move(raw);
        pos+=46+nlen+elen+clen;
    }
    if(pos!=eocd)return fail("zip directory size mismatch");
    out=std::move(extracted);return true;
}
static std::string trim_copy(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    size_t i = 0; while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}
static void add_prop_line(const std::string& row, bool overwrite) {
    auto c = row.find(':');
    if (c == std::string::npos) return;
    std::string key = trim_copy(row.substr(0, c));
    if (key.empty()) return;
    if (!overwrite && VMS_CL(g_props).find(key) != VMS_CL(g_props).end()) return;
    VMS_CL(g_props)[key] = trim_copy(row.substr(c + 1));
}
static void load_text_props(std::string text, bool overwrite) {
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text = text.substr(3);
    std::string line, folded;
    auto flush = [&]() { if (!folded.empty()) add_prop_line(folded, overwrite); folded.clear(); };
    for (size_t i = 0; i <= text.size(); ++i) {
        char ch = i < text.size() ? text[i] : '\n';
        if (ch == '\r') continue;
        if (ch != '\n') { line.push_back(ch); continue; }
        if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) folded += trim_copy(line);
        else { flush(); folded = line; }
        line.clear();
    }
    flush();
}
static void load_props() {
    auto it = VMS_CL(g_jar_files).find("META-INF/MANIFEST.MF");
    if (it == VMS_CL(g_jar_files).end()) {
        for (auto& kv : VMS_CL(g_jar_files))
            if (kv.first.size() >= 12 && kv.first.find("MANIFEST.MF") != std::string::npos) { it = VMS_CL(g_jar_files).find(kv.first); break; }
    }
    if (it == VMS_CL(g_jar_files).end()) return;
    std::string text(it->second.begin(), it->second.end());
    if (text.size() >= 2 && (unsigned char)text[0] == 0xFF && (unsigned char)text[1] == 0xFE) {
        std::string wide;
        for (size_t i = 2; i + 1 < text.size(); i += 2) wide.push_back(text[i]);
        text = wide;
    }
    load_text_props(text, true);
}
static bool has_midlet_prop() {
    for (auto& kv : VMS_CL(g_props)) {
        std::string key = kv.first;
        for (char& c : key) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        if (key == "midlet-1") return true;
    }
    return false;
}
static void load_sidecar_jad(const std::string& jar_path) {
    if (has_midlet_prop()) return;
    auto dot = jar_path.rfind('.');
    if (dot == std::string::npos) return;
    std::string jad = jar_path.substr(0, dot) + ".jad";
    std::vector<uint8_t> bytes;
    if (!read_whole(jad, bytes) && !read_whole(jar_path.substr(0, dot) + ".JAD", bytes)) return;
    load_text_props(std::string(bytes.begin(), bytes.end()), false);
    if (has_midlet_prop()) log_line("JAD " + jad);
}
static std::string class_super_name(const std::vector<uint8_t>& data) {
    if (data.size() < 10 || ru32(&data[0]) != 0xCAFEBABE) return "";
    size_t p = 8;
    auto u1 = [&]() { return p < data.size() ? data[p++] : 0; };
    auto u2 = [&]() { uint16_t v = p + 1 < data.size() ? ru16(&data[p]) : 0; p += 2; return v; };
    int count = u2();
    std::vector<std::string> utf(count);
    std::vector<int> cls(count, 0);
    for (int i = 1; i < count; ++i) {
        int tag = u1();
        if (tag == 1) { int n = u2(); if (p + n > data.size()) return ""; utf[i].assign((const char*)&data[p], n); p += n; }
        else if (tag == 7) cls[i] = u2();
        else if (tag == 8 || tag == 16 || tag == 19 || tag == 20) u2();
        else if (tag == 3 || tag == 4) p += 4;
        else if (tag == 5 || tag == 6) { p += 8; ++i; }
        else if (tag == 9 || tag == 10 || tag == 11 || tag == 12 || tag == 17 || tag == 18) p += 4;
        else if (tag == 15) p += 3;
        else return "";
    }
    u2();
    int this_idx = u2(), super_idx = u2();
    if (super_idx <= 0 || super_idx >= count) return "";
    int name_idx = cls[super_idx];
    if (name_idx <= 0 || name_idx >= count) return "";
    return utf[name_idx];
}
static bool class_declares(const std::vector<uint8_t>& data, const char* method, const char* descriptor) {
    if (data.size() < 10 || ru32(&data[0]) != 0xCAFEBABE) return false;
    size_t p = 8;
    auto u1 = [&]() { return p < data.size() ? data[p++] : 0; };
    auto u2 = [&]() { uint16_t v = p + 1 < data.size() ? ru16(&data[p]) : 0; p += 2; return v; };
    auto u4 = [&]() { uint32_t v = p + 3 < data.size() ? ru32(&data[p]) : 0; p += 4; return v; };
    int count = u2();
    std::vector<std::string> utf(count);
    for (int i = 1; i < count; ++i) {
        int tag = u1();
        if (tag == 1) { int n = u2(); if (p + n > data.size()) return false; utf[i].assign((const char*)&data[p], n); p += n; }
        else if (tag == 7 || tag == 8 || tag == 16 || tag == 19 || tag == 20) u2();
        else if (tag == 3 || tag == 4) p += 4;
        else if (tag == 5 || tag == 6) { p += 8; ++i; }
        else if (tag == 9 || tag == 10 || tag == 11 || tag == 12 || tag == 17 || tag == 18) p += 4;
        else if (tag == 15) p += 3;
        else return false;
    }
    if (p + 6 > data.size()) return false;
    u2(); u2(); u2();
    int interfaces = u2();
    if (p + size_t(interfaces) * 2 > data.size()) return false;
    p += size_t(interfaces) * 2;
    auto skip_members = [&](bool want) {
        int members = u2();
        for (int i = 0; i < members; ++i) {
            if (p + 8 > data.size()) return false;
            u2();
            int name_idx = u2(), desc_idx = u2(), attrs = u2();
            bool hit = want && name_idx > 0 && name_idx < count && desc_idx > 0 && desc_idx < count &&
                       utf[name_idx] == method && (descriptor == nullptr || utf[desc_idx] == descriptor);
            for (int a = 0; a < attrs; ++a) {
                if (p + 6 > data.size()) return false;
                u2();
                uint32_t len = u4();
                if (p + len > data.size()) return false;
                p += len;
            }
            if (hit) return true;
        }
        return false;
    };
    if (skip_members(false)) return false;
    return skip_members(true);
}
static bool extends_midlet(const std::string& name, int depth) {
    if (depth > 8) return false;
    auto it = VMS_CL(g_jar_files).find(name + ".class");
    if (it == VMS_CL(g_jar_files).end()) return false;
    std::string super = class_super_name(it->second);
    if (super == "javax/microedition/midlet/MIDlet") return true;
    if (super.empty() || super == "java/lang/Object") return false;
    return extends_midlet(super, depth + 1);
}
static std::string class_entry_name(std::string name) {
    for (char& c : name) if (c == '\\') c = '/';
    while (!name.empty() && (name[0] == '/' || name[0] == '.')) name.erase(name.begin());
    if (name.size() >= 6) {
        std::string ext = name.substr(name.size() - 6);
        for (char& c : ext) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        if (ext == ".class") name.resize(name.size() - 6);
    }
    return name;
}
static std::string find_midlet_class() {
    std::string direct, declared;
    for (auto& kv : VMS_CL(g_jar_files)) {
        if (kv.first.find('$') != std::string::npos) continue;
        bool named = kv.first.size() >= 6;
        if (named) {
            std::string ext = kv.first.substr(kv.first.size() - 6);
            for (char& c : ext) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
            named = ext == ".class";
        }
        if (!named && (kv.second.size() < 4 || ru32(kv.second.data()) != 0xCAFEBABE)) continue;
        std::string name = class_entry_name(kv.first);
        if (name.empty() || name.find('$') != std::string::npos) continue;
        std::string base = name;
        auto slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);
        bool titled = base.find("MIDlet") != std::string::npos || base.find("Midlet") != std::string::npos || base.find("midlet") != std::string::npos;
        std::string super = class_super_name(kv.second);
        bool midlet = super == "javax/microedition/midlet/MIDlet" ||
            (!super.empty() && super != "java/lang/Object" && extends_midlet(super, 0));
        if (midlet) {
            if (titled) return name;
            if (direct.empty()) direct = name;
            continue;
        }
        if (class_declares(kv.second, "startApp", "()V") && class_declares(kv.second, "destroyApp", nullptr)) {
            if (titled) return name;
            if (declared.empty()) declared = name;
        }
    }
    return !direct.empty() ? direct : declared;
}
static std::string manifest_midlet() {
    for (auto& kv : VMS_CL(g_props)) {
        std::string key = kv.first;
        for (char& c : key) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (key != "midlet-1") continue;
        auto a = kv.second.rfind(',');
        if (a == std::string::npos) return trim_copy(kv.second);
        return trim_copy(kv.second.substr(a + 1));
    }
    return "";
}
static void launch_class(const std::string& main, const std::string& suite = "", bool import_legacy = false) {
    // Copy before stop_thread: callers may pass the current identity by reference.
    const std::string identity = suite.empty() ? rms_class_identity(main, VMS_CL(g_classpath)) : suite;
    startup_trace("launch/stop-before");
    stop_thread();
    startup_trace("launch/stop-after");
    VMS_EXEC(g_last_console_line).clear(); VMS_EXEC(g_self_exit)=false;   // 0.2.112 F2: per-launch
    p1_new_launch(identity,main);
    VMS_PLAT(g_rms_suite) = identity; VMS_PLAT(g_rms_import_legacy) = import_legacy;
    // This released package selects absent sound1.mp3 by default but supplies
    // only sound1.mid. Supply the missing JAD property, never change its bytes.
    if(identity=="0e5e8024cbe7405614aff5d1366f6b9fc6bb1824023ecedbf03dc973c26521fe" &&
       !VMS_CL(g_props).count("sound") && VMS_CL(g_jar_files).count("sound1.mid") && !VMS_CL(g_jar_files).count("sound1.mp3")) {
        VMS_CL(g_props)["sound"]="0";
        log_line("COMPAT exact_JAR_property sound=0 reason=missing_JAD_only_MIDI_resource");
    }
    VMS_EXEC(g_farm_startup).start(identity,monotonic_ms());
    if (VMS_EXEC(g_farm_startup).waiting()) log_line("COMPAT reviewed startup enabled for exact JAR identity; deadline_ms=30000");
    log_line("RMS scope=" + identity + " legacy_import=" + (import_legacy ? "requested-for-this-launch" : "off"));
    VMS_EXEC(g_execution_profile).reset();
    VMS_HEAP(g_frame_buffer_pool).clear();
    VMS_HEAP(g_primitive_buffers).clear();
    std::vector<char>().swap(VMS_HEAP(g_gc_mark_scratch));
    std::vector<int>().swap(VMS_HEAP(g_gc_work_scratch));
    VMS_CL(g_classes).clear(); VMS_CL(g_class_index).clear();
    resver::bump_epoch(); // P2.5: launch/suite switch invalidates every ResourceKey
    p7::pool().epoch_clear(resver::g_epoch); p7::counters_reset(); p8_caught_reset(); // P7.2 / P8.2
    VMS_HEAP(g_heap).assign(1, Obj{}); VMS_HEAP(g_free).clear();
    VMS_DIAG(g_exception_traces).clear();
    VMS_HEAP(g_array_payload_live) = VMS_HEAP(g_array_payload_peak) = VMS_HEAP(g_array_alloc_since_gc) = 0;
    VMS_HEAP(g_gc_array_refs_scanned) = VMS_HEAP(g_gc_cycles) = 0; VMS_HEAP(g_alloc_since) = 0;
    VMS_RS(g_gfx_color).clear(); VMS_RS(g_gfx_target).clear(); VMS_RS(g_gfx_tx).clear(); VMS_RS(g_gfx_ty).clear(); VMS_RS(g_clip).clear();
    VMS_RS(g_current) = 0; VMS_RS(g_display) = 0; VMS_EXEC(g_midlet) = 0; VMS_RS(g_paint_serial) = 0;
    if ((int)VMS_RS(g_fb).size() != VMS_RS(g_screen_w) * VMS_RS(g_screen_h) * 3) VMS_RS(g_fb).assign((size_t)VMS_RS(g_screen_w) * VMS_RS(g_screen_h) * 3, 0x20);
    startup_trace("launch/class-before");
    int cid = ensure_class(main);
    startup_trace("launch/class-after");
    Method* init = find_method(cid, "<init>", "()V", true);
    Method* start = find_method(cid, "startApp", "()V", true);
    if (!init || !start) die("MIDlet needs <init> and startApp");
    int self = alloc_obj(cid); VMS_EXEC(g_midlet) = self;
    startup_trace("launch/init-before");
    { RootScope roots(ReentrySite::LaunchInit,{&self}); run(init, {Value::of_ref(self)}); } // S5: scope ends before startApp/host turns
    startup_trace("launch/init-after");
    VMS_EXEC(g_midlet_state)=MidletState::Paused;
    startup_trace("launch/resume-before");
    lifecycle_resume();
    startup_trace("launch/resume-after");
    host_events();
    if (VMS_EXEC(g_thread_live)) thread_pump();
    else if (VMS_RS(g_current)) repaint_current();
    p1_flush("launch");
    log_line("LAUNCH ok canvas=" + (VMS_RS(g_current) > 0 ? VMS_CL(g_classes)[VMS_HEAP(g_heap)[VMS_RS(g_current)].cls].name : std::string("-")) +
             " thread=" + std::string(VMS_EXEC(g_thread_live) ? "1" : "0") +
             " slots=" + std::to_string(VMS_HEAP(g_heap).size()) + " freelist=" + std::to_string(VMS_HEAP(g_free).size()));
}
static int adapt_key(int code) {
    if (VMS_RS(g_current) > 0 && type_compatible(VMS_RS(g_current), "com/nokia/mid/ui/FullCanvas")) {
        if (code == 1) return -1;
        if (code == 6) return -2;
        if (code == 2) return -3;
        if (code == 5) return -4;
    }
    return code;
}
static void dispatch_key(int code, bool release) {
    {char line[144];std::snprintf(line,sizeof(line),"code=%d release=%d canvas=%d paused=%d lifecycle=%d",code,int(release),VMS_RS(g_current),int(VMS_PLAT(g_host_paused)),int(VMS_EXEC(g_midlet_state)));const auto now=paint_clock_us();g_device_diag.flight.note(now,"input",line);g_device_diag.owner.input_us=now;g_device_diag.owner.key=code;g_device_diag.owner.release=release;g_device_diag.live.publish(g_device_diag.owner);}
    // Command presses never reached keyPressed. Match their raw host release
    // even if commandAction switched screen, changed key mapping, or paused.
    if(release && VMS_PLAT(g_command_keys).erase(code))return;
    if (!code || VMS_RS(g_current) <= 0 || VMS_PLAT(g_host_paused) || VMS_EXEC(g_notify_destroyed) || VMS_EXEC(g_midlet_state) == MidletState::Paused || VMS_EXEC(g_midlet_state) == MidletState::Destroyed) return;
    if (VMS_EXEC(g_farm_startup).waiting()) {
        if (VMS_EXEC(g_farm_startup).enqueue(code,release)) {
            log_line(std::string("COMPAT reviewed startup queued ") + (release ? "release" : "press") + " code=" + std::to_string(code));
            return;
        }
        log_line("COMPAT reviewed startup input overflow; ordinary callbacks restored");
    }
    // A delivered host press starts a new round, including after Tracker.clear
    // discarded its old release. Ignored paused/startup input keeps old capture.
    if(!release)VMS_PLAT(g_command_keys).erase(code);
    int send = adapt_key(code);
    p1_event("key code=%d effective=%d release=%d canvas=%d",code,send,int(release),VMS_RS(g_current));
    const int target=VMS_RS(g_current);
    if(!release && VMS_EXEC(g_midlet_state)==MidletState::Active && VMS_RS(g_shown)==target &&
       type_compatible(target,"javax/microedition/lcdui/Canvas") &&
       !type_compatible(target,"com/nokia/mid/ui/FullCanvas") &&
       !type_compatible(target,"javax/microedition/lcdui/game/GameCanvas") &&
       !screen_meta(VMS_HEAP(g_heap)[target].text).full) {
        const int cls=ensure_class("javax/microedition/lcdui/Displayable");
        Field* count=find_field(cls,"commandCount","I");
        Field* listener=find_field(cls,"listener","Ljavax/microedition/lcdui/CommandListener;");
        Field* menu=find_field(cls,"commandMenu","Z");
        if(count && listener && menu && VMS_HEAP(g_heap)[target].fields[count->slot].i>0 &&
           VMS_HEAP(g_heap)[target].fields[listener->slot].ref &&
           (send==-6 || send==-7 || VMS_HEAP(g_heap)[target].fields[menu->slot].i)) {
            // Use the existing Displayable binding/menu contract. FIRE and
            // numeric/directional keys belong to Canvas unless its menu is open.
            Method* command=find_method(cls,"dispatchCommand","(I)Z",false);
            VMS_PLAT(g_command_keys).insert(code);
            if(run_event_callback(command,{Value::of_ref(target),Value::of_i(send)}).i) {
                log_line("KEY midp command code="+std::to_string(send));
                VMS_RS(g_repaint)=true;
                if(!VMS_EXEC(g_notify_destroyed) && !VMS_EXEC(g_thread_live))repaint_current();
                return;
            }
            VMS_PLAT(g_command_keys).erase(code);
        }
    }
    const int action = game_key_action(send);
    if (action && VMS_RS(g_shown) == VMS_RS(g_current)) {
        if (Value* held = game_canvas_state(VMS_RS(g_current), "keyHeld")) {
            const int bit = 1 << action;
            if (release) held->i &= ~bit;
            else {
                held->i |= bit;
                if (Value* latched = game_canvas_state(VMS_RS(g_current), "keyLatched")) latched->i |= bit;
            }
            if (Value* suppress = game_canvas_state(VMS_RS(g_current), "suppressKeyEvents", "Z"))
                if (suppress->i) return;
        }
    }
    if(release)VMS_PLAT(g_pressed_keys).erase(send);else VMS_PLAT(g_pressed_keys).insert(send);
    const char* nm = release ? "keyReleased" : "keyPressed";
    Method* m = find_method(VMS_HEAP(g_heap)[VMS_RS(g_current)].cls, nm, "(I)V", true);
    std::string cn = VMS_CL(g_classes)[VMS_HEAP(g_heap)[VMS_RS(g_current)].cls].name;
    if (!m) { log_line(std::string("KEY nohandler ") + nm); return; }
    log_line(std::string("KEY midp ") + nm + " code=" + std::to_string(send) + " canvas=" + cn);
    // Do not drop the clock on a key. A tiled map repeats the same text and
    // image count, so the static test fired while walking and pulled 60 down to 30.
    CityKeySpan city_key(code,send,release,VMS_RS(g_current),m,VMS_EXEC(g_fast_ops)+VMS_EXEC(g_slow_ops));
    run_event_callback(m, {Value::of_ref(VMS_RS(g_current)), Value::of_i(send)});
    city_key.end();
    if (!release && VMS_RS(g_current) && !type_compatible(VMS_RS(g_current),"javax/microedition/lcdui/Canvas"))VMS_RS(g_repaint)=true;
    if (!release && !VMS_EXEC(g_thread_live)) repaint_current();
}
static uint32_t crc32(const uint8_t* data, size_t n) {
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; ++i) { c ^= data[i]; for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320u & -(c & 1)); }
    return ~c;
}
static uint32_t adler32(const uint8_t* data, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) { a = (a + data[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}
static void write_png(const std::string& path, const std::vector<uint8_t>& rgb, int w, int h) {
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + ((size_t)y + 1) * w * 3);
    }
    std::vector<uint8_t> zlib{0x78, 0x01};
    size_t off = 0;
    while (off < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - off);
        bool last = off + n == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(n & 255); zlib.push_back((n >> 8) & 255);
        uint16_t nn = (uint16_t)~n;
        zlib.push_back(nn & 255); zlib.push_back((nn >> 8) & 255);
        zlib.insert(zlib.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
    }
    uint32_t ad = adler32(raw.data(), raw.size());
    zlib.push_back((ad >> 24) & 255); zlib.push_back((ad >> 16) & 255); zlib.push_back((ad >> 8) & 255); zlib.push_back(ad & 255);
    auto be32 = [](uint32_t v) { return std::vector<uint8_t>{(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; };
    auto chunk = [&](const char* type, const std::vector<uint8_t>& body) {
        std::vector<uint8_t> o = be32((uint32_t)body.size());
        o.insert(o.end(), type, type + 4); o.insert(o.end(), body.begin(), body.end());
        auto cb = be32(crc32(o.data() + 4, body.size() + 4)); o.insert(o.end(), cb.begin(), cb.end());
        return o;
    };
    std::vector<uint8_t> ihdr = be32(w); auto hb = be32(h); ihdr.insert(ihdr.end(), hb.begin(), hb.end()); ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    std::ofstream o(path, std::ios::binary);
    const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    o.write((const char*)sig, 8);
    auto a = chunk("IHDR", ihdr); o.write((const char*)a.data(), a.size());
    auto b = chunk("IDAT", zlib); o.write((const char*)b.data(), b.size());
    auto c = chunk("IEND", {}); o.write((const char*)c.data(), c.size());
}


#ifdef __SWITCH__
static void report_cpu_clock(bool force);
#endif
// P8.7: per-second stats block (CONT summary … PREPSTAT). Default every 10s; p1-stat.enable / J2ME_P1_STAT=1 → 1s.
// Always drains the 1s paint/wake windows so counters do not grow when we skip logging.
static bool p1_stat_every_1s(){
#ifdef __SWITCH__
    struct stat st{}; return stat("sdmc:/switch/j2me/p1-stat.enable",&st)==0;
#else
    const char* e=std::getenv("J2ME_P1_STAT"); return e&&std::strcmp(e,"1")==0;
#endif
}
static void maybe_emit_runtime_stats(){
    const int64_t stats_now_us=paint_clock_us();
    if(!VMS_RS(g_paint_stats).ready(stats_now_us)) return;
    static int64_t s_last_emit_us=0; static bool s_cfg=false; static bool s_every1=false;
    if(!s_cfg){ s_cfg=true; s_every1=p1_stat_every_1s();
        log_line(std::string("P1STAT_CONFIG period_s=")+(s_every1?"1":"10")+" enable=p1-stat.enable"); }
    const int64_t period_us=s_every1?1000000:10000000;
    const bool emit=(s_last_emit_us==0)||(stats_now_us-s_last_emit_us>=period_us);
    const PaintStatsWindow stats=VMS_RS(g_paint_stats).take(stats_now_us,VMS_RS(g_game_frames));
    // 0.2.112 F4: canvas_fps = present() executions in the same window; the old paint-callback rate stays as paint_calls/paint_fps.
    // 0.2.113 F4: the baseline is re-taken by reset_runtime_stats() (launch, continue/restore, resume), so the first
    // window never includes presents from before this launch.
    const uint64_t present_now=VMS_RS(g_present_frames);
    const uint64_t window_presents=present_now>=g_stat_present_seen?present_now-g_stat_present_seen:0; g_stat_present_seen=present_now;
    const WakeStats wake=VMS_EXEC(g_wake_stats).take();
    const auto work=VMS_EXEC(g_runtime_profile).take();
    diag_window(stats_now_us,stats,work,window_presents); // Preserve all ten one-second windows before legacy resets.
    if(!emit){
        // silent drain of the same windows the log path would consume
        // Keep the calling-thread CPU baseline aligned with this wall window.
        VMS_PLAT(g_cpu_window_start_ns)=j2me_cpu_clock::now_ns();
        (void)VMS_RS(g_graphics_profile).take_global();
        (void)VMS_RS(g_graphics_profile).take();
#ifdef __SWITCH__
        (void)VMS_PLAT(g_audio_sink).takeStats();
#endif
        VMS_PLAT(g_present_stats)={};
        (void)VMS_RS(g_frame_metrics).take();
        VMS_HEAP(g_frame_buffer_pool).reset_counters();
        VMS_EXEC(g_fast_ops)=VMS_EXEC(g_fast_blocks)=VMS_EXEC(g_slow_ops)=VMS_EXEC(g_fast_float241_ops)=0;
        VMS_RS(g_decoded_images).reset_counters();VMS_RS(g_png_fast)=VMS_RS(g_png_fallback)=VMS_RS(g_image_decode_us)=VMS_RS(g_image_copy_us)=0;
        VMS_NATIVE(g_library_indexes).reset_stats();VMS_NATIVE(g_library_leaf_calls)=0;
        VMS_EXEC(g_poll_calls)=VMS_EXEC(g_poll_gate_reads)=VMS_EXEC(g_poll_waits)=VMS_EXEC(g_poll_wait_us)=0;
        VMS_EXEC(g_speed_budget_window_frames)=VMS_EXEC(g_speed_group_timeouts)=0;
        VMS_EXEC(g_speed_slept_us)=VMS_EXEC(g_speed_cut_us)=VMS_EXEC(g_speed_add_us)=0; VMS_EXEC(g_pace_aligns)=0;
        g_frame_sleep_stats={};
        VMS_PLAT(g_audio_event_suppressed)=0;
        return;
    }
    s_last_emit_us=stats_now_us;
#ifdef __SWITCH__
    report_cpu_clock(false);
#endif
    p1_report();
    char schedule[640];
    std::snprintf(schedule,sizeof(schedule),"SCHEDSTAT sleep_wakes=%llu wake_late_avg_ms=%.3f wake_late_max_ms=%llu near_waits=%llu near_wait_ms_sum=%.3f near_wait_max_ms=%.3f wake_late_3ms_count=%llu deferred_wakes=%llu extended_waits=%llu budget_cap_min_ms=%d budget_cap_max_ms=%d recent_work_ms=%.3f recent_submit_ms=%.3f mode=budgeted-near-wake",
        (unsigned long long)wake.sleep_wakes,wake.sleep_wakes?double(wake.wake_late_ms)/wake.sleep_wakes:0.0,
        (unsigned long long)wake.wake_late_max_ms,(unsigned long long)wake.near_waits,wake.near_wait_us/1000.0,wake.near_wait_max_us/1000.0,
        (unsigned long long)wake.wake_late_3ms_count,(unsigned long long)wake.deferred_wakes,(unsigned long long)wake.extended_waits,
        wake.budget_cap_min_ms,wake.budget_cap_max_ms,VMS_EXEC(g_frame_wait_budget).recent_work_us()/1000.0,VMS_EXEC(g_frame_wait_budget).recent_submit_us()/1000.0);
    log_line(schedule);
    char work_line[1024];
    std::snprintf(work_line,sizeof(work_line),"WORKSTAT worker_gross_ms=%.3f worker_syncpaint_ms=%.3f worker_capwait_ms=%.3f worker_pollwait_ms=%.3f worker_nativewait_ms=%.3f worker_paint_blocked_ms=%.3f worker_remaining_wall_ms=%.3f gc_ms=%.3f worker_gc_ms=%.3f worker_paint_gc_ms=%.3f gc_count=%llu worker_gc_count=%llu worker_turns=%llu worker_yields=%llu worker_suspends=%llu early_repaint_handoffs=%llu scope=wall_time remaining_scope=wall_estimate_not_cpu",
        work.worker_gross_us/1000.0,work.worker_syncpaint_us/1000.0,work.worker_capwait_us/1000.0,
        work.worker_pollwait_us/1000.0,work.worker_nativewait_us/1000.0,work.worker_paint_blocked_us/1000.0,
        work.remaining_wall_us()/1000.0,work.gc_us/1000.0,work.worker_gc_us/1000.0,work.worker_paint_gc_us/1000.0,
        (unsigned long long)work.gc_count,(unsigned long long)work.worker_gc_count,(unsigned long long)work.worker_turns,
        (unsigned long long)work.worker_yields,(unsigned long long)work.worker_suspends,(unsigned long long)work.early_repaint_handoffs);
    log_line(work_line);
    report_optimization_window(stats.elapsed_us,work);
    const auto global=VMS_RS(g_graphics_profile).take_global();
    char global_line[640];
    std::snprintf(global_line,sizeof(global_line),"GFXGLOBAL native_calls=%llu offscreen_calls=%llu sampled_calls=%llu sampled_offscreen_calls=%llu raster_calls=%llu native_ms=%.3f offscreen_ms=%.3f raster_ms=%.3f scope=outer_native_calls sample_rate=1/64 raster_in_native=1 offscreen_in_native=1",
        (unsigned long long)global.native_calls,(unsigned long long)global.offscreen_calls,(unsigned long long)global.sampled_calls,
        (unsigned long long)global.sampled_offscreen_calls,(unsigned long long)global.raster_calls,
        global.native_ns/1000000.0,global.offscreen_ns/1000000.0,global.raster_ns/1000000.0);
    log_line(global_line);
    const auto graphics=VMS_RS(g_graphics_profile).take();
    char graphics_line[480];
    std::snprintf(graphics_line,sizeof(graphics_line),"GFXSTAT sampled_paints=%llu image_calls=%llu clip_calls=%llu other_calls=%llu raster_calls=%llu image_native_ms=%.3f clip_native_ms=%.3f other_native_ms=%.3f raster_ms=%.3f scope=sampled_paints raster_in_image_native=1",
        (unsigned long long)graphics.sampled_paints,(unsigned long long)graphics.image_calls,(unsigned long long)graphics.clip_calls,
        (unsigned long long)graphics.other_calls,(unsigned long long)graphics.raster_calls,
        graphics.image_native_ns/1000000.0,graphics.clip_native_ns/1000000.0,graphics.other_native_ns/1000000.0,graphics.raster_ns/1000000.0);
    log_line(graphics_line);
#ifdef __SWITCH__
    const auto audio=VMS_PLAT(g_audio_sink).takeStats();
    char sound[768];
    std::snprintf(sound,sizeof(sound),"AUDIOSTAT available=%d paused=%d loaded=%zu started=%zu blocks=%llu frames=%llu mix_ms_sum=%.3f mix_max_ms=%.3f nonzero_blocks=%llu peak=%u pumps=%llu pump_gap_max_ms=%.3f released=%llu queued=%zu queued_min=%zu queue_empty_risk=%llu errors=%llu events_suppressed=%llu",
        VMS_PLAT(g_audio_sink).available()?1:0,VMS_PLAT(g_audio).paused()?1:0,VMS_PLAT(g_audio).loadedPlayers(),VMS_PLAT(g_audio).activePlayers(),
        (unsigned long long)audio.render_blocks,(unsigned long long)audio.render_frames,audio.total_mix_us/1000.0,audio.max_mix_us/1000.0,
        (unsigned long long)audio.nonzero_blocks,audio.peak_abs_sample,(unsigned long long)audio.pump_count,audio.max_pump_gap_us/1000.0,
        (unsigned long long)audio.released_buffers,VMS_PLAT(g_audio_sink).queuedBuffers(),audio.min_queued_buffers,
        (unsigned long long)audio.queue_empty_polls,(unsigned long long)audio.errors,(unsigned long long)VMS_PLAT(g_audio_event_suppressed));
    log_line(sound);VMS_PLAT(g_audio_event_suppressed)=0;
    log_line("AUDIOPOLL schema=1 requests="+std::to_string(audio.throttled_calls)+
        " skipped_release_polls="+std::to_string(audio.skipped_release_polls)+" max_cache_us=1000 known_queue_guard=2");
#else
    VMS_PLAT(g_audio_event_suppressed)=0;
#endif
    char timings[256];
    char presentation[384];
    std::snprintf(presentation,sizeof(presentation), " renderer=%s presents=%llu present_avg_ms=%.3f present_max_ms=%.3f acquire_avg_ms=%.3f prepare_avg_ms=%.3f submit_avg_ms=%.3f gpu_wait_avg_ms=%.3f",
#ifdef J2ME_ENABLE_DEKO3D
        VMS_PLAT(g_gpu_active)?"gpu":"cpu",
#else
        "cpu",
#endif
        (unsigned long long)VMS_PLAT(g_present_stats).count,
        VMS_PLAT(g_present_stats).average_ms(VMS_PLAT(g_present_stats).total_us),VMS_PLAT(g_present_stats).max_us/1000.0,
        VMS_PLAT(g_present_stats).average_ms(VMS_PLAT(g_present_stats).acquire_us),VMS_PLAT(g_present_stats).average_ms(VMS_PLAT(g_present_stats).prepare_us),
        VMS_PLAT(g_present_stats).average_ms(VMS_PLAT(g_present_stats).submit_us),VMS_PLAT(g_present_stats).average_ms(VMS_PLAT(g_present_stats).gpu_wait_us));
    VMS_PLAT(g_present_stats)={};
    std::snprintf(timings, sizeof(timings), " window_ms=%.3f canvas_fps=%.2f paint_calls=%llu paint_fps=%.2f canvas_paint_avg_ms=%.3f canvas_paint_max_ms=%.3f worker_ms_sum=%.3f",
        stats.elapsed_us / 1000.0, stats.elapsed_us > 0 ? window_presents * 1000000.0 / stats.elapsed_us : 0.0,
        (unsigned long long)stats.canvas_paints, stats.canvas_fps(), stats.paint_avg_ms(), stats.paint_max_us / 1000.0, stats.worker_us / 1000.0);
    const double budget_fps=stats.elapsed_us>0?double(VMS_EXEC(g_speed_budget_window_frames))*1000000.0/stats.elapsed_us:0.0;
    const char* speed_result = "native";
    if (VMS_EXEC(g_speed_cap) == 30 || VMS_EXEC(g_speed_cap) == 60) {
        if (budget_fps + 3.0 >= VMS_EXEC(g_speed_cap)) speed_result = VMS_EXEC(g_game_clock).scale>1.01 || VMS_EXEC(g_speed_cut_us) > VMS_EXEC(g_speed_add_us) ? "boosted" : "limited";
        else if (stats.worker_us >= stats.elapsed_us * 7 / 10) speed_result = "work-or-wait";
        else speed_result = "short";
    }
    log_line("STAT slice=" + std::to_string(VMS_EXEC(g_slices)) + timings + presentation +
             " canvas_paints=" + std::to_string(stats.canvas_paints) +
             " canvas_paint_over_16.67ms=" + std::to_string(stats.over_60hz) +
             " canvas_paint_over_33.33ms=" + std::to_string(stats.over_30hz) +
             " gamecanvas_flushes=" + std::to_string(stats.gamecanvas_flushes) +
             " pace_aligns=" + std::to_string(VMS_EXEC(g_pace_aligns)) +
             " sleep_ms=" + std::to_string(next_thread_delay()) +
             " thread=" + std::string(VMS_EXEC(g_thread_live) ? "1" : "0") +
             " array_bytes=" + std::to_string(VMS_HEAP(g_array_payload_live)) + " array_peak=" + std::to_string(VMS_HEAP(g_array_payload_peak)) +
             " raw_draw_scope=draw_callbacks worker_scope=wall_time target_60fps=unverified"
             " fps_scope=" + std::string(speed_period_us()?"cap_budget_frames":"draw_callbacks") +
             " cap_budget_frames=" + std::to_string(VMS_EXEC(g_speed_budget_window_frames)) +
             " cap_budget_fps=" + std::to_string(budget_fps) +
             " group_timeouts=" + std::to_string(VMS_EXEC(g_speed_group_timeouts)) +
             " speed=" + std::to_string(VMS_EXEC(g_speed_cap)) +
             " speed_result=" + speed_result +
             " game_clock_scale=" + std::to_string(VMS_EXEC(g_game_clock).scale) +
             " speed_wait_ms=" + std::to_string(VMS_EXEC(g_speed_slept_us) / 1000) +
             " speed_cut_ms=" + std::to_string(VMS_EXEC(g_speed_cut_us) / 1000) +
             " speed_add_ms=" + std::to_string(VMS_EXEC(g_speed_add_us) / 1000));
    log_line("POLLSTAT calls="+std::to_string(VMS_EXEC(g_poll_calls))+" gate_reads="+std::to_string(VMS_EXEC(g_poll_gate_reads))+" waits="+std::to_string(VMS_EXEC(g_poll_waits))+" wait_ms="+std::to_string(VMS_EXEC(g_poll_wait_us)/1000));
    log_line("SLEEPSTAT short_1_7="+std::to_string(g_frame_sleep_stats.short_calls)+
        " long_over_120="+std::to_string(g_frame_sleep_stats.long_calls)+
        " paced_8_120="+std::to_string(g_frame_sleep_stats.owned)+
        " unowned_8_120="+std::to_string(g_frame_sleep_stats.unowned)+
        " timer_paced="+std::to_string(g_frame_sleep_stats.timer_paced)+" timer_unpaced="+std::to_string(g_frame_sleep_stats.timer_unpaced)+
        " timer_scaled="+std::to_string(g_frame_sleep_stats.timer_scaled)+
        " requested_ms="+std::to_string(g_frame_sleep_stats.requested_ms)+
        " effective_ms="+std::to_string(g_frame_sleep_stats.effective_ms)+
        " scope=thread_sleep_requests_not_actual_elapsed");
    g_frame_sleep_stats={};
    int64_t prep_lead_us=VMS_EXEC(g_main_thread).frame_prep.lead_us(speed_period_us());
    unsigned prep_ready=VMS_EXEC(g_main_thread).frame_prep.count>=4?1:0;
    for(const auto& th:VMS_EXEC(g_threads))if(th.state!=ThreadState::Terminated) {
        prep_lead_us=std::max(prep_lead_us,th.frame_prep.lead_us(speed_period_us()));
        prep_ready+=th.frame_prep.count>=4?1:0;
    }
    log_line("PREPSTAT ready_owners="+std::to_string(prep_ready)+" lead_max_ms="+std::to_string(prep_lead_us/1000.0)+" scope=owned_sleep_wake_to_draw_before_cap");
    log_line("LOGCOST schema=1 main_log_us="+std::to_string(j2melog::main_us_snapshot())+" drops="+std::to_string(j2melog::g_drops)+" shed="+std::to_string(j2melog::g_shed)+" bytes="+std::to_string(j2melog::g_bytes)+" sync="+std::string(j2melog::g_sync?"1":"0"));
    VMS_EXEC(g_poll_calls)=VMS_EXEC(g_poll_gate_reads)=VMS_EXEC(g_poll_waits)=VMS_EXEC(g_poll_wait_us)=0;
    VMS_EXEC(g_speed_budget_window_frames)=VMS_EXEC(g_speed_group_timeouts)=0;
    VMS_EXEC(g_speed_slept_us)=0; VMS_EXEC(g_speed_cut_us)=0; VMS_EXEC(g_speed_add_us)=0; VMS_EXEC(g_pace_aligns)=0;
}

#ifndef __SWITCH__
#include "deterministic_driver.inc"
#include "platform/desktop_host.inc"
#else
#include "platform/switch_host.inc"  // 0.2.107: no deterministic test driver on Switch
#endif

// P2.1 final: single storage path. VMStorage (migrated module storage) + VMState facade; the P2.1-c
// J2ME_MIGRATE_* A/B switches, their legacy declarations and the facade-off path were removed.
#include "vm_storage.h"
#include "vm_state.h"
#include "vm_access_impl.h"
