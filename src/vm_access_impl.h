// vm_access_impl.h — P2.1-c batch 1: definitions of the accessors declared at the old declaration
// sites (VMS_*_ACCESSOR). Each accessor goes through vm_state() (the P2.1-b facade) when the facade
// is on, otherwise straight to the VMStorage object.
#pragma once
#define J2ME_VMS_ROUTE(m, n) (::j2me_vmstate::vm_state().m.n)
namespace j2me_vmstate { namespace acc {
static inline CompletedPaintBudget241& exec_g_completed_paint241() noexcept { return J2ME_VMS_ROUTE(exec, g_completed_paint241); }
static inline uint64_t& exec_g_fast_blocks() noexcept { return J2ME_VMS_ROUTE(exec, g_fast_blocks); }
static inline uint64_t& exec_g_fast_ops() noexcept { return J2ME_VMS_ROUTE(exec, g_fast_ops); }
static inline uint64_t& exec_g_slow_ops() noexcept { return J2ME_VMS_ROUTE(exec, g_slow_ops); }
static inline bool& exec_g_fast_float241_enabled() noexcept { return J2ME_VMS_ROUTE(exec, g_fast_float241_enabled); }
static inline uint64_t& exec_g_fast_float241_ops() noexcept { return J2ME_VMS_ROUTE(exec, g_fast_float241_ops); }
static inline int& exec_g_midlet() noexcept { return J2ME_VMS_ROUTE(exec, g_midlet); }
static inline MidletState& exec_g_midlet_state() noexcept { return J2ME_VMS_ROUTE(exec, g_midlet_state); }
static inline bool& exec_g_midlet_resume() noexcept { return J2ME_VMS_ROUTE(exec, g_midlet_resume); }
static inline bool& exec_g_notify_destroyed() noexcept { return J2ME_VMS_ROUTE(exec, g_notify_destroyed); }
static inline std::string& exec_g_last_console_line() noexcept { return J2ME_VMS_ROUTE(exec, g_last_console_line); }
static inline bool& exec_g_self_exit() noexcept { return J2ME_VMS_ROUTE(exec, g_self_exit); }
static inline bool& exec_g_notify_paused() noexcept { return J2ME_VMS_ROUTE(exec, g_notify_paused); }
static inline bool& exec_g_lifecycle_busy() noexcept { return J2ME_VMS_ROUTE(exec, g_lifecycle_busy); }
static inline std::deque<int>& exec_g_serial_calls() noexcept { return J2ME_VMS_ROUTE(exec, g_serial_calls); }
static inline WakeStats& exec_g_wake_stats() noexcept { return J2ME_VMS_ROUTE(exec, g_wake_stats); }
static inline uint64_t& exec_g_pace_aligns() noexcept { return J2ME_VMS_ROUTE(exec, g_pace_aligns); }
static inline int& exec_g_slice_ms() noexcept { return J2ME_VMS_ROUTE(exec, g_slice_ms); }
static inline FrameWaitBudget& exec_g_frame_wait_budget() noexcept { return J2ME_VMS_ROUTE(exec, g_frame_wait_budget); }
static inline int64_t& exec_g_frame_work_finished_us() noexcept { return J2ME_VMS_ROUTE(exec, g_frame_work_finished_us); }
static inline int64_t& exec_g_frame_work_us() noexcept { return J2ME_VMS_ROUTE(exec, g_frame_work_us); }
static inline int64_t& exec_g_last_slice_us() noexcept { return J2ME_VMS_ROUTE(exec, g_last_slice_us); }
static inline bool& exec_g_thread_live() noexcept { return J2ME_VMS_ROUTE(exec, g_thread_live); }
static inline bool& exec_g_thrown() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_thrown); }
static inline bool& exec_g_yield() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_yield); }
static inline int& exec_g_cuts() noexcept { return J2ME_VMS_ROUTE(exec, g_cuts); }
static inline int& exec_g_run_depth() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_run_depth); }
static inline int& exec_g_slices() noexcept { return J2ME_VMS_ROUTE(exec, g_slices); }
static inline RuntimeProfile& exec_g_runtime_profile() noexcept { return J2ME_VMS_ROUTE(exec, g_runtime_profile); }
static inline ExecutionProfile& exec_g_execution_profile() noexcept { return J2ME_VMS_ROUTE(exec, g_execution_profile); }
static inline int& exec_g_speed_cap() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_cap); }
static inline uint64_t& exec_g_poll_calls() noexcept { return J2ME_VMS_ROUTE(exec, g_poll_calls); }
static inline uint64_t& exec_g_poll_gate_reads() noexcept { return J2ME_VMS_ROUTE(exec, g_poll_gate_reads); }
static inline uint64_t& exec_g_poll_wait_us() noexcept { return J2ME_VMS_ROUTE(exec, g_poll_wait_us); }
static inline uint64_t& exec_g_poll_waits() noexcept { return J2ME_VMS_ROUTE(exec, g_poll_waits); }
static inline GameClock& exec_g_game_clock() noexcept { return J2ME_VMS_ROUTE(exec, g_game_clock); }
static inline int& exec_g_game_clock_mode() noexcept { return J2ME_VMS_ROUTE(exec, g_game_clock_mode); }
static inline int64_t& exec_g_clock_input_hold_us() noexcept { return J2ME_VMS_ROUTE(exec, g_clock_input_hold_us); }
static inline bool& exec_g_game_clock_inherited() noexcept { return J2ME_VMS_ROUTE(exec, g_game_clock_inherited); }
static inline int64_t& exec_g_speed_last_paint_us() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_last_paint_us); }
static inline int64_t& exec_g_speed_next_us() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_next_us); }
static inline int64_t& exec_g_speed_slept_us() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_slept_us); }
static inline int64_t& exec_g_speed_cut_us() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_cut_us); }
static inline int64_t& exec_g_speed_add_us() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_add_us); }
static inline int& exec_g_repaint_owner_ref() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_repaint_owner_ref); }
static inline int& exec_g_repaint_owner_yields() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_repaint_owner_yields); }
static inline int& exec_g_repaint_yield_canvas() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_repaint_yield_canvas); }
static inline bool& exec_g_repaint_yield_handoff() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_repaint_yield_handoff); }
static inline bool& exec_g_speed_frame_open() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_frame_open); }
static inline bool& exec_g_speed_serial_serviced() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_serial_serviced); }
static inline std::string& exec_g_error() noexcept { return J2ME_VMS_ROUTE(exec, g_error); }
static inline JavaThreadContext& exec_g_main_thread() noexcept { return J2ME_VMS_ROUTE(exec, g_main_thread); }
static inline uint64_t& exec_g_speed_budget_frames() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_budget_frames); }
static inline uint64_t& exec_g_speed_budget_window_frames() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_budget_window_frames); }
static inline uint64_t& exec_g_speed_group_timeouts() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_group_timeouts); }
static inline JavaThreadContext*& exec_g_speed_budget_owner() noexcept { return J2ME_VMS_ROUTE(exec, g_speed_budget_owner); }
static inline JavaThreadContext*& exec_g_running_thread() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_running_thread); }
static inline std::list<JavaThreadContext>& exec_g_threads() noexcept { return J2ME_VMS_ROUTE(exec, g_threads); }
static inline std::unordered_map<int, Monitor>& exec_g_monitors() noexcept { return J2ME_VMS_ROUTE(exec, g_monitors); }
static inline bool& exec_g_can_suspend() noexcept { return J2ME_VMS_ROUTE(exec, g_can_suspend); }
static inline bool& exec_g_resume_request() noexcept { return J2ME_VMS_ROUTE(exec, g_resume_request); }
static inline size_t& exec_g_round_robin() noexcept { return J2ME_VMS_ROUTE(exec, g_round_robin); }
static inline int& exec_g_pump_depth() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_pump_depth); }
static inline FarmStartup& exec_g_farm_startup() noexcept { return J2ME_VMS_ROUTE(exec, g_farm_startup); }
static inline uint64_t& exec_g_flag_spin_pumps() noexcept { return J2ME_VMS_ROUTE(exec, g_flag_spin_pumps); }
static inline std::vector<Frame>*& exec_g_active_stack() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_active_stack); }
static inline Method*& exec_g_repaint_yield_method() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_repaint_yield_method); }
static inline int& exec_g_repaint_yield_pc() noexcept { return J2ME_VMS_ROUTE(exec, ctx.g_repaint_yield_pc); }
static inline SaveClockAnchor& exec_g_save_clock() noexcept { return J2ME_VMS_ROUTE(exec, g_save_clock); }
static inline ExecutionContext& exec_g_exec_ctx() noexcept { return J2ME_VMS_ROUTE(exec, ctx); }
static inline std::unordered_map<int, std::string>& diag_g_exception_traces() noexcept { return J2ME_VMS_ROUTE(diag, g_exception_traces); }
static inline bool& diag_g_log_draw() noexcept { return J2ME_VMS_ROUTE(diag, g_log_draw); }
static inline P1M3GEpoch& diag_g_p1_m3g_base() noexcept { return J2ME_VMS_ROUTE(diag, g_p1_m3g_base); }
static inline CityDiagnostics& diag_g_city_diag() noexcept { return J2ME_VMS_ROUTE(diag, g_city_diag); }
static inline p1::Profile& diag_g_p1_profile() noexcept { return J2ME_VMS_ROUTE(diag, g_p1_profile); }
static inline P1Diagnostics& diag_g_p1() noexcept { return J2ME_VMS_ROUTE(diag, g_p1); }
static inline jit236::Engine& natives_g_native_jit() noexcept { return J2ME_VMS_ROUTE(natives, g_native_jit); }
static inline j2me_runtime::LibraryIndex& natives_g_library_indexes() noexcept { return J2ME_VMS_ROUTE(natives, g_library_indexes); }
static inline uint64_t& natives_g_index_key_epoch() noexcept { return J2ME_VMS_ROUTE(natives, g_index_key_epoch); }
static inline uint64_t& natives_g_library_leaf_calls() noexcept { return J2ME_VMS_ROUTE(natives, g_library_leaf_calls); }
static inline std::unordered_map<std::string, std::vector<uint8_t>>& classes_g_jar_files() noexcept { return J2ME_VMS_ROUTE(classes, g_jar_files); }
static inline std::unordered_map<std::string, std::string>& classes_g_props() noexcept { return J2ME_VMS_ROUTE(classes, g_props); }
static inline std::deque<Class>& classes_g_classes() noexcept { return J2ME_VMS_ROUTE(classes, g_classes); }
static inline std::unordered_map<std::string, int>& classes_g_class_index() noexcept { return J2ME_VMS_ROUTE(classes, g_class_index); }
static inline std::vector<std::string>& classes_g_classpath() noexcept { return J2ME_VMS_ROUTE(classes, g_classpath); }
static inline std::unordered_map<std::string, int>& classes_g_class_objects() noexcept { return J2ME_VMS_ROUTE(classes, g_class_objects); }
static inline LiteralCache& classes_g_literal_cache() noexcept { return J2ME_VMS_ROUTE(classes, g_literal_cache); }
static inline int& heap_g_alloc_since() noexcept { return J2ME_VMS_ROUTE(heap, g_alloc_since); }
static inline int& heap_g_gc_logs() noexcept { return J2ME_VMS_ROUTE(heap, g_gc_logs); }
static inline std::vector<char>& heap_g_gc_mark_scratch() noexcept { return J2ME_VMS_ROUTE(heap, g_gc_mark_scratch); }
static inline std::vector<int>& heap_g_gc_work_scratch() noexcept { return J2ME_VMS_ROUTE(heap, g_gc_work_scratch); }
static inline uint64_t& heap_g_array_alloc_since_gc() noexcept { return J2ME_VMS_ROUTE(heap, g_array_alloc_since_gc); }
static inline uint64_t& heap_g_array_payload_live() noexcept { return J2ME_VMS_ROUTE(heap, g_array_payload_live); }
static inline uint64_t& heap_g_array_payload_peak() noexcept { return J2ME_VMS_ROUTE(heap, g_array_payload_peak); }
static inline uint64_t& heap_g_gc_array_refs_scanned() noexcept { return J2ME_VMS_ROUTE(heap, g_gc_array_refs_scanned); }
static inline uint64_t& heap_g_gc_cycles() noexcept { return J2ME_VMS_ROUTE(heap, g_gc_cycles); }
static inline std::deque<Obj>& heap_g_heap() noexcept { return J2ME_VMS_ROUTE(heap, g_heap); }
static inline std::vector<int>& heap_g_free() noexcept { return J2ME_VMS_ROUTE(heap, g_free); }
static inline std::vector<std::pair<std::vector<Frame>*, JavaThreadContext*>>& heap_g_root_stacks() noexcept { return J2ME_VMS_ROUTE(heap, g_root_stacks); }
static inline FrameBufferPool<Value>& heap_g_frame_buffer_pool() noexcept { return J2ME_VMS_ROUTE(heap, g_frame_buffer_pool); }
static inline PrimitiveBufferPool& heap_g_primitive_buffers() noexcept { return J2ME_VMS_ROUTE(heap, g_primitive_buffers); }
static inline std::ofstream& platform_g_log() noexcept { return J2ME_VMS_ROUTE(platform, g_log); }
static inline int64_t& platform_g_boot_ms() noexcept { return J2ME_VMS_ROUTE(platform, g_boot_ms); }
static inline bool& platform_g_host_frozen236() noexcept { return J2ME_VMS_ROUTE(platform, g_host_frozen236); }
static inline int64_t& platform_g_host_freeze_started236() noexcept { return J2ME_VMS_ROUTE(platform, g_host_freeze_started236); }
static inline std::unordered_set<int>& platform_g_pressed_keys() noexcept { return J2ME_VMS_ROUTE(platform, g_pressed_keys); }
static inline std::unordered_set<int>& platform_g_command_keys() noexcept { return J2ME_VMS_ROUTE(platform, g_command_keys); }
static inline j2me_audio::Engine& platform_g_audio() noexcept { p7audio::fence(); return J2ME_VMS_ROUTE(platform, g_audio); } // P7.3 fence
static inline j2me_audio::Engine& platform_g_audio_unfenced() noexcept { return J2ME_VMS_ROUTE(platform, g_audio); } // P7.3: only for the render owner
static inline int64_t& platform_g_audio_event_window() noexcept { return J2ME_VMS_ROUTE(platform, g_audio_event_window); }
static inline unsigned& platform_g_audio_event_count() noexcept { return J2ME_VMS_ROUTE(platform, g_audio_event_count); }
static inline uint64_t& platform_g_audio_event_suppressed() noexcept { return J2ME_VMS_ROUTE(platform, g_audio_event_suppressed); }
static inline std::unordered_set<int>& platform_g_tones() noexcept { return J2ME_VMS_ROUTE(platform, g_tones); }
#ifdef __SWITCH__
static inline j2me_audio::SwitchAudioSink& platform_g_audio_sink() noexcept { p7audio::fence(); return J2ME_VMS_ROUTE(platform, g_audio_sink); } // P7.3 fence
#endif
static inline int64_t& platform_g_cpu_window_start_ns() noexcept { return J2ME_VMS_ROUTE(platform, g_cpu_window_start_ns); }
static inline PresentStats& platform_g_present_stats() noexcept { return J2ME_VMS_ROUTE(platform, g_present_stats); }
static inline std::string& platform_g_rms_suite() noexcept { return J2ME_VMS_ROUTE(platform, g_rms_suite); }
static inline bool& platform_g_rms_import_legacy() noexcept { return J2ME_VMS_ROUTE(platform, g_rms_import_legacy); }
#ifdef __SWITCH__
static inline std::string& platform_g_launched_path() noexcept { return J2ME_VMS_ROUTE(platform, g_launched_path); }
#endif
#if defined(__SWITCH__) && defined(J2ME_ENABLE_DEKO3D)
static inline SwitchGpuPresent& platform_g_gpu_present() noexcept { return J2ME_VMS_ROUTE(platform, g_gpu_present); }
#endif
#if defined(__SWITCH__) && defined(J2ME_ENABLE_DEKO3D)
static inline GpuHostLayerCache& platform_g_gpu_layers() noexcept { return J2ME_VMS_ROUTE(platform, g_gpu_layers); }
#endif
#ifdef __SWITCH__
static inline Framebuffer& platform_g_win() noexcept { return J2ME_VMS_ROUTE(platform, g_win); }
#endif
#ifdef __SWITCH__
static inline bool& platform_g_win_ready() noexcept { return J2ME_VMS_ROUTE(platform, g_win_ready); }
#endif
#ifdef __SWITCH__
static inline bool& platform_g_gpu_active() noexcept { return J2ME_VMS_ROUTE(platform, g_gpu_active); }
#endif
#ifdef __SWITCH__
static inline int& platform_g_cpu_cleared() noexcept { return J2ME_VMS_ROUTE(platform, g_cpu_cleared); }
#endif
#ifdef __SWITCH__
static inline int& platform_g_chrome_bezel() noexcept { return J2ME_VMS_ROUTE(platform, g_chrome_bezel); }
#endif
#ifdef __SWITCH__
static inline int& platform_g_chrome_fps() noexcept { return J2ME_VMS_ROUTE(platform, g_chrome_fps); }
#endif
#ifdef __SWITCH__
static inline u32*& platform_g_cpu_frame() noexcept { return J2ME_VMS_ROUTE(platform, g_cpu_frame); }
#endif
#ifdef __SWITCH__
static inline u32& platform_g_cpu_stride() noexcept { return J2ME_VMS_ROUTE(platform, g_cpu_stride); }
#endif
#ifdef __SWITCH__
static inline int64_t& platform_g_cpu_acquire_us() noexcept { return J2ME_VMS_ROUTE(platform, g_cpu_acquire_us); }
#endif
static inline int64_t& platform_g_last_present_us() noexcept { return J2ME_VMS_ROUTE(platform, g_last_present_us); }
#ifdef __SWITCH__
static inline HostSurface& platform_g_host_surface() noexcept { return J2ME_VMS_ROUTE(platform, g_host_surface); }
#endif
#ifdef __SWITCH__
static inline bool& platform_g_exit_sheet() noexcept { return J2ME_VMS_ROUTE(platform, g_exit_sheet); }
#endif
#ifdef __SWITCH__
static inline bool& platform_g_sheet_paused_here() noexcept { return J2ME_VMS_ROUTE(platform, g_sheet_paused_here); }
#endif
#ifdef __SWITCH__
static inline u64& platform_g_sheet_swallow() noexcept { return J2ME_VMS_ROUTE(platform, g_sheet_swallow); }
#endif
#ifdef __SWITCH__
static inline std::string& platform_g_exit_sheet_note() noexcept { return J2ME_VMS_ROUTE(platform, g_exit_sheet_note); }
#endif
#ifdef __SWITCH__
static inline host_menu::Settings& platform_g_menu_settings() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_settings); }
#endif
#ifdef __SWITCH__
static inline host_menu::Focus& platform_g_menu_focus() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_focus); }
#endif
#ifdef __SWITCH__
static inline host_menu::Focus& platform_g_menu_option_focus() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_option_focus); }
#endif
#ifdef __SWITCH__
static inline std::vector<std::string>& platform_g_menu_labels() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_labels); }
#endif
#ifdef __SWITCH__
static inline std::vector<std::string>& platform_g_menu_paths() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_paths); }
#endif
#ifdef __SWITCH__
static inline std::vector<uint8_t>& platform_g_menu_saved() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_saved); }
#endif
#ifdef __SWITCH__
static inline std::map<std::string, int>& platform_g_game_speeds() noexcept { return J2ME_VMS_ROUTE(platform, g_game_speeds); }
#endif
#ifdef __SWITCH__
static inline int& platform_g_menu_selected() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_selected); }
#endif
#ifdef __SWITCH__
static inline bool& platform_g_fps_reset() noexcept { return J2ME_VMS_ROUTE(platform, g_fps_reset); }
#endif
#ifdef __SWITCH__
static inline host_font::Font& platform_g_host_font() noexcept { return J2ME_VMS_ROUTE(platform, g_host_font); }
#endif
#ifdef __SWITCH__
static inline bool& platform_g_host_font_service() noexcept { return J2ME_VMS_ROUTE(platform, g_host_font_service); }
#endif
static inline int& platform_g_key_states() noexcept { return J2ME_VMS_ROUTE(platform, g_key_states); }
static inline bool& platform_g_host_paused() noexcept { return J2ME_VMS_ROUTE(platform, g_host_paused); }
static inline int& platform_g_log_pending() noexcept { return J2ME_VMS_ROUTE(platform, g_log_pending); }
#ifdef __SWITCH__
static inline host_skin::Cache& platform_g_host_skin() noexcept { return J2ME_VMS_ROUTE(platform, g_host_skin); }
static inline std::string& platform_g_menu_status() noexcept { return J2ME_VMS_ROUTE(platform, g_menu_status); }
#endif
#ifdef __SWITCH__
static inline std::string& platform_g_import_label() noexcept { return J2ME_VMS_ROUTE(platform, g_import_label); }
#endif
static inline int64_t& platform_audio_pump_previous() noexcept { return J2ME_VMS_ROUTE(platform, audio_pump_previous); }
#ifdef __SWITCH__
static inline int64_t& platform_report_cpu_clock_last_ms() noexcept { return J2ME_VMS_ROUTE(platform, report_cpu_clock_last_ms); }
#endif
#ifdef __SWITCH__
static inline int& platform_present_fps_shown() noexcept { return J2ME_VMS_ROUTE(platform, present_fps_shown); }
#endif
#ifdef __SWITCH__
static inline uint64_t& platform_present_fps_seen() noexcept { return J2ME_VMS_ROUTE(platform, present_fps_seen); }
#endif
#ifdef __SWITCH__
static inline int64_t& platform_present_fps_t0() noexcept { return J2ME_VMS_ROUTE(platform, present_fps_t0); }
#endif
#ifdef __SWITCH__
static inline int& platform_main_sel() noexcept { return J2ME_VMS_ROUTE(platform, main_sel); }
#endif
#ifdef __SWITCH__
static inline int& platform_main_in_game() noexcept { return J2ME_VMS_ROUTE(platform, main_in_game); }
#endif
#ifdef __SWITCH__
static inline int& platform_main_last_ks() noexcept { return J2ME_VMS_ROUTE(platform, main_last_ks); }
#endif
#ifdef __SWITCH__
static inline bool& platform_main_import_prompt() noexcept { return J2ME_VMS_ROUTE(platform, main_import_prompt); }
#endif
#ifdef __SWITCH__
static inline int64_t& platform_cpu_clock_info_type() noexcept { return J2ME_VMS_ROUTE(platform, cpu_clock_info_type); }
#endif
static inline int& render_g_screen_w() noexcept { return J2ME_VMS_ROUTE(render, g_screen_w); }
static inline int& render_g_screen_h() noexcept { return J2ME_VMS_ROUTE(render, g_screen_h); }
static inline std::vector<uint8_t>& render_g_fb() noexcept { return J2ME_VMS_ROUTE(render, g_fb); }
static inline int& render_g_shown() noexcept { return J2ME_VMS_ROUTE(render, display.g_shown); }
static inline int& render_g_alert_return() noexcept { return J2ME_VMS_ROUTE(render, display.g_alert_return); }
static inline int64_t& render_g_alert_deadline() noexcept { return J2ME_VMS_ROUTE(render, display.g_alert_deadline); }
static inline int64_t& render_g_alert_remaining() noexcept { return J2ME_VMS_ROUTE(render, display.g_alert_remaining); }
static inline CanvasPaintStats& render_g_paint_stats() noexcept { return J2ME_VMS_ROUTE(render, g_paint_stats); }
static inline GraphicsProfile& render_g_graphics_profile() noexcept { return J2ME_VMS_ROUTE(render, g_graphics_profile); }
static inline FrameMetrics& render_g_frame_metrics() noexcept { return J2ME_VMS_ROUTE(render, g_frame_metrics); }
static inline DecodedImageCache& render_g_decoded_images() noexcept { return J2ME_VMS_ROUTE(render, g_decoded_images); }
static inline uint64_t& render_g_png_fast() noexcept { return J2ME_VMS_ROUTE(render, g_png_fast); }
static inline uint64_t& render_g_png_fallback() noexcept { return J2ME_VMS_ROUTE(render, g_png_fallback); }
static inline uint64_t& render_g_image_decode_us() noexcept { return J2ME_VMS_ROUTE(render, g_image_decode_us); }
static inline uint64_t& render_g_image_copy_us() noexcept { return J2ME_VMS_ROUTE(render, g_image_copy_us); }
static inline std::unordered_map<int, int>& render_g_offscreen() noexcept { return J2ME_VMS_ROUTE(render, g_offscreen); }
static inline std::unordered_map<int, int>& render_g_gfx_color() noexcept { return J2ME_VMS_ROUTE(render, g_gfx_color); }
static inline std::unordered_map<int, int>& render_g_gfx_target() noexcept { return J2ME_VMS_ROUTE(render, g_gfx_target); }
static inline std::unordered_map<int, int>& render_g_gfx_tx() noexcept { return J2ME_VMS_ROUTE(render, g_gfx_tx); }
static inline std::unordered_map<int, int>& render_g_gfx_ty() noexcept { return J2ME_VMS_ROUTE(render, g_gfx_ty); }
static inline std::unordered_map<int, Clip>& render_g_clip() noexcept { return J2ME_VMS_ROUTE(render, g_clip); }
static inline int& render_g_paint_depth() noexcept { return J2ME_VMS_ROUTE(render, g_paint_depth); }
static inline DrawAcc& render_g_drawacc() noexcept { return J2ME_VMS_ROUTE(render, g_drawacc); }
static inline std::vector<CjkGlyph>& render_g_cjk() noexcept { return J2ME_VMS_ROUTE(render, g_cjk); }
static inline bool& render_g_cjk_tried() noexcept { return J2ME_VMS_ROUTE(render, g_cjk_tried); }
static inline int& render_g_current() noexcept { return J2ME_VMS_ROUTE(render, display.g_current); }
static inline int& render_g_display() noexcept { return J2ME_VMS_ROUTE(render, display.g_display); }
static inline bool& render_g_repaint() noexcept { return J2ME_VMS_ROUTE(render, g_repaint); }
static inline bool& render_g_fb_dirty() noexcept { return J2ME_VMS_ROUTE(render, g_fb_dirty); }
static inline int& render_g_paint_serial() noexcept { return J2ME_VMS_ROUTE(render, g_paint_serial); }
static inline int& render_g_game_frames() noexcept { return J2ME_VMS_ROUTE(render, g_game_frames); }
static inline uint64_t& render_g_present_frames() noexcept { return J2ME_VMS_ROUTE(render, g_present_frames); }
static inline uint32_t& render_g_frame_sig() noexcept { return J2ME_VMS_ROUTE(render, g_frame_sig); }
static inline std::string& render_draw_end_prev() noexcept { return J2ME_VMS_ROUTE(render, draw_end_prev); }
static inline uint64_t& render_g_m3g_native_backgrounds() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_backgrounds); }
static inline uint64_t& render_g_m3g_native_background_pixels() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_background_pixels); }
static inline uint64_t& render_g_m3g_native_sorts() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_sorts); }
static inline uint64_t& render_g_m3g_native_vectors() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_vectors); }
static inline uint64_t& render_g_m3g_native_vertices() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_vertices); }
static inline uint64_t& render_g_m3g_native_matrices() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_matrices); }
static inline bool& render_g_m3g_prep241_enabled() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_prep241_enabled); }
static inline bool& render_g_m3g_mesh242_enabled() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_mesh242_enabled); }
static inline bool& render_g_host_boundary242_enabled() noexcept { return J2ME_VMS_ROUTE(render, g_host_boundary242_enabled); }
static inline uint64_t& render_g_m3g_native_triangles() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_triangles); }
static inline uint64_t& render_g_m3g_native_pixels() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_native_pixels); }
static inline uint64_t& render_g_m3g_fallback_aa() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_fallback_aa); }
static inline uint64_t& render_g_m3g_fallback_dither() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_fallback_dither); }
static inline uint64_t& render_g_m3g_fallback_fog() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_fallback_fog); }
static inline uint64_t& render_g_m3g_fallback_mip() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_fallback_mip); }
static inline bool& render_g_m3g_row240_enabled() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_row240_enabled); }
static inline bool& render_g_m3g_span239_enabled() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_span239_enabled); }
static inline bool& render_g_m3g_sprite241_enabled() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_sprite241_enabled); }
static inline bool& render_g_m3g_texture238_enabled() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_texture238_enabled); }
static inline uint64_t& render_g_m3g_texture238_calls() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_texture238_calls); }
static inline uint64_t& render_g_m3g_texture238_units() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_texture238_units); }
static inline uint64_t& render_g_m3g_texture238_mips() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_texture238_mips); }
static inline uint64_t& render_g_m3g_texture238_declined() noexcept { return J2ME_VMS_ROUTE(render, g_m3g_texture238_declined); }
static inline uint64_t& render_m3g_assembly242_gathered() noexcept { return J2ME_VMS_ROUTE(render, m3g_assembly242_gathered); }
static inline uint64_t& render_m3g_assembly242_culled() noexcept { return J2ME_VMS_ROUTE(render, m3g_assembly242_culled); }
static inline uint64_t& render_m3g_assembly242_emitted() noexcept { return J2ME_VMS_ROUTE(render, m3g_assembly242_emitted); }
static inline uint64_t& render_m3g_assembly242_declines() noexcept { return J2ME_VMS_ROUTE(render, m3g_assembly242_declines); }
static inline m3g_image241::Stats& render_m3g_image241_stats() noexcept { return J2ME_VMS_ROUTE(render, m3g_image241_stats); }
static inline uint64_t& render_m3g_lighting242_calls() noexcept { return J2ME_VMS_ROUTE(render, m3g_lighting242_calls); }
static inline uint64_t& render_m3g_lighting242_vertices() noexcept { return J2ME_VMS_ROUTE(render, m3g_lighting242_vertices); }
static inline uint64_t& render_m3g_lighting242_light_visits() noexcept { return J2ME_VMS_ROUTE(render, m3g_lighting242_light_visits); }
static inline uint64_t& render_m3g_lighting242_declines() noexcept { return J2ME_VMS_ROUTE(render, m3g_lighting242_declines); }
static inline m3g_mesh242::Stats& render_m3g_mesh242_stats() noexcept { return J2ME_VMS_ROUTE(render, m3g_mesh242_stats); }
static inline uint64_t& render_m3g_mesh242_composite_calls() noexcept { return J2ME_VMS_ROUTE(render, m3g_mesh242_composite_calls); }
static inline uint64_t& render_m3g_mesh242_transform_to_calls() noexcept { return J2ME_VMS_ROUTE(render, m3g_mesh242_transform_to_calls); }
static inline uint64_t& render_m3g_mesh242_transform_to_nodes() noexcept { return J2ME_VMS_ROUTE(render, m3g_mesh242_transform_to_nodes); }
static inline m3g_row240::Stats& render_m3g_row240_stats() noexcept { return J2ME_VMS_ROUTE(render, m3g_row240_stats); }
static inline m3g_span239::Stats& render_m3g_span239_stats() noexcept { return J2ME_VMS_ROUTE(render, m3g_span239_stats); }
static inline m3g_sprite241::Stats& render_m3g_sprite241_stats() noexcept { return J2ME_VMS_ROUTE(render, m3g_sprite241_stats); }
static inline paint_hotspots241::Snapshot& render_paint_hotspots241_current() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_current); }
static inline paint_hotspots241::Snapshot& render_paint_hotspots241_pending() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_pending); }
static inline bool& render_paint_hotspots241_active() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_active); }
static inline bool& render_paint_hotspots241_have_pending() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_have_pending); }
static inline uint64_t& render_paint_hotspots241_paints() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_paints); }
static inline uint64_t& render_paint_hotspots241_slow_paints() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_slow_paints); }
static inline uint64_t& render_paint_hotspots241_total_samples() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_total_samples); }
static inline uint64_t& render_paint_hotspots241_omitted_slow() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_omitted_slow); }
static inline int64_t& render_paint_hotspots241_longest_us() noexcept { return J2ME_VMS_ROUTE(render, paint_hotspots241_longest_us); }
}}  // namespace j2me_vmstate::acc
// namespace-scope reference bindings (VMS_RS_NSREF)
uint64_t& m3g_assembly242::vms_ref_gathered() noexcept { return ::j2me_vmstate::acc::render_m3g_assembly242_gathered(); }
uint64_t& m3g_assembly242::vms_ref_culled() noexcept { return ::j2me_vmstate::acc::render_m3g_assembly242_culled(); }
uint64_t& m3g_assembly242::vms_ref_emitted() noexcept { return ::j2me_vmstate::acc::render_m3g_assembly242_emitted(); }
uint64_t& m3g_assembly242::vms_ref_declines() noexcept { return ::j2me_vmstate::acc::render_m3g_assembly242_declines(); }
m3g_image241::Stats& m3g_image241::vms_ref_stats() noexcept { return ::j2me_vmstate::acc::render_m3g_image241_stats(); }
uint64_t& m3g_lighting242::vms_ref_calls() noexcept { return ::j2me_vmstate::acc::render_m3g_lighting242_calls(); }
uint64_t& m3g_lighting242::vms_ref_vertices() noexcept { return ::j2me_vmstate::acc::render_m3g_lighting242_vertices(); }
uint64_t& m3g_lighting242::vms_ref_light_visits() noexcept { return ::j2me_vmstate::acc::render_m3g_lighting242_light_visits(); }
uint64_t& m3g_lighting242::vms_ref_declines() noexcept { return ::j2me_vmstate::acc::render_m3g_lighting242_declines(); }
m3g_mesh242::Stats& m3g_mesh242::vms_ref_stats() noexcept { return ::j2me_vmstate::acc::render_m3g_mesh242_stats(); }
uint64_t& m3g_mesh242::vms_ref_composite_calls() noexcept { return ::j2me_vmstate::acc::render_m3g_mesh242_composite_calls(); }
uint64_t& m3g_mesh242::vms_ref_transform_to_calls() noexcept { return ::j2me_vmstate::acc::render_m3g_mesh242_transform_to_calls(); }
uint64_t& m3g_mesh242::vms_ref_transform_to_nodes() noexcept { return ::j2me_vmstate::acc::render_m3g_mesh242_transform_to_nodes(); }
m3g_row240::Stats& m3g_row240::vms_ref_stats() noexcept { return ::j2me_vmstate::acc::render_m3g_row240_stats(); }
m3g_span239::Stats& m3g_span239::vms_ref_stats() noexcept { return ::j2me_vmstate::acc::render_m3g_span239_stats(); }
m3g_sprite241::Stats& m3g_sprite241::vms_ref_stats() noexcept { return ::j2me_vmstate::acc::render_m3g_sprite241_stats(); }
paint_hotspots241::Snapshot& paint_hotspots241::vms_ref_current() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_current(); }
paint_hotspots241::Snapshot& paint_hotspots241::vms_ref_pending() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_pending(); }
bool& paint_hotspots241::vms_ref_active() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_active(); }
bool& paint_hotspots241::vms_ref_have_pending() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_have_pending(); }
uint64_t& paint_hotspots241::vms_ref_paints() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_paints(); }
uint64_t& paint_hotspots241::vms_ref_slow_paints() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_slow_paints(); }
uint64_t& paint_hotspots241::vms_ref_total_samples() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_total_samples(); }
uint64_t& paint_hotspots241::vms_ref_omitted_slow() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_omitted_slow(); }
int64_t& paint_hotspots241::vms_ref_longest_us() noexcept { return ::j2me_vmstate::acc::render_paint_hotspots241_longest_us(); }
