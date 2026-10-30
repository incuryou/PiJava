// vm_storage.h — P2.1-c batch 1: the ONE place where migrated module state is stored.
// Included at the end of j2mevm.cpp (after every module type is complete), before vm_state.h.
// P2.1 final: single storage path.
#pragma once
#include "render_surface.h"
namespace j2me_vmstate {
// c1-a — Diagnostics module (inventory module "Other"); initial values copied 1:1 from the old declarations.
struct DiagnosticsStorage {
    std::unordered_map<int, std::string> g_exception_traces;  // was src/exception_trace.h:3
    bool g_log_draw = true;                                     // was src/j2mevm.cpp:65
    P1M3GEpoch g_p1_m3g_base;                                   // was src/j2mevm.cpp:1713
    CityDiagnostics g_city_diag;                                // was src/p1_city_diagnostics.inc:14
    p1::Profile g_p1_profile;                                   // was src/p1_diagnostics.inc:4
    P1Diagnostics g_p1;                                         // was src/p1_diagnostics.inc:25
};
}  // namespace j2me_vmstate
namespace j2me_vmstate {
// c1-b — NativeRegistry module: the 4 mutable items only (read-only constant tables stay where they are).
struct NativeRegistryStorage {
    jit236::Engine g_native_jit;                       // was src/j2mevm.cpp:82
    j2me_runtime::LibraryIndex g_library_indexes;      // was src/runtime/library_intrinsics.inc:5
    uint64_t g_index_key_epoch = 0;                    // was src/runtime/library_intrinsics.inc:6
    uint64_t g_library_leaf_calls = 0;                 // was src/runtime/library_intrinsics.inc:6
};
}  // namespace j2me_vmstate
namespace j2me_vmstate {
// c2 — ClassLoader/Resolver module. Class-init state (Class::init_state/init_owner/init_error) lives inside
// the Class objects of g_classes, so it moves with g_classes and keeps exactly one owner.
struct ClassLoaderStorage {
    std::unordered_map<std::string, std::vector<uint8_t>> g_jar_files;  // was src/j2mevm.cpp:157
    std::unordered_map<std::string, std::string> g_props;               // was src/j2mevm.cpp:158
    std::deque<Class> g_classes;                                        // was src/j2mevm.cpp:179
    std::unordered_map<std::string, int> g_class_index;                 // was src/j2mevm.cpp:182
    std::vector<std::string> g_classpath;                               // was src/j2mevm.cpp:183
    std::unordered_map<std::string, int> g_class_objects;               // was src/j2mevm.cpp:219
    LiteralCache g_literal_cache;
};
}  // namespace j2me_vmstate
namespace j2me_vmstate {
// c3 — Heap/RootScope module (13 mutable items); initial values copied 1:1.
struct HeapStorage {
    int g_alloc_since = 0;                                   // was src/j2mevm.cpp:105
    int g_gc_logs = 0;                                       // was src/j2mevm.cpp:105
    std::vector<char> g_gc_mark_scratch;                     // was src/j2mevm.cpp:115
    std::vector<int> g_gc_work_scratch;                      // was src/j2mevm.cpp:116
    uint64_t g_array_alloc_since_gc = 0;                     // was src/j2mevm.cpp:152
    uint64_t g_array_payload_live = 0;                       // was src/j2mevm.cpp:152
    uint64_t g_array_payload_peak = 0;                       // was src/j2mevm.cpp:152
    uint64_t g_gc_array_refs_scanned = 0;                    // was src/j2mevm.cpp:153
    uint64_t g_gc_cycles = 0;                                // was src/j2mevm.cpp:153
    std::deque<Obj> g_heap = std::deque<Obj>(1);             // was src/j2mevm.cpp:180 (slot 0 = null)
    std::vector<int> g_free;                                 // was src/j2mevm.cpp:181
    std::vector<std::pair<std::vector<Frame>*, JavaThreadContext*>> g_root_stacks;  // was src/j2mevm.cpp:220
    FrameBufferPool<Value> g_frame_buffer_pool;              // was src/runtime/object_model.inc:59
    PrimitiveBufferPool g_primitive_buffers; // bounded dead primitive bytes; never roots or snapshot state
};
}  // namespace j2me_vmstate
namespace j2me_vmstate {
struct DisplayStorage {  // c6 Display sub-module: current/shown Displayable + Alert timing (J2S5 fields)
    int g_shown = 0;  // was src/j2mevm.cpp
    int g_alert_return = 0;  // was src/j2mevm.cpp
    int64_t g_alert_deadline = 0;  // was src/j2mevm.cpp
    int64_t g_alert_remaining = -1;  // was src/j2mevm.cpp
    int g_current = 0;  // was src/j2mevm.cpp
    int g_display = 0;  // was src/j2mevm.cpp
};
struct RenderStorage {  // c6 RenderSurface: 80 inventory items (74 here + 6 in display) + draw_end::prev (inventory said init-only)
    int g_screen_w = 240;  // was src/j2mevm.cpp
    int g_screen_h = 320;  // was src/j2mevm.cpp
    std::vector<uint8_t> g_fb;  // was src/j2mevm.cpp
    CanvasPaintStats g_paint_stats;  // was src/j2mevm.cpp
    GraphicsProfile g_graphics_profile;  // was src/j2mevm.cpp
    FrameMetrics g_frame_metrics;  // was src/j2mevm.cpp
    DecodedImageCache g_decoded_images;  // was src/j2mevm.cpp
    uint64_t g_png_fast = 0;  // was src/j2mevm.cpp
    uint64_t g_png_fallback = 0;  // was src/j2mevm.cpp
    uint64_t g_image_decode_us = 0;  // was src/j2mevm.cpp
    uint64_t g_image_copy_us = 0;  // was src/j2mevm.cpp
    std::unordered_map<int, int> g_offscreen;  // was src/j2mevm.cpp
    std::unordered_map<int, int> g_gfx_color;  // was src/j2mevm.cpp
    std::unordered_map<int, int> g_gfx_target;  // was src/j2mevm.cpp
    std::unordered_map<int, int> g_gfx_tx;  // was src/j2mevm.cpp
    std::unordered_map<int, int> g_gfx_ty;  // was src/j2mevm.cpp
    std::unordered_map<int, Clip> g_clip;  // was src/j2mevm.cpp
    int g_paint_depth = 0;  // was src/j2mevm.cpp
    DrawAcc g_drawacc;  // was src/j2mevm.cpp
    std::vector<CjkGlyph> g_cjk;  // was src/j2mevm.cpp
    bool g_cjk_tried = false;  // was src/j2mevm.cpp
    bool g_repaint = false;  // was src/j2mevm.cpp
    bool g_fb_dirty = false;  // was src/j2mevm.cpp
    int g_paint_serial = 0;  // was src/j2mevm.cpp
    int g_game_frames = 0;  // was src/j2mevm.cpp
    uint64_t g_present_frames = 0;  // 0.2.112 F4: in-game present() executions (frames delivered to the screen)
    uint32_t g_frame_sig = 2166136261u;  // was src/j2mevm.cpp
    std::string draw_end_prev;  // was src/j2mevm.cpp
    uint64_t g_m3g_native_backgrounds = 0;  // was src/m3g_background.h
    uint64_t g_m3g_native_background_pixels = 0;  // was src/m3g_background.h
    uint64_t g_m3g_native_sorts = 0;  // was src/m3g_background.h
    uint64_t g_m3g_native_vectors = 0;  // was src/m3g_geometry.h
    uint64_t g_m3g_native_vertices = 0;  // was src/m3g_geometry.h
    uint64_t g_m3g_native_matrices = 0;  // was src/m3g_geometry.h
    bool g_m3g_prep241_enabled = true;  // was src/m3g_image241.h
    bool g_m3g_mesh242_enabled = true;  // was src/m3g_mesh242.h
    bool g_host_boundary242_enabled = true;  // was src/m3g_mesh242.h
    uint64_t g_m3g_native_triangles = 0;  // was src/m3g_native.h
    uint64_t g_m3g_native_pixels = 0;  // was src/m3g_native.h
    uint64_t g_m3g_fallback_aa = 0;  // was src/m3g_native.h
    uint64_t g_m3g_fallback_dither = 0;  // was src/m3g_native.h
    uint64_t g_m3g_fallback_fog = 0;  // was src/m3g_native.h
    uint64_t g_m3g_fallback_mip = 0;  // was src/m3g_native.h
    bool g_m3g_row240_enabled = J2ME_OPT_M3G_ROW240 != 0;  // was src/m3g_row240.h
    bool g_m3g_span239_enabled = J2ME_OPT_M3G_SPAN239 != 0;  // was src/m3g_span239.h
    bool g_m3g_sprite241_enabled = true;  // was src/m3g_sprite241.h
    bool g_m3g_texture238_enabled = J2ME_OPT_M3G_TEXTURE238 != 0;  // was src/m3g_texture238.h
    uint64_t g_m3g_texture238_calls = 0;  // was src/m3g_texture238.h
    uint64_t g_m3g_texture238_units = 0;  // was src/m3g_texture238.h
    uint64_t g_m3g_texture238_mips = 0;  // was src/m3g_texture238.h
    uint64_t g_m3g_texture238_declined = 0;  // was src/m3g_texture238.h
    uint64_t m3g_assembly242_gathered = 0;  // was src/m3g_assembly242.h (m3g_assembly242::gathered)
    uint64_t m3g_assembly242_culled = 0;  // was src/m3g_assembly242.h (m3g_assembly242::culled)
    uint64_t m3g_assembly242_emitted = 0;  // was src/m3g_assembly242.h (m3g_assembly242::emitted)
    uint64_t m3g_assembly242_declines = 0;  // was src/m3g_assembly242.h (m3g_assembly242::declines)
    m3g_image241::Stats m3g_image241_stats;  // was src/m3g_image241.h (m3g_image241::stats)
    uint64_t m3g_lighting242_calls = 0;  // was src/m3g_lighting242.h (m3g_lighting242::calls)
    uint64_t m3g_lighting242_vertices = 0;  // was src/m3g_lighting242.h (m3g_lighting242::vertices)
    uint64_t m3g_lighting242_light_visits = 0;  // was src/m3g_lighting242.h (m3g_lighting242::light_visits)
    uint64_t m3g_lighting242_declines = 0;  // was src/m3g_lighting242.h (m3g_lighting242::declines)
    m3g_mesh242::Stats m3g_mesh242_stats;  // was src/m3g_mesh242.h (m3g_mesh242::stats)
    uint64_t m3g_mesh242_composite_calls = 0;  // was src/m3g_mesh242.h (m3g_mesh242::composite_calls)
    uint64_t m3g_mesh242_transform_to_calls = 0;  // was src/m3g_mesh242.h (m3g_mesh242::transform_to_calls)
    uint64_t m3g_mesh242_transform_to_nodes = 0;  // was src/m3g_mesh242.h (m3g_mesh242::transform_to_nodes)
    m3g_row240::Stats m3g_row240_stats;  // was src/m3g_row240.h (m3g_row240::stats)
    m3g_span239::Stats m3g_span239_stats;  // was src/m3g_span239.h (m3g_span239::stats)
    m3g_sprite241::Stats m3g_sprite241_stats;  // was src/m3g_sprite241.h (m3g_sprite241::stats)
    paint_hotspots241::Snapshot paint_hotspots241_current;  // was src/paint_hotspots241.h (paint_hotspots241::current)
    paint_hotspots241::Snapshot paint_hotspots241_pending;  // was src/paint_hotspots241.h (paint_hotspots241::pending)
    bool paint_hotspots241_active = false;  // was src/paint_hotspots241.h (paint_hotspots241::active)
    bool paint_hotspots241_have_pending = false;  // was src/paint_hotspots241.h (paint_hotspots241::have_pending)
    uint64_t paint_hotspots241_paints = 0;  // was src/paint_hotspots241.h (paint_hotspots241::paints)
    uint64_t paint_hotspots241_slow_paints = 0;  // was src/paint_hotspots241.h (paint_hotspots241::slow_paints)
    uint64_t paint_hotspots241_total_samples = 0;  // was src/paint_hotspots241.h (paint_hotspots241::total_samples)
    uint64_t paint_hotspots241_omitted_slow = 0;  // was src/paint_hotspots241.h (paint_hotspots241::omitted_slow)
    int64_t paint_hotspots241_longest_us = 0;  // was src/paint_hotspots241.h (paint_hotspots241::longest_us)
    DisplayStorage display;
    j2me_render::SurfaceStateMachine surface;  // C09: CPU_VALID only (GPU states: P5)
};
// c5 — PlatformHost module: 57 inventory items (49 file-scope + 8 function-local) + main::in_game/main::last_ks
// (function-local, missing from the P2.1-a inventory). kVer, g_host_skin, main::items, audio_ffmpeg236 api/counters
// are init-only and stay where they are. HARD RULE: this struct lives inside the never-destroyed VMStorageCell, so
// g_audio_sink's audout buffers and g_gpu_present/g_gpu_layers have ONE address for the whole run (see the
// static_asserts after g_vm_storage below).
struct PlatformStorage {
    std::ofstream g_log;  // was src/j2mevm.cpp
    int64_t g_boot_ms = 0;  // was src/j2mevm.cpp
    bool g_host_frozen236 = false;  // was src/j2mevm.cpp
    int64_t g_host_freeze_started236 = 0;  // was src/j2mevm.cpp
    std::unordered_set<int> g_pressed_keys;  // was src/j2mevm.cpp
    std::unordered_set<int> g_command_keys;  // raw host presses consumed by a Canvas command/menu
    j2me_audio::Engine g_audio;  // was src/j2mevm.cpp
    int64_t g_audio_event_window = 0;  // was src/j2mevm.cpp
    unsigned g_audio_event_count = 0;  // was src/j2mevm.cpp
    uint64_t g_audio_event_suppressed = 0;  // was src/j2mevm.cpp
    std::unordered_set<int> g_tones;  // was src/j2mevm.cpp
#ifdef __SWITCH__
    j2me_audio::SwitchAudioSink g_audio_sink;  // was src/j2mevm.cpp
#endif
    int64_t g_cpu_window_start_ns = -1;  // was src/j2mevm.cpp
    PresentStats g_present_stats;  // was src/j2mevm.cpp
    std::string g_rms_suite;  // was src/j2mevm.cpp
    bool g_rms_import_legacy = false;  // was src/j2mevm.cpp
#ifdef __SWITCH__
    std::string g_launched_path;  // was src/instant_state.inc
#endif
#if defined(__SWITCH__) && defined(J2ME_ENABLE_DEKO3D)
    SwitchGpuPresent g_gpu_present;  // was src/platform/switch_host.inc
#endif
#if defined(__SWITCH__) && defined(J2ME_ENABLE_DEKO3D)
    GpuHostLayerCache g_gpu_layers;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    Framebuffer g_win;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    bool g_win_ready = false;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    bool g_gpu_active = false;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    int g_cpu_cleared = 0;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    int g_chrome_bezel = -2;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    int g_chrome_fps = -2;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    u32* g_cpu_frame = nullptr;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    u32 g_cpu_stride = 0;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    int64_t g_cpu_acquire_us = 0;  // was src/platform/switch_host.inc
#endif
    int64_t g_last_present_us = 0;  // was src/platform/switch_host.inc; P2.6: shared scheduler core (both platforms)
#ifdef __SWITCH__
    HostSurface g_host_surface = HostSurface::Menu;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    bool g_exit_sheet = false;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    bool g_sheet_paused_here = false;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    u64 g_sheet_swallow = 0;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    std::string g_exit_sheet_note;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    host_menu::Settings g_menu_settings;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    host_menu::Focus g_menu_focus = host_menu::Focus::Games;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    host_menu::Focus g_menu_option_focus = host_menu::Focus::Bezel;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    std::vector<std::string> g_menu_labels;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    std::vector<std::string> g_menu_paths;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    std::vector<uint8_t> g_menu_saved;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    std::map<std::string, int> g_game_speeds;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    int g_menu_selected = 0;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    bool g_fps_reset = true;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    host_font::Font g_host_font;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    bool g_host_font_service = false;  // was src/platform/switch_host.inc
#endif
    int g_key_states = 0;  // was src/j2mevm.cpp
    bool g_host_paused = false;  // was src/j2mevm.cpp
    int g_log_pending = 0;  // was src/j2mevm.cpp
#ifdef __SWITCH__
    host_skin::Cache g_host_skin;  // c6: moved in (c5 deviation 2 ruling; inventory said init-only but select() mutates it)
    std::string g_menu_status;  // was src/platform/switch_host.inc
#endif
#ifdef __SWITCH__
    std::string g_import_label;  // was src/platform/switch_host.inc
#endif
    // platform-owned function-local statics (initial values as in the old `static` lines)
    int64_t audio_pump_previous = 0;
#ifdef __SWITCH__
    int64_t report_cpu_clock_last_ms = 0;
#endif
#ifdef __SWITCH__
    int present_fps_shown = 0;
#endif
#ifdef __SWITCH__
    uint64_t present_fps_seen = 0;
#endif
#ifdef __SWITCH__
    int64_t present_fps_t0 = 0;
#endif
#ifdef __SWITCH__
    int main_sel = 0;
#endif
#ifdef __SWITCH__
    int main_in_game = 0;
#endif
#ifdef __SWITCH__
    int main_last_ks = -1;
#endif
#ifdef __SWITCH__
    bool main_import_prompt = false;
#endif
#ifdef __SWITCH__
    int64_t cpu_clock_info_type = -1;
#endif
};
struct ExecStorage {  // c4: 66 inventory fields (54 here + 12 in ctx) + new g_save_clock; g_die_jmp/g_die_catch stay plain globals
    CompletedPaintBudget241 g_completed_paint241;
    uint64_t g_fast_blocks = 0;
    uint64_t g_fast_ops = 0;
    uint64_t g_slow_ops = 0;
    bool g_fast_float241_enabled = true;
    uint64_t g_fast_float241_ops = 0;
    int g_midlet = 0;
    MidletState g_midlet_state = MidletState::Empty;
    bool g_midlet_resume = false;
    bool g_notify_destroyed = false;
    std::string g_last_console_line;  // 0.2.112 F2: last non-empty System.out/err line of this launch
    bool g_self_exit = false;         // 0.2.112 F2: MIDlet ended itself (notifyDestroyed/System.exit/failed start)
    bool g_notify_paused = false;
    bool g_lifecycle_busy = false;
    std::deque<int> g_serial_calls;
    WakeStats g_wake_stats;
    uint64_t g_pace_aligns = 0;
    int g_slice_ms = 4;
    FrameWaitBudget g_frame_wait_budget;
    int64_t g_frame_work_finished_us = 0;
    int64_t g_frame_work_us = 0;
    int64_t g_last_slice_us = 0;
    bool g_thread_live = false;
    int g_cuts = 0;
    int g_slices = 0;
    RuntimeProfile g_runtime_profile;
    ExecutionProfile g_execution_profile;
    int g_speed_cap = 0;
    uint64_t g_poll_calls = 0;
    uint64_t g_poll_gate_reads = 0;
    uint64_t g_poll_wait_us = 0;
    uint64_t g_poll_waits = 0;
    GameClock g_game_clock;
    int g_game_clock_mode = -1;
    int64_t g_clock_input_hold_us = 0;
    bool g_game_clock_inherited = false;
    int64_t g_speed_last_paint_us = 0;
    int64_t g_speed_next_us = 0;
    int64_t g_speed_slept_us = 0;
    int64_t g_speed_cut_us = 0;
    int64_t g_speed_add_us = 0;
    bool g_speed_frame_open = false;
    bool g_speed_serial_serviced = false;
    std::string g_error;
    JavaThreadContext g_main_thread;
    uint64_t g_speed_budget_frames = 0;
    uint64_t g_speed_budget_window_frames = 0;
    uint64_t g_speed_group_timeouts = 0;
    JavaThreadContext* g_speed_budget_owner = nullptr;
    std::list<JavaThreadContext> g_threads;
    std::unordered_map<int, Monitor> g_monitors;
    bool g_can_suspend = false;
    bool g_resume_request = false;
    size_t g_round_robin = 0;
    FarmStartup g_farm_startup;
    uint64_t g_flag_spin_pumps = 0;
    SaveClockAnchor g_save_clock;  // c4 new: J2S5 logical clock (not in the P2.1-a inventory)
    ExecutionContext ctx;
};
struct VMStorage {
    DiagnosticsStorage diag;
    NativeRegistryStorage natives;
    ClassLoaderStorage classes;
    HeapStorage heap;
    ExecStorage exec;
    PlatformStorage platform;
    RenderStorage render;
};
// Single static-storage object (zero-initialised, then default member initialisers — same as the
// old namespace-scope globals). There is no second copy of any migrated variable.
// (c6) Render: g_fb is handed to the GPU presenter (game_rgba -> present layers); its storage keeps one address.
// (c5) The platform module also needs this: g_audio_sink hands audout the addresses of its page buffers and
// g_gpu_present/g_gpu_layers hold deko3d memory/fence state; they must never move or be destroyed while
// another global destructor could still run. Normal exit already closes them explicitly in main(); the
// atexit hook below repeats those idempotent closes (in the old reverse-declaration order) for exit paths
// that used to rely on the destructors, and flushes g_log, which the old ~ofstream did.
// The heap module owns g_frame_buffer_pool, which ~Frame() releases into. Frames also live in globals defined
// EARLIER in the translation unit (g_threads, g_main_thread), which are destroyed LATER at exit. The old pool was
// defined before them and therefore outlived them; to keep that guarantee the storage is never destroyed.
static void platform_atexit_close() noexcept;
union VMStorageCell { VMStorage value; VMStorageCell() : value() { std::atexit(platform_atexit_close); } ~VMStorageCell() {} };
static VMStorageCell g_vm_storage_cell;
static constexpr VMStorage& g_vm_storage = g_vm_storage_cell.value;
// Address-stability proof (compile time): each pointer below is an *address constant expression*, which
// only exists for an object of static storage duration at a fixed address. Together with the never-destroyed
// cell (no destructor runs, no reallocation is possible for a non-pointer member) the address is constant
// for the whole run. Runtime twin: platform_address_check() (logged once at audio/GPU init and verified at exit).
static constexpr VMStorage* kVmStorageAddr = &g_vm_storage_cell.value;
static_assert(kVmStorageAddr != nullptr, "VMStorage must be a static-storage object");

#ifdef __SWITCH__
static constexpr j2me_audio::SwitchAudioSink* kAudioSinkAddr = &g_vm_storage_cell.value.platform.g_audio_sink;
static_assert(kAudioSinkAddr != nullptr, "g_audio_sink must have a fixed static address");
static_assert(!std::is_copy_assignable<j2me_audio::SwitchAudioSink>::value && !std::is_move_assignable<j2me_audio::SwitchAudioSink>::value,
              "g_audio_sink must not be reassigned (its audout buffers would move)");
#ifdef J2ME_ENABLE_DEKO3D
static constexpr SwitchGpuPresent* kGpuPresentAddr = &g_vm_storage_cell.value.platform.g_gpu_present;
static constexpr GpuHostLayerCache* kGpuLayersAddr = &g_vm_storage_cell.value.platform.g_gpu_layers;
static_assert(kGpuPresentAddr != nullptr && kGpuLayersAddr != nullptr, "GPU presenter state must have fixed static addresses");
static_assert(!std::is_copy_assignable<SwitchGpuPresent>::value, "g_gpu_present must not be reassigned");
#endif
#endif
static void platform_atexit_close() noexcept {
    auto& p = g_vm_storage_cell.value.platform;
#if defined(__SWITCH__) && defined(J2ME_ENABLE_DEKO3D)
    p.g_gpu_present.close();          // was ~SwitchGpuPresent (declared after g_audio_sink -> destroyed first)
#endif
#ifdef __SWITCH__
    (void)p.g_audio_sink.shutdown();  // was ~SwitchAudioSink
#endif
    if (p.g_log.is_open()) p.g_log.flush();  // was ~ofstream (flush; the OS closes the descriptor)
}
}  // namespace j2me_vmstate
