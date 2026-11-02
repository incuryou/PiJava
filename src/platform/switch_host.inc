// Horizon frontend, input, layered presenter and application lifecycle.
#include "../present_fps.h"
#include "../host_render_lifecycle.h"
#include <switch.h>
#include <dirent.h>
#include "../n1_paths.h"
#include "../host_menu.h"
#include "../game_names.h"
#include "../game_aliases.h"
#include "../game_favorites.h"
#include "../launcher_library.h"
#include "../menu_catalog.h"
#include "../game_title_cache.h"
#include "../host_menu_settings.h"
#include "../host_display_preferences.h"
#include "../host_display_menu.h"
#include "../host_keymap_ui.h"
#include "../host_data_editor.h"
#include "../controller_skin_labels.h"
#include "../presentation/orientation.h"
static presentation::orientation::Cache g_orientation;
static const std::vector<uint8_t>* g_display_pixels=nullptr;
static int g_display_w=240,g_display_h=320;
static host_keymap::Store g_keymaps;
static host_keymap::Editor g_keymap_editor;
static host_data::Editor g_data_editor;
static game_data::Session g_data_session;
static host_pro::Preview g_keymap_pro;
static host_keymap::Config g_keymap_active;
static host_controller::Legends g_controller_legends;
static host_menu::UniversalLayout g_universal_layout;
static auto g_universal_legend_layout=host_controller::universal_game_layout(g_universal_layout);
static bool standard_game_keys();
static host_keymap::Tracker g_keymap_tracker;
static std::string g_keymap_id, g_keymap_name;
static bool g_keymap_pro_attempted=false;
static u64 g_keymap_blocked=0;
static const char* g_keymap_path="sdmc:/switch/j2me/keymap.cfg";
#include "../host_speed.h"
#include "../speed_keys.h"
static speed_keys::State g_speed_keys; // P8.3: SHA256-keyed speed.cfg
#include "../host_skin.h"
#include "../host_font.h"
#include "../chapter_save_ui.h"
#ifdef J2ME_ENABLE_DEKO3D
#include "../switch_gpu_present.h"
#include "../switch_gpu_owner.h"
#include "../presentation/backend_policy.h"
static void menu_gpu_stage(SwitchGpuPresent::Stage stage){
    using G=SwitchGpuPresent::Stage;using M=menu_stall::Stage;M value=M::GpuCommands;
    switch(stage){case G::Acquire:value=M::GpuAcquire;break;case G::AcquireDone:value=M::GpuAcquireDone;break;
    case G::Fence:value=M::GpuFence;break;case G::Upload:value=M::GpuUpload;break;case G::Commands:value=M::GpuCommands;break;
    case G::Submit:value=M::GpuSubmit;break;case G::Present:value=M::GpuPresent;break;case G::Done:value=M::GpuDone;break;
    case G::Finish:value=M::GpuFinish;break;case G::Close:value=M::GpuClose;break;default:break;}
    if(p7::t_slot==p7::RenderSlot)menu_stall::gpu(value);else menu_stall::main(value);
}
// Host lifetime is explicit; __appExit can run after ordinary C++ destructors.
static SwitchGpuOwner* g_render_owner=nullptr;
static bool g_render_async=false;
static uint64_t g_render_sequence=0,g_render_epoch=0,g_render_credit_us=0;
static render_owner::ChromeSnapshot& render_chrome(){static auto* value=new render_owner::ChromeSnapshot;return *value;}
static render_owner::RgbSnapshot& render_rgb(){static auto* value=new render_owner::RgbSnapshot;return *value;}
static bool render_owner_barrier();
static bool render_owner_epoch(uint64_t epoch);
static m3g_gpu::GeometryResult render_gpu_geometry(m3g_gpu::GeometryRequest&& request);
static m3g_gpu::SkinResult render_gpu_skin(m3g_gpu::SkinRequest&& request);
static void render_owner_report(bool force=false);
 VMS_PLAT_ACCESSOR(g_gpu_present, SwitchGpuPresent)
struct GpuHostLayerCache {
    std::vector<uint32_t> rgba,canvas,raw; uint64_t generation=0;
    j2me_present::gpu_filter::FrameCache raw_frame;
    int logged_path=-1,logged_filter=-1,logged_w=0,logged_h=0;uint64_t logged_epoch=0;
    int bezel=-1,fps=-1,source_w=0,source_h=0; uint64_t mapping_generation=0; bool was_layered=false; std::string previous_label;
    void clear(){*this=GpuHostLayerCache{};}
};
 VMS_PLAT_ACCESSOR(g_gpu_layers, GpuHostLayerCache)
#endif
 VMS_PLAT_ACCESSOR(g_win, Framebuffer)
 VMS_PLAT_ACCESSOR(g_win_ready, bool)
 VMS_PLAT_ACCESSOR(g_gpu_active, bool)
 VMS_PLAT_ACCESSOR(g_cpu_cleared, int)
 VMS_PLAT_ACCESSOR(g_chrome_bezel, int)
 VMS_PLAT_ACCESSOR(g_chrome_fps, int)
 VMS_PLAT_ACCESSOR(g_cpu_frame, u32*)
 VMS_PLAT_ACCESSOR(g_cpu_stride, u32)
 VMS_PLAT_ACCESSOR(g_cpu_acquire_us, int64_t)
 VMS_PLAT_ACCESSOR(report_cpu_clock_last_ms, int64_t) VMS_PLAT_ACCESSOR(present_fps_shown, int) VMS_PLAT_ACCESSOR(present_fps_seen, uint64_t) VMS_PLAT_ACCESSOR(present_fps_t0, int64_t) VMS_PLAT_ACCESSOR(main_sel, int) VMS_PLAT_ACCESSOR(main_in_game, int) VMS_PLAT_ACCESSOR(main_last_ks, int) VMS_PLAT_ACCESSOR(main_import_prompt, bool)
// Read the effective clock, independently of presentation backend. Do not
// change the device clock while comparing scheduling improvements.
static void report_cpu_clock(bool force) {
    VMS_PLAT_LOCAL(last_ms, report_cpu_clock_last_ms, static int64_t last_ms=0;)
    const int64_t now=monotonic_ms();
    if(!force && now-last_ms<5000)return;
    last_ms=now;
    u32 hz=0;
    Result rc=clkrstInitialize();
    if(R_SUCCEEDED(rc)) {
        ClkrstSession cpu{};
        rc=clkrstOpenSession(&cpu,PcvModuleId_CpuBus,3);
        if(R_SUCCEEDED(rc)) {
            rc=clkrstGetClockRate(&cpu,&hz);
            clkrstCloseSession(&cpu);
        }
        clkrstExit();
    }
    if(R_SUCCEEDED(rc) && hz)log_line("CPU hz="+std::to_string(hz)+" source=clkrst available=1 query_only=1");
    else log_line("CPU unavailable=1 result="+std::to_string(rc)+" reason="+(R_SUCCEEDED(rc)?std::string("zero_rate"):std::string("query_failed"))+" source=clkrst query_only=1");
}
enum class HostSurface { Game, Menu, Import };
 VMS_PLAT_ACCESSOR(g_host_surface, HostSurface)
 VMS_PLAT_ACCESSOR(g_exit_sheet, bool)
 VMS_PLAT_ACCESSOR(g_sheet_paused_here, bool)
 VMS_PLAT_ACCESSOR(g_sheet_swallow, u64)
 VMS_PLAT_ACCESSOR(g_exit_sheet_note, std::string)
 VMS_PLAT_ACCESSOR(g_menu_settings, host_menu::Settings)
 VMS_PLAT_ACCESSOR(g_host_skin, host_skin::Cache)
 VMS_PLAT_ACCESSOR(g_menu_focus, host_menu::Focus)
 VMS_PLAT_ACCESSOR(g_menu_option_focus, host_menu::Focus)
 VMS_PLAT_ACCESSOR(g_menu_labels, std::vector<std::string>)
 VMS_PLAT_ACCESSOR(g_menu_paths, std::vector<std::string>)
 VMS_PLAT_ACCESSOR(g_menu_saved, std::vector<uint8_t>)
 VMS_PLAT_ACCESSOR(g_game_speeds, std::map<std::string, int>)
 VMS_PLAT_ACCESSOR(g_menu_selected, int)
 VMS_PLAT_ACCESSOR(g_menu_status, std::string) VMS_PLAT_ACCESSOR(g_import_label, std::string)
 VMS_PLAT_ACCESSOR(g_fps_reset, bool)
 VMS_PLAT_ACCESSOR(g_host_font, host_font::Font)
 VMS_PLAT_ACCESSOR(g_host_font_service, bool)
// P8.11 host-only menu UI state (display text and scroll clock; never keys or saves).
struct MenuNameUi { menu_catalog::State catalog; int64_t scroll_t0 = 0; int last_sel = -1; bool scrolling = false; int64_t last_tick = 0; };
static MenuNameUi g_menu_name_ui;
static game_titles::Service g_title_service;
static bool g_title_worker_available=false;
static std::map<std::string,size_t> g_title_indices;
static menu_actions::Deletion g_menu_delete;
static host_display_menu::Model g_display_menu;
static game_aliases::Store g_game_aliases;
static bool g_game_aliases_loaded=false;
static const char* g_game_aliases_path="sdmc:/switch/j2me/game-names.cfg";
static game_favorites::Store g_game_favorites;
static bool g_game_favorites_loaded=false;
static const char* g_game_favorites_path="sdmc:/switch/j2me/favorites.cfg";
static std::vector<uint8_t> g_menu_favorite_marks;
static launcher_library::View g_launcher_view;
static int g_launcher_total_count=0,g_launcher_favorite_count=0;
static menu_actions::Repeat g_menu_repeat;
static chapter_ui::State g_chapter_import;
static u64 g_chapter_blocked=0;
static const char* g_chapter_rms_root="sdmc:/switch/j2me/rms";
static bool g_menu_note_error=false,g_menu_note_neutral=true;
static void invalidate_display_preferences() {
    g_display_pixels=nullptr;
    g_orientation=presentation::orientation::Cache{};
    VMS_PLAT(g_cpu_cleared)=0;
    VMS_RS(g_fb_dirty)=true;
#ifdef J2ME_ENABLE_DEKO3D
    VMS_PLAT(g_gpu_layers).raw_frame.invalidate();
    if(!g_render_async)VMS_PLAT(g_gpu_present).invalidate_game();
    render_chrome()={};
#endif
}
static void select_display_preferences(const std::string& path,bool force=false) {
    static std::string selected_path;
    static bool loaded=false;
    if(!force&&loaded&&selected_path==path)return;
    selected_path=path;loaded=true;
    host_display::load_into(path,VMS_PLAT(g_menu_settings));
    invalidate_display_preferences();
    log_line("DISPLAY preferences path="+path+" rotation="+std::to_string(VMS_PLAT(g_menu_settings).rotation*90)+
             " canvas="+std::to_string(VMS_PLAT(g_menu_settings).canvas)+" source=per_game_sidecars");
}
static void close_host_fonts() {
    if (VMS_PLAT(g_host_font_service)) { plExit(); VMS_PLAT(g_host_font_service) = false; }
}
static void init_host_fonts() {
    Result rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc)) { log_line("FONT shared service unavailable result=" + std::to_string(rc) + " fallback=bitmap"); return; }
    VMS_PLAT(g_host_font_service) = true;
    int loaded = 0;
    for (PlSharedFontType type : {PlSharedFontType_Standard, PlSharedFontType_ChineseSimplified,
                                  PlSharedFontType_ExtChineseSimplified}) {
        PlFontData data{};
        rc = plGetSharedFontByType(&data, type);
        if (R_SUCCEEDED(rc) && VMS_PLAT(g_host_font).add_font(data.address, data.size)) ++loaded;
    }
    log_line("FONT menu=shared-font grayscale=1 faces=" + std::to_string(loaded) +
             " fallback=" + (VMS_PLAT(g_host_font).ready() ? std::string("none") : std::string("bitmap")));
}
static void host_text_bitmap(u32* frame, u32 pitch, int x, int y, const std::string& text,
                           u32 color, int size, int max_width) {
    if (size <= 0 || max_width <= 0) return;
    int cx = x;
    size_t offset = 0;
    while (offset < text.size()) {
        int cp = utf8_next(text, offset);
        int width = cp < 128 ? size / 2 : size;
        if (cx - x + width > max_width) break;
        const uint8_t* cjk = cp >= 128 ? find_cjk(cp) : nullptr;
        for (int row = 0; row < size; ++row) {
            int sy = row * 16 / size;
            unsigned bits = cp >= 32 && cp < 127 ? latin16[cp - 32][sy] :
                cp < 128 ? 0 : cjk ? (unsigned(cjk[sy * 2]) << 8) | cjk[sy * 2 + 1] :
                (sy == 0 || sy == 15 ? 0xffff : 0x8001);
            for (int col = 0; col < width; ++col) {
                int sx = col * (cp < 128 ? 8 : 16) / width;
                int xx = cx + col, yy = y + row;
                if ((bits & (0x8000u >> sx)) && (unsigned)xx < 1280 && (unsigned)yy < 720)
                    frame[size_t(yy) * pitch + xx] = color;
            }
        }
        cx += width;
    }
}
static void host_text_utf8(u32* frame, u32 pitch, int x, int y, const std::string& text,
                           u32 color, int size, int max_width) {
    if (!VMS_PLAT(g_host_font).draw(frame, pitch, x, y, text, color, size, max_width))
        host_text_bitmap(frame, pitch, x, y, text, color, size, max_width);
}
static host_menu::Text host_text_callback(u32* frame, u32 pitch, bool centered = false) {
    return [frame, pitch, centered](int x, int y, const std::string& text, u32 color, int size, int max_width) {
        if (centered) {
            if (!VMS_PLAT(g_host_font).draw_centered(frame, pitch, x, y, text, color, size, max_width))
                host_text_bitmap(frame, pitch, x - std::min(max_width, text_width(text)*size/16)/2,
                                 y-size/2, text, color, size, max_width);
        } else host_text_utf8(frame, pitch, x, y, text, color, size, max_width);
    };
}
static void sync_controller_legends() {
    // Check source geometry before the GPU decides whether to reuse its chrome.
    // Dimensions are a key even when their aspect ratio and frame ID repeat.
    const int layout_w=g_display_pixels?g_display_w:VMS_RS(g_screen_w);
    const int layout_h=g_display_pixels?g_display_h:VMS_RS(g_screen_h);
    if(g_universal_layout.update(layout_w,layout_h)){
        g_universal_legend_layout=host_controller::universal_game_layout(g_universal_layout);
        if(VMS_PLAT(g_menu_settings).bezel==j2me_present::adaptive_bezel)g_controller_legends.invalidate();
        VMS_PLAT(g_cpu_cleared)=0;
    }
    if(g_controller_legends.update(g_keymap_active,standard_game_keys(),VMS_PLAT(g_menu_settings).bezel,
        [](const std::string& text,int size){
            const auto measured=VMS_PLAT(g_host_font).measure(text,size);
            return measured.ok?measured.advance:host_menu::text_advance(text,size);
        },VMS_PLAT(g_menu_settings).bezel==j2me_present::adaptive_bezel?&g_universal_legend_layout:nullptr))VMS_PLAT(g_cpu_cleared)=0;
}
static bool save_menu_settings() {
    VMS_PLAT(g_cpu_cleared) = 0;
    // Only explicit menu changes write this small configuration, never a frame.
    mkdir("sdmc:/switch/j2me", 0777);
    bool ok = host_menu::save_settings("sdmc:/switch/j2me/menu.cfg", VMS_PLAT(g_menu_settings));
    log_line("MENU settings bezel=" + std::to_string(VMS_PLAT(g_menu_settings).bezel) +
             " show_fps=" + std::to_string(VMS_PLAT(g_menu_settings).show_fps) +
             " filter=" + std::to_string(int(VMS_PLAT(g_menu_settings).filter)) + " saved=" + std::to_string(ok));
    return ok;
}
static void close_presenter() {
#ifdef J2ME_ENABLE_DEKO3D
    m3g_gpu::geometry_executor=nullptr;m3g_gpu::skin_executor=nullptr;
    if(g_render_owner){g_render_owner->queue.close();if(g_render_async)render_owner_report(true);}
    g_render_async=false;render_chrome()={};render_rgb()={};g_render_credit_us=0;
    VMS_PLAT(g_gpu_present).close();VMS_PLAT(g_gpu_layers).clear();
#endif
    VMS_PLAT(g_gpu_active) = false;
    if(VMS_PLAT(g_cpu_frame) && VMS_PLAT(g_win_ready)) {framebufferEnd(&VMS_PLAT(g_win));VMS_PLAT(g_cpu_frame)=nullptr;}
    VMS_PLAT(g_cpu_stride)=0;VMS_PLAT(g_cpu_acquire_us)=0;
    if(VMS_PLAT(g_win_ready)) { framebufferClose(&VMS_PLAT(g_win)); VMS_PLAT(g_win_ready)=false; }
    VMS_PLAT(g_cpu_cleared)=0;
    VMS_PLAT(g_present_stats)={};
}
#ifdef J2ME_ENABLE_DEKO3D
static void render_owner_report(bool force) {
    if(!g_render_owner)return;
    static int64_t reported=0;
    render_owner::Completion error;
    if(g_render_owner->queue.poll_error(error))
        log_line("RENDEROWNER error sequence="+std::to_string(error.sequence)+" epoch="+std::to_string(error.epoch)+" reason="+error.error);
    const auto now=paint_clock_us();
    if(!force&&now-reported<1000000)return;
    reported=now;
    const auto stats=g_render_owner->queue.stats();
    render_owner::Completion completed;
    const bool has_completion=g_render_owner->queue.poll_completion(completed);
    const auto counts=g_render_owner->backend.counts();
    log_line("RENDEROWNER accepted_commands="+std::to_string(stats.accepted)+" completed_commands="+std::to_string(stats.submitted)+
        " present_submitted="+std::to_string(counts.presents)+" gpu_geometry_completed="+std::to_string(counts.geometry)+
        " gpu_skin_completed="+std::to_string(counts.skin)+" geometry_skin_declines="+std::to_string(counts.declines)+
        " m3g_allocations="+std::to_string(counts.allocations)+" m3g_reuses="+std::to_string(counts.reuses)+
        " m3g_resident_bytes="+std::to_string(counts.resident_bytes)+" m3g_peak_bytes="+std::to_string(counts.peak_bytes)+
        " fence_completed_at_barrier="+std::to_string(stats.fence_completed)+" cancelled="+std::to_string(stats.cancelled)+
        " producer_credit_wait_us="+std::to_string(g_render_credit_us)+" latest_sequence="+std::to_string(has_completion?completed.sequence:0)+
        " rgb_snapshot_copies="+std::to_string(render_rgb().copies)+" rgb_snapshot_bytes="+std::to_string(render_rgb().copy_bytes)+
        " rgb_snapshot_reuses="+std::to_string(render_rgb().reuses)+
        " latest_epoch="+std::to_string(has_completion?completed.epoch:0)+" latest_owner_us="+std::to_string(has_completion?completed.total_us:0)+
        " latest_gpu_acquire_us="+std::to_string(has_completion?completed.acquire_wait_us:0)+
        " latest_slot_fence_wait_us="+std::to_string(has_completion?completed.gpu_wait_us:0)+
        " latest_upload_us="+std::to_string(has_completion?completed.upload_us:0)+
        " scanout_verified=0 gpu_slots=3 cpu_pending_limit=1");
}
static bool render_owner_barrier() {
    if(!VMS_PLAT(g_gpu_active))return true;
    if(g_render_async?g_render_owner->queue.drain():VMS_PLAT(g_gpu_present).finish())return true;
    log_line("RENDEROWNER lifecycle drain failed; closing before CPU fallback");
    close_presenter();return true; // Joined and released; CPU fallback is safe.
}
static bool render_owner_epoch(uint64_t epoch) {
    render_rgb().clear();
    if(!g_render_async){const bool ok=render_owner_barrier();VMS_PLAT(g_gpu_present).invalidate_game();return ok;}
    g_render_owner->queue.cancel_before_epoch(epoch);
    if(!g_render_owner->queue.drain()||!g_render_owner->queue.resume(epoch)){
        close_presenter();return true; // Old epoch cannot reach the window now.
    }
    g_render_epoch=epoch;render_chrome()={};return true;
}
static m3g_gpu::GeometryResult render_gpu_geometry(m3g_gpu::GeometryRequest&& request) {
    using namespace m3g_gpu;
    if(!g_render_async||!g_render_owner)return {Code::Unavailable,"owner_unavailable",{}, {}};
    if(request.epoch!=resver::g_epoch)return {Code::Cancelled,"stale_epoch",{}, {}};
    if(g_render_epoch!=request.epoch&&!render_owner_epoch(request.epoch))return {Code::Unavailable,"owner_epoch",{}, {}};
    auto result=std::make_shared<ResultState<GeometryResult>>();
    render_owner::FrameRequest frame;frame.kind=render_owner::FrameRequest::Kind::Geometry;
    frame.sequence=++g_render_sequence;frame.epoch=request.epoch;
    frame.geometry.reset(new GeometryJob(std::move(request),result));
    // Java paint may already hold the sole producer credit. Reuse it.
    if(!render_owner::acquire_for_present(g_render_owner->queue)||!g_render_owner->queue.submit(std::move(frame)))
        return {Code::Unavailable,"owner_rejected",{}, {}};
    return result->wait(); // prototype CPU visibility barrier, not a throughput claim
}
static m3g_gpu::SkinResult render_gpu_skin(m3g_gpu::SkinRequest&& request) {
    using namespace m3g_gpu;
    if(!g_render_async||!g_render_owner)return {Code::Unavailable,"owner_unavailable",{}, {}};
    if(request.epoch!=resver::g_epoch)return {Code::Cancelled,"stale_epoch",{}, {}};
    if(g_render_epoch!=request.epoch&&!render_owner_epoch(request.epoch))return {Code::Unavailable,"owner_epoch",{}, {}};
    auto result=std::make_shared<ResultState<SkinResult>>();
    render_owner::FrameRequest frame;frame.kind=render_owner::FrameRequest::Kind::Skin;
    frame.sequence=++g_render_sequence;frame.epoch=request.epoch;
    frame.skin.reset(new SkinJob(std::move(request),result));
    if(!render_owner::acquire_for_present(g_render_owner->queue)||!g_render_owner->queue.submit(std::move(frame)))
        return {Code::Unavailable,"owner_rejected",{}, {}};
    return result->wait();
}
#endif
// Called after loading menu.cfg and on explicit filter changes, never per frame.
// Backend selection also selects the existing CPU/GPU scheduling branch.
static bool select_presenter() {
    bool request_gpu=false;
#ifdef J2ME_ENABLE_DEKO3D
    host_render_lifecycle::set_callbacks(render_owner_barrier,render_owner_epoch);
    VMS_PLAT(g_gpu_present).set_stage_callback(menu_gpu_stage);
    struct stat m3g_flag{};
    const bool m3g_requested=stat("sdmc:/switch/j2me/m3g-gpu.enable",&m3g_flag)==0;
    std::string owner_source;
    const bool owner_requested=m3g_requested||p810_switch("render-owner","J2ME_RENDER_OWNER",owner_source);
    request_gpu=j2me_present::request_gpu_backend(VMS_PLAT(g_menu_settings).filter,owner_requested,VMS_DIAG(g_p1_profile).active);
#endif
    if(request_gpu!=VMS_PLAT(g_gpu_active)) {
        log_drain();
        close_presenter(); // Release the old window owner before creating the next.
#ifdef J2ME_ENABLE_DEKO3D
        if(request_gpu) {
            if(owner_requested){
                static bool exit_registered=false;
                if(!exit_registered){
                    std::atexit([]{if(g_render_owner)g_render_owner->queue.close();g_render_async=false;});
                    exit_registered=true;
                }
                delete g_render_owner;g_render_owner=new SwitchGpuOwner;
                g_render_owner->backend.set_stage_callback(menu_gpu_stage);
                g_render_async=g_render_owner->queue.start();
                if(g_render_async){
                    g_render_epoch=resver::g_epoch;g_render_owner->queue.resume(g_render_epoch);
                    m3g_gpu::geometry_executor=render_gpu_geometry;m3g_gpu::skin_executor=render_gpu_skin;
                }
                VMS_PLAT(g_gpu_active)=g_render_async;
                // A failed owner init has already destroyed its window resources
                // on the owner; choose CPU rather than creating a second device.
                if(!g_render_async)render_owner_report(true);
                log_line("RENDEROWNER requested=1 enabled="+std::to_string(g_render_async)+" core=1 priority=45 audio_priority=44 producer_acquire=credit gpu_acquire=owner cpu_pending_limit=1 gpu_slots=3 sdk_acquire_cancel=unsupported");
            }else VMS_PLAT(g_gpu_active)=VMS_PLAT(g_gpu_present).init();
            if(!VMS_PLAT(g_gpu_active))log_line("RENDER GPU initialization returned failure; CPU selected");
        }
#endif
    }
    log_line(std::string("RENDER policy=filter_or_owner filter=")+std::to_string(int(VMS_PLAT(g_menu_settings).filter))+
        " requested="+(request_gpu?"gpu":"cpu")+" effective="+(VMS_PLAT(g_gpu_active)?"gpu":"cpu")+
        " p1_cpu="+std::to_string(VMS_DIAG(g_p1_profile).active));
    return VMS_PLAT(g_gpu_active);
}
// Apply the selected display filter at the main-menu lifecycle boundary.
static void apply_and_save_display_setting(std::string& status){
    invalidate_display_preferences();select_presenter();
    const bool saved=save_menu_settings();
    status=saved?"设置保存成功":"设置保存失败，本次运行仍然生效";
    g_menu_note_error=!saved;g_menu_note_neutral=false;
}
static void change_filter_menu_setting(std::string& status){
    if(!host_render_lifecycle::barrier()){
        status="暂时无法切换滤镜，请稍后重试";g_menu_note_error=true;g_menu_note_neutral=false;return;
    }
    host_menu::change_filter(VMS_PLAT(g_menu_settings),1);
    apply_and_save_display_setting(status);
}
// libnx calls __appExit after C++ destructors and the logger's atexit join.
// Use a local stream here; the normal logger's globals no longer exist.
static void elog(const char* s) {std::ofstream o("sdmc:/switch/j2me/logs/j2me-vm.log",std::ios::app);if(o)o<<s<<"\n";}
extern "C" void __nx_win_exit(void);
extern "C" void __appExit(void) {
    p8_exit_summary(); // P8.2: last game's P7COUNT when the program exits without a stop
    elog("appExit: close");
    close_presenter();
    close_host_fonts();
    __nx_win_exit(); hidExit(); appletExit(); romfsExit();
    elog("appExit: svcExitProcess");
    svcExitProcess();
}
static void host_text_size(u32* frame, u32 pitch, int x, int y, const std::string& s, u32 color, int size, int width=1280, int height=720) {
    int cx = x;
    for (unsigned char ch : s) {
        const uint8_t* g = font8x8_basic[ch & 127];
        for (int row = 0; row < size; ++row) {
            int srcRow = row * 8 / size;
            for (int col = 0; col < size; ++col) {
                if (!(g[srcRow] & (1 << (col * 8 / size)))) continue;
                int xx = cx + col, yy = y + row;
                if ((unsigned)xx < (unsigned)width && (unsigned)yy < (unsigned)height) frame[(u32)yy * pitch + (u32)xx] = color;
            }
        }
        cx += size;
    }
}
static void acquire_cpu_frame() {
    if(VMS_PLAT(g_gpu_active) || VMS_PLAT(g_cpu_frame))return;
    if (!VMS_PLAT(g_win_ready)) {
        Result rc = framebufferCreate(&VMS_PLAT(g_win), nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 3);
        if (R_FAILED(rc)) rc=framebufferCreate(&VMS_PLAT(g_win), nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
        if (R_FAILED(rc)) die("CPU framebufferCreate failed " + std::to_string(rc));
        rc=framebufferMakeLinear(&VMS_PLAT(g_win));
        if (R_FAILED(rc)) { framebufferClose(&VMS_PLAT(g_win)); die("CPU framebufferMakeLinear failed " + std::to_string(rc)); }
        VMS_PLAT(g_win_ready) = true;
    }
    const int64_t acquire_start=paint_clock_us();
    VMS_PLAT(g_cpu_stride)=0;
    VMS_PLAT(g_cpu_frame)=(u32*)framebufferBegin(&VMS_PLAT(g_win),&VMS_PLAT(g_cpu_stride));
    VMS_PLAT(g_cpu_acquire_us)=paint_clock_us()-acquire_start;
    if(!VMS_PLAT(g_cpu_frame) || VMS_PLAT(g_cpu_stride)<1280*4 || VMS_PLAT(g_cpu_stride)%4 != 0) {
        if(VMS_PLAT(g_cpu_frame)) {framebufferEnd(&VMS_PLAT(g_win));VMS_PLAT(g_cpu_frame)=nullptr;}
        die("CPU framebufferBegin failed");
    }
}

static void acquire_game_frame() {
#ifdef J2ME_ENABLE_DEKO3D
    if(VMS_PLAT(g_gpu_active)) {
        if(g_render_async){
            const auto start=paint_clock_us();
            const bool ok=render_owner::acquire_for_present(g_render_owner->queue);
            g_render_credit_us+=uint64_t(std::max<int64_t>(0,paint_clock_us()-start));
            if(ok)return;
        }else if(VMS_PLAT(g_gpu_present).acquire_frame())return;
        log_line("RENDER GPU acquire returned failure; falling back to CPU");
        close_presenter();
    }
#endif
    acquire_cpu_frame();
}

// Shared host chrome and overlay implementation: CPU and GPU use the same
// menus, phone skins, FPS placement, text metrics and pause sheet.
static void compose_host_surface(u32* frame,u32 pitch,const std::string& label,
                                 bool include_game,bool force_chrome) {
    if (g_keymap_editor.open) {
        if(g_data_editor.open) {
            host_data::View view;view.count=g_data_session.count();view.ready=g_data_session.ready();
            view.busy=g_data_session.busy();view.note=g_data_session.note;view.type=g_data_session.type();
            for(size_t i=g_data_editor.first();!view.busy&&i<view.count&&view.rows.size()<host_data::visible_rows;++i) {
                int64_t value=0;const bool readable=g_data_session.read(i,value);
                view.rows.push_back({g_data_session.label(i),readable?std::to_string(value):"位置已失效"});
            }
            host_data::render(frame,pitch,g_data_editor,view,host_text_callback(frame,pitch),host_text_callback(frame,pitch,true),
                [](const std::string& label,int size){const auto m=VMS_PLAT(g_host_font).measure(label,size);return m.ok?m.x+m.w:host_menu::text_advance(label,size);});
            return;
        }
        host_keymap::render(frame,pitch,g_keymap_editor,host_text_callback(frame,pitch),host_text_callback(frame,pitch,true),
            [](const std::string& label,int size) {
                const auto measured=VMS_PLAT(g_host_font).measure(label,size);
                return measured.ok?measured.x+measured.w:host_menu::text_advance(label,size);
            },&g_keymap_pro,[](const std::string& label,int size){const auto m=VMS_PLAT(g_host_font).measure(label,size);return host_controller::Ink{m.w,m.h,m.ok};});
        return;
    }
    if (VMS_PLAT(g_host_surface) == HostSurface::Menu) {
        host_menu::render_menu(frame, pitch, VMS_PLAT(g_menu_labels), VMS_PLAT(g_menu_selected), VMS_PLAT(g_menu_focus),
                               VMS_PLAT(g_menu_settings), VMS_PLAT(g_menu_status), host_text_callback(frame, pitch),
                               host_text_callback(frame, pitch, true),
                               VMS_PLAT(g_menu_selected) >= 0 && VMS_PLAT(g_menu_selected) < (int)VMS_PLAT(g_menu_paths).size()
                                   ? host_speed::label(speed_keys::lookup(g_speed_keys, VMS_PLAT(g_game_speeds), VMS_PLAT(g_menu_paths)[VMS_PLAT(g_menu_selected)]))
                                   : "原生",
                               VMS_PLAT(g_menu_saved),
                               [&](const std::string& s, int px) {
                                   const auto measured = VMS_PLAT(g_host_font).measure(s, px);
                                   return measured.ok && measured.advance > 0 ? measured.advance
                                                                              : host_menu::text_advance(s, px);
                               },
                               g_menu_name_ui.catalog.details, monotonic_ms() - g_menu_name_ui.scroll_t0, &g_menu_name_ui.scrolling,
                               g_menu_note_error,g_menu_note_neutral,g_menu_favorite_marks,
                               g_launcher_view.favorites_only,g_launcher_view.query,g_launcher_favorite_count,g_launcher_total_count);
        host_menu::render_delete(frame,pitch,g_menu_delete,host_text_callback(frame,pitch),host_text_callback(frame,pitch,true));
        host_display_menu::render(frame,pitch,g_display_menu,VMS_PLAT(g_menu_settings),host_text_callback(frame,pitch),host_text_callback(frame,pitch,true));
        chapter_ui::render(frame,pitch,g_chapter_import,host_text_callback(frame,pitch));
    } else if (VMS_PLAT(g_host_surface) == HostSurface::Import) {
        host_menu::rect(frame, pitch, 0, 0, 1280, 720, host_menu::color(0x1c201f));
        const auto text = host_text_callback(frame, pitch);
        const u32 white = host_menu::color(0xe4e9e5), muted = host_menu::color(0xa3afa7);
        text(64, 60, "导入旧存档", white, 32, 1152);
        text(64, 140, VMS_PLAT(g_import_label), white, 24, 1152);
        text(64, 232, "旧存档没有游戏标识，请确认属于当前游戏。", white, 24, 1152);
        text(64, 304, "仅复制游戏实际请求的存档；保留当前已有存档。", muted, 24, 1152);
        text(64, 360, "旧文件不移动、不删除、不改写。", muted, 24, 1152);
        text(64, 596, "X 导入并启动       B 取消", white, 24, 1152);
    } else {
    // Phone skin and the FPS badge are host chrome. Repaint them when the menu
    // selection changes so a continued game follows the current 遮罩 and 显示帧数.
    sync_controller_legends();
    const int display_w=g_display_pixels?g_display_w:VMS_RS(g_screen_w),display_h=g_display_pixels?g_display_h:VMS_RS(g_screen_h);
    const auto& display_rgb=g_display_pixels?*g_display_pixels:VMS_RS(g_fb);
    const int fps_bit = VMS_PLAT(g_menu_settings).show_fps ? 1 : 0;
    const auto viewport=j2me_present::game_viewport(display_w,display_h,VMS_PLAT(g_menu_settings).bezel==j2me_present::adaptive_bezel);
    static j2me_present::Viewport previous_viewport;
    static int logged_w=0,logged_h=0,logged_rotation=-1;
    if(viewport.x!=previous_viewport.x||viewport.y!=previous_viewport.y||viewport.w!=previous_viewport.w||viewport.h!=previous_viewport.h){
        previous_viewport=viewport;VMS_PLAT(g_cpu_cleared)=0;
    }
    if (VMS_PLAT(g_chrome_bezel) != VMS_PLAT(g_menu_settings).bezel || VMS_PLAT(g_chrome_fps) != fps_bit || logged_w!=display_w || logged_h!=display_h || logged_rotation!=VMS_PLAT(g_menu_settings).rotation) {
        logged_w=display_w;logged_h=display_h;logged_rotation=VMS_PLAT(g_menu_settings).rotation;
        VMS_PLAT(g_chrome_bezel) = VMS_PLAT(g_menu_settings).bezel;
        VMS_PLAT(g_chrome_fps) = fps_bit;
        VMS_PLAT(g_cpu_cleared) = 0;
        log_line("HOST chrome bezel=" + std::to_string(VMS_PLAT(g_menu_settings).bezel) +
                 " show_fps=" + std::to_string(fps_bit) +
                 " speed=" + host_speed::log_name(VMS_EXEC(g_speed_cap)) +
                 " source=" + std::to_string(VMS_RS(g_screen_w)) + "x" + std::to_string(VMS_RS(g_screen_h)) +
                 " rotation="+std::to_string(VMS_PLAT(g_menu_settings).rotation*90)+" oriented="+std::to_string(display_w)+"x"+std::to_string(display_h)+" output=" + std::to_string(viewport.x) + "," + std::to_string(viewport.y) + "," + std::to_string(viewport.w) + "," + std::to_string(viewport.h));
    }
    if (force_chrome || VMS_PLAT(g_cpu_cleared) < 3) {
        VMS_PLAT(g_host_skin).select(VMS_PLAT(g_menu_settings).bezel, [](const std::string& message) { log_line(message); });
        host_menu::render_bezel(frame, pitch, VMS_PLAT(g_menu_settings).bezel, host_text_callback(frame, pitch),
                                VMS_PLAT(g_host_skin).data(), VMS_PLAT(g_host_skin).size(),&g_universal_layout);
        if(VMS_PLAT(g_host_skin).data())g_controller_legends.render(frame,pitch,VMS_PLAT(g_menu_settings).bezel,host_text_callback(frame,pitch),host_controller::Context::Game,host_text_callback(frame,pitch,true));
        ++VMS_PLAT(g_cpu_cleared);
    }
    const uint8_t* src = display_rgb.data();
    const size_t fb_need = (size_t)display_w * (size_t)display_h * 3;
    if (include_game && logical_screen::valid_extent(display_w,display_h) && display_rgb.size() >= fb_need) {
    p7present::blit_game(reinterpret_cast<uint32_t*>(frame), uint32_t(pitch), src, display_w, display_h, VMS_PLAT(g_menu_settings).filter,viewport); // legacy retains P7.5 row split
    }
    if (VMS_PLAT(g_menu_settings).show_fps) {
        const auto prefix = VMS_PLAT(g_host_font).measure("FPS ", host_menu::fps_font);
        const auto tens = VMS_PLAT(g_host_font).measure("8", host_menu::fps_font);
        const auto slot = VMS_PLAT(g_host_font).measure("FPS 88", host_menu::fps_font);
        const auto num = VMS_PLAT(g_host_font).measure(label, host_menu::fps_font);
        host_menu::render_fps(frame, pitch, VMS_PLAT(g_menu_settings).bezel, label, host_text_callback(frame, pitch),
                              num.ok ? num.x : 0,
                              prefix.ok ? prefix.advance : 0,
                              tens.ok ? tens.advance : 0,
                              slot.ok ? slot.w : 0, slot.ok ? slot.h : 0,
                              slot.ok ? slot.x : 0, slot.ok ? slot.y : 0);
    }
    if (VMS_PLAT(g_exit_sheet)) {
        host_menu::render_exit_sheet(frame, pitch, host_text_callback(frame, pitch),
                                     host_text_callback(frame, pitch, true), VMS_PLAT(g_exit_sheet_note));
    }
    }
}

static void present_impl();
static void present(){menu_stall::main(menu_stall::Stage::Present);const auto phase=diag_phase_enter(runtime_diag::Phase::Present);present_impl();diag_native_leave(phase);menu_stall::main(menu_stall::Stage::PresentDone);}
static void present_impl() {
    const int64_t present_started=paint_clock_us();
    g_display_pixels=nullptr;
    g_display_w=VMS_RS(g_screen_w);g_display_h=VMS_RS(g_screen_h);
    if(VMS_PLAT(g_host_surface)==HostSurface::Game) {
        static int previous_w=0,previous_h=0,previous_rotation=-1;
        const int rotation=VMS_PLAT(g_menu_settings).rotation;
        if(previous_w!=VMS_RS(g_screen_w)||previous_h!=VMS_RS(g_screen_h)||previous_rotation!=rotation) {
            log_line("DISPLAY source_size="+std::to_string(VMS_RS(g_screen_w))+"x"+std::to_string(VMS_RS(g_screen_h))+
                " previous="+std::to_string(previous_w)+"x"+std::to_string(previous_h)+" rotation="+std::to_string(rotation*90));
            previous_w=VMS_RS(g_screen_w);previous_h=VMS_RS(g_screen_h);previous_rotation=rotation;
            invalidate_display_preferences(); // Also covers 180 degrees and square buffers.
        }
        g_display_pixels=&g_orientation.apply(VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h),rotation,resver::g_epoch,uint64_t(uint32_t(VMS_RS(g_game_frames))),VMS_RS(g_fb_dirty));
        g_display_w=g_orientation.width();g_display_h=g_orientation.height();
    }
    VMS_PLAT_LOCAL(fps_shown, present_fps_shown, static int fps_shown = 0;)
    VMS_PLAT_LOCAL(fps_seen, present_fps_seen, static uint64_t fps_seen = 0;)
    VMS_PLAT_LOCAL(fps_t0, present_fps_t0, static int64_t fps_t0 = 0;)
    int64_t now = paint_clock_us();
    const uint64_t fps_frames=VMS_RS(g_present_frames);  // 0.2.112 F4: frames delivered to the screen, not paint calls
    if (!fps_t0 || VMS_PLAT(g_fps_reset) || fps_frames < fps_seen) {
        fps_t0 = now; fps_seen = fps_frames; fps_shown = 0; VMS_PLAT(g_fps_reset) = false;
    }
    int64_t dt = now - fps_t0;
    if (dt >= 500000) {
        const uint64_t paints = fps_frames - fps_seen;
        fps_shown = rounded_present_fps(paints, dt);
        fps_seen = fps_frames;
        fps_t0 = now;
    }
    std::string label = std::to_string(fps_shown);
    sync_controller_legends();
#ifdef J2ME_ENABLE_DEKO3D
    if(VMS_PLAT(g_gpu_active)) {
        auto& cache=VMS_PLAT(g_gpu_layers);auto& rgba=cache.rgba;auto& canvas=cache.canvas;
        auto& generation=cache.generation;auto& bezel=cache.bezel;auto& fps=cache.fps;
        auto& was_layered=cache.was_layered;auto& previous_label=cache.previous_label;
        const bool layered=VMS_PLAT(g_host_surface)==HostSurface::Game && !VMS_PLAT(g_exit_sheet) && !g_keymap_editor.open;
        // Re-render all non-game surfaces. The layer cache is host-only: no Java
        // getRGB contents, game timing or save state depend on these pixels.
        const bool rebuild=!layered || !was_layered || canvas.size()!=j2me_present::canvas_pixels ||
            bezel!=VMS_PLAT(g_menu_settings).bezel || fps!=int(VMS_PLAT(g_menu_settings).show_fps) ||
            cache.mapping_generation!=g_controller_legends.generation() ||
            cache.source_w!=g_display_w || cache.source_h!=g_display_h ||
            (VMS_PLAT(g_menu_settings).show_fps && previous_label!=label) || VMS_PLAT(g_cpu_cleared)==0;
        if(rebuild) {
            canvas.resize(j2me_present::canvas_pixels);
            menu_stall::main(menu_stall::Stage::Compose);compose_host_surface(canvas.data(),1280,label,!layered,true);menu_stall::main(menu_stall::Stage::ComposeDone);
            ++generation;bezel=VMS_PLAT(g_menu_settings).bezel;fps=int(VMS_PLAT(g_menu_settings).show_fps);previous_label=label;
            cache.mapping_generation=g_controller_legends.generation();
            cache.source_w=g_display_w;cache.source_h=g_display_h;
        }
        was_layered=layered;
        const auto filter=VMS_PLAT(g_menu_settings).filter;
        const bool shader=layered&&display_filter::valid(filter);
        const bool adaptive=VMS_PLAT(g_menu_settings).bezel==j2me_present::adaptive_bezel;
        const j2me_present::gpu_filter::FrameKey frame_key{resver::g_epoch,uint64_t(uint32_t(VMS_RS(g_game_frames))),g_display_w,g_display_h,filter,adaptive};
        bool valid=true;
        if(g_render_async){
            valid=!layered||(g_display_pixels&&logical_screen::valid(frame_key.w,frame_key.h));
        }else if(shader){
            // Dirty is also an explicit refresh signal (for example host-side
            // restored pixels), even if a completed-frame counter repeats.
            const bool changed=cache.raw_frame.needs_upload(frame_key);
            if(VMS_RS(g_fb_dirty)&&!changed)VMS_PLAT(g_gpu_present).invalidate_game();
            if(changed||VMS_RS(g_fb_dirty)){
                valid=g_display_pixels&&logical_screen::valid(frame_key.w,frame_key.h)&&j2me_present::gpu_filter::pack(*g_display_pixels,frame_key.w,frame_key.h,filter,cache.raw,adaptive,true);
                if(valid)cache.raw_frame.committed(frame_key);
            }
        }else{
            cache.raw_frame.invalidate();VMS_PLAT(g_gpu_present).invalidate_game();
            valid=!layered||(g_display_pixels&&j2me_present::game_rgba(*g_display_pixels,g_display_w,g_display_h,rgba,filter));
        }
        const int path=shader?2:layered?1:0;
        if(cache.logged_path!=path||cache.logged_filter!=int(filter)||cache.logged_w!=frame_key.w||cache.logged_h!=frame_key.h||cache.logged_epoch!=frame_key.epoch){
            log_line(std::string("GPUFILTER path=")+(shader?"fragment_raw":layered?"cpu_prepared_blit":"composed_overlay")+
                " filter="+std::to_string(int(filter))+" source="+std::to_string(frame_key.w)+"x"+std::to_string(frame_key.h)+" epoch="+std::to_string(frame_key.epoch));
            cache.logged_path=path;cache.logged_filter=int(filter);cache.logged_w=frame_key.w;cache.logged_h=frame_key.h;cache.logged_epoch=frame_key.epoch;
        }
        const int64_t prepared=paint_clock_us();
        j2me_present::Viewport fps_overlay{0,0,0,0};
        if(layered&&adaptive&&VMS_PLAT(g_menu_settings).show_fps){
            const auto prefix=VMS_PLAT(g_host_font).measure("FPS ",host_menu::fps_font);
            const auto tens=VMS_PLAT(g_host_font).measure("8",host_menu::fps_font);
            const auto slot=VMS_PLAT(g_host_font).measure("FPS 88",host_menu::fps_font);
            const auto box=host_menu::place_fps(label,0,prefix.ok?prefix.advance:0,tens.ok?tens.advance:0,
                slot.ok?slot.w:0,slot.ok?slot.h:0,slot.ok?slot.x:0,slot.ok?slot.y:0);
            fps_overlay={box.x,box.y,std::min(box.w,1280-box.x),std::min(box.h,720-box.y)};
        }
        if(g_render_async){
            if(g_render_epoch!=frame_key.epoch)valid=render_owner_epoch(frame_key.epoch)&&valid;
            render_owner_report();
            render_owner::FrameRequest frame;
            frame.sequence=++g_render_sequence;frame.epoch=frame_key.epoch;frame.generation=generation;
            frame.chrome=render_chrome().freeze(canvas,generation);
            frame.key=frame_key;frame.layered=layered;frame.shader=shader;frame.adaptive=adaptive;
            frame.rotation=VMS_PLAT(g_menu_settings).rotation;frame.overlay=fps_overlay;
            frame.invalidate=VMS_RS(g_fb_dirty)||!shader;
            if(layered&&g_display_pixels)
                frame.rgb=render_rgb().freeze(*g_display_pixels,frame_key,frame.rotation,VMS_RS(g_fb_dirty));
            const auto credit_started=paint_clock_us();
            menu_stall::main(menu_stall::Stage::OwnerCredit);
            const bool credit=valid&&render_owner::acquire_for_present(g_render_owner->queue);
            const uint64_t submit_wait=uint64_t(std::max<int64_t>(0,paint_clock_us()-credit_started));
            g_render_credit_us+=submit_wait;
            menu_stall::main(menu_stall::Stage::OwnerSubmit);
            const bool accepted=credit&&g_render_owner->queue.submit(std::move(frame));
            const auto done=paint_clock_us();
            // Main timing describes this submission only. Owner completion N-1
            // never becomes Java N's deadline budget or synchronous GPU wait.
            const uint64_t total=uint64_t(done-present_started);
            VMS_PLAT(g_present_stats).add(total,submit_wait,total>submit_wait?total-submit_wait:0,0,0);
            if(accepted){
                VMS_PLAT(g_cpu_cleared)=3;
                if(VMS_EXEC(g_frame_work_finished_us)){
                    VMS_EXEC(g_frame_wait_budget).observe(VMS_EXEC(g_frame_work_us),std::max<int64_t>(0,done-VMS_EXEC(g_frame_work_finished_us)-int64_t(submit_wait)));
                    VMS_EXEC(g_frame_work_finished_us)=0;
                }
                return; // Java N+1 can execute while owner renders N.
            }
            log_line("RENDEROWNER accept failed; draining owner and selecting CPU");close_presenter();
        }else{
        const bool ok=valid && VMS_PLAT(g_gpu_present).present_layers(reinterpret_cast<const uint8_t*>(canvas.data()),generation,
                        layered&&!shader?reinterpret_cast<const uint8_t*>(rgba.data()):nullptr,
                        shader?cache.raw.data():nullptr,shader?&frame_key:nullptr,
                        fps_overlay.w>0?&fps_overlay:nullptr);
        const int64_t done=paint_clock_us();const auto timing=VMS_PLAT(g_gpu_present).lastTimings();
        const uint64_t prepare=uint64_t(prepared-present_started)+timing.upload_us;
        const uint64_t total=uint64_t(done-present_started)+timing.acquire_before_present_us;
        const uint64_t accounted=timing.acquire_wait_us+prepare+timing.gpu_wait_us;
        VMS_PLAT(g_present_stats).add(total,timing.acquire_wait_us,prepare,total>accounted?total-accounted:0,timing.gpu_wait_us);
        if(ok) {
            // One cached chrome canvas feeds all GPU slots; each slot's
            // generation still controls its upload. No three-pass CPU clear.
            VMS_PLAT(g_cpu_cleared)=3;
            // Same scheduler feedback contract as CPU presentation.
            if(VMS_EXEC(g_frame_work_finished_us)){
                const int64_t blocking_acquire=int64_t(timing.acquire_wait_us-timing.acquire_before_present_us);
                VMS_EXEC(g_frame_wait_budget).observe(VMS_EXEC(g_frame_work_us),std::max<int64_t>(0,done-VMS_EXEC(g_frame_work_finished_us)-blocking_acquire));
                VMS_EXEC(g_frame_work_finished_us)=0;
            }
            return;
        }
        log_line("RENDER GPU submission returned failure; falling back to CPU");close_presenter();
        }
        if(VMS_PLAT(g_host_surface)==HostSurface::Menu){VMS_PLAT(g_menu_status)="GPU 运行失败，已回退 CPU";g_menu_note_error=true;g_menu_note_neutral=false;}
    }
#endif
    menu_stall::main(menu_stall::Stage::CpuAcquire);
    { P7_SCOPE(PresentWait); acquire_cpu_frame(); }
    P7_SCOPE(Present);
    const int64_t copy_started=paint_clock_us();
    u32* frame=VMS_PLAT(g_cpu_frame);
    const u32 stride_bytes=VMS_PLAT(g_cpu_stride);
    u32 pitch = stride_bytes / 4;
    menu_stall::main(menu_stall::Stage::Compose);compose_host_surface(frame,pitch,label,true,false);menu_stall::main(menu_stall::Stage::ComposeDone);
    const int64_t prepared=paint_clock_us();
    menu_stall::main(menu_stall::Stage::CpuPresent);framebufferEnd(&VMS_PLAT(g_win));VMS_PLAT(g_cpu_frame)=nullptr;VMS_PLAT(g_cpu_stride)=0;
    const int64_t done=paint_clock_us();
    // Keep Java work between acquire and copy out of presentation timings.
    VMS_PLAT(g_present_stats).add(VMS_PLAT(g_cpu_acquire_us)+done-copy_started,VMS_PLAT(g_cpu_acquire_us),prepared-copy_started,done-prepared);
    // Include diagnostics and preparation after advance, but exclude framebuffer
    // acquisition, Java work already measured above, and the optional wake wait.
    if(VMS_EXEC(g_frame_work_finished_us)) {
        VMS_EXEC(g_frame_wait_budget).observe(VMS_EXEC(g_frame_work_us),done-VMS_EXEC(g_frame_work_finished_us));
        VMS_EXEC(g_frame_work_finished_us)=0;
    }
    VMS_PLAT(g_cpu_acquire_us)=0;
}
struct Entry { std::string label, path; };
static std::vector<Entry> g_launcher_catalog;
static std::unordered_map<std::string,size_t> g_launcher_catalog_indices;
static bool g_launcher_names_changed=false;
static void rebuild_launcher(std::vector<Entry>& items,int& selected){
    g_launcher_view.rebuild(g_launcher_catalog,[](const std::string& path){return g_game_favorites.contains(path);},items,selected);
    g_launcher_total_count=int(g_launcher_catalog.size());g_launcher_favorite_count=0;
    for(const auto& item:g_launcher_catalog)if(g_game_favorites.contains(item.path))++g_launcher_favorite_count;
    g_menu_name_ui.catalog.dirty=true;g_menu_repeat.reset();
}
static bool begin_chapter_import(const std::vector<Entry>& items,int selected){
    if(selected<0||size_t(selected)>=items.size())return false;
    for(size_t i=0;i<g_launcher_catalog.size();++i)if(g_launcher_catalog[i].path==items[size_t(selected)].path)
        return g_chapter_import.begin(g_launcher_catalog,int(i));
    return false;
}
static void remember_launcher_title(const std::string& path,const std::string& label){
    const auto found=g_launcher_catalog_indices.find(path);if(found==g_launcher_catalog_indices.end())return;
    auto& item=g_launcher_catalog[found->second];if(item.label!=label){item.label=label;g_launcher_names_changed=true;}
}

static std::string filename_label(const std::string& path) {
    const auto at=path.find_last_of("/\\");return at==std::string::npos?path:path.substr(at+1);
}
static bool apply_game_title(size_t i,const game_titles::Record& record) {
    auto& paths=VMS_PLAT(g_menu_paths);auto& labels=VMS_PLAT(g_menu_labels);auto& catalog=g_menu_name_ui.catalog;
    catalog.names.names[record.path]=record.name;catalog.names.details[record.path]=record.detail;
    const auto label=g_game_aliases.label(record.path,record.name.empty()?filename_label(record.path):record.name);
    remember_launcher_title(record.path,label);
    if(i>=paths.size()||i>=labels.size()||i>=catalog.details.size()||paths[i]!=record.path)return false;
    const bool changed=labels[i]!=label||catalog.details[i]!=record.detail;
    labels[i]=label;catalog.details[i]=record.detail;
    if(int(i)==VMS_PLAT(g_menu_selected)){catalog.name_pending=false;if(changed)g_menu_name_ui.scroll_t0=monotonic_ms();}
    return changed;
}
static bool refresh_visible_titles() {
    if(!g_title_worker_available)return false;
    const auto& paths=VMS_PLAT(g_menu_paths);if(paths.empty())return false;
    const size_t selected=std::min(size_t(std::max(0,VMS_PLAT(g_menu_selected))),paths.size()-1);
    const size_t first=selected/host_menu::visible_rows*host_menu::visible_rows;
    bool changed=false;
    for(size_t i=first;i<std::min(paths.size(),first+host_menu::visible_rows);++i){
        game_titles::Record record;if(g_title_service.find(paths[i],record))changed=apply_game_title(i,record)||changed;
    }
    return changed;
}
static std::vector<Entry> scan_games() {
    if(!g_game_aliases_loaded){g_game_aliases.load(g_game_aliases_path);g_game_aliases_loaded=true;}
    if(!g_game_favorites_loaded){g_game_favorites.load(g_game_favorites_path);g_game_favorites_loaded=true;}
    std::vector<Entry> items;
    std::vector<std::string> paths;
    for (auto& g : n1paths::scan_games("sdmc:/switch/j2me")) {
        game_titles::Record record;const bool known=g_title_worker_available&&g_title_service.find(g.path,record);
        items.push_back({g_game_aliases.label(g.path,known&&!record.name.empty()?record.name:g.label),g.path});
    }
    for(const auto& item:items)paths.push_back(item.path);
    g_title_service.reset(paths);
    log_line("GAMES scan=games_only dir=sdmc:/switch/j2me/games count=" + std::to_string(items.size()));
    g_launcher_catalog=items;g_launcher_catalog_indices.clear();g_launcher_catalog_indices.reserve(items.size());
    for(size_t i=0;i<g_launcher_catalog.size();++i)g_launcher_catalog_indices.emplace(g_launcher_catalog[i].path,i);
    int selected=0;rebuild_launcher(items,selected);
    return items;
}
static std::string instant_state_path(const std::string& jar) {
    std::string base = jar;
    const auto slash = base.find_last_of('/');
    if (slash != std::string::npos) base = base.substr(slash + 1);
    return "sdmc:/switch/j2me/state/" + base + ".sav";
}
static bool instant_state_exists(const std::string& jar) {
    struct stat st{};
    const std::string path = instant_state_path(jar);
    if (stat(path.c_str(), &st) == 0) return S_ISREG(st.st_mode);
    if (errno != ENOENT) return false;
    return stat((path + ".previous").c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
#include "../instant_state.inc"
static void draw_menu(const std::vector<Entry>& items, int sel, const std::string& status) {
    menu_stall::menu(true);menu_stall::progress.context(sel,int(items.size()));menu_stall::main(menu_stall::Stage::Draw);
    VMS_PLAT(g_host_surface) = HostSurface::Menu;
    VMS_PLAT(g_cpu_cleared) = 0;
    VMS_PLAT(g_menu_selected) = std::max(0, std::min(sel, std::max(0, int(items.size()) - 1)));
    VMS_PLAT(g_menu_status) = status;
    const int selected=VMS_PLAT(g_menu_selected);
    menu_stall::main(menu_stall::Stage::Preferences);
    select_display_preferences(items.empty()?std::string():items[selected].path);
    auto& catalog = g_menu_name_ui.catalog;
    if (catalog.dirty) {
        catalog.rebuild(items, VMS_PLAT(g_menu_labels), VMS_PLAT(g_menu_paths), VMS_PLAT(g_menu_saved));
        g_title_indices.clear();for(size_t i=0;i<items.size();++i)g_title_indices.emplace(items[i].path,i);
        g_menu_favorite_marks.clear();for(const auto& item:items)g_menu_favorite_marks.push_back(!g_launcher_view.favorites_only&&g_game_favorites.contains(item.path));
        g_menu_name_ui.last_sel = -1;
    }
    if (VMS_PLAT(g_menu_selected) != g_menu_name_ui.last_sel) { g_menu_name_ui.last_sel = VMS_PLAT(g_menu_selected); g_menu_name_ui.scroll_t0 = monotonic_ms(); }
    menu_stall::main(menu_stall::Stage::Page);
    catalog.prepare_page(VMS_PLAT(g_menu_paths), VMS_PLAT(g_menu_saved), VMS_PLAT(g_menu_selected), host_menu::visible_rows, instant_state_exists);
    menu_stall::main(menu_stall::Stage::Titles);refresh_visible_titles();
    menu_stall::main(menu_stall::Stage::Select);
    catalog.select(VMS_PLAT(g_menu_paths), VMS_PLAT(g_menu_selected), monotonic_ms());
    if(!items.empty()){
        const size_t first=size_t(selected)/host_menu::visible_rows*host_menu::visible_rows;
        std::vector<std::string> priority{items[selected].path};
        for(size_t i=first;i<std::min(items.size(),first+host_menu::visible_rows);++i)if(int(i)!=selected)priority.push_back(items[i].path);
        menu_stall::main(menu_stall::Stage::Prioritize);g_title_service.prioritize(priority);
    }
    menu_stall::main(menu_stall::Stage::DrawDone);
}
static void search_launcher(std::vector<Entry>& items,int& selected,std::string& status){
    g_title_service.enable(false);
    SwkbdConfig keyboard{};Result rc=swkbdCreate(&keyboard,0);
    if(R_FAILED(rc)){g_title_service.enable(true);status="系统键盘不可用，请重试";g_menu_note_error=true;g_menu_note_neutral=false;draw_menu(items,selected,status);return;}
    swkbdConfigMakePresetDefault(&keyboard);swkbdConfigSetType(&keyboard,SwkbdType_All);
    swkbdConfigSetReturnButtonFlag(&keyboard,0);swkbdConfigSetHeaderText(&keyboard,"搜索游戏");
    swkbdConfigSetGuideText(&keyboard,"输入游戏名称；支持中文，英文不区分大小写；留空显示全部");
    swkbdConfigSetInitialText(&keyboard,g_launcher_view.query.c_str());
    swkbdConfigSetStringLenMax(&keyboard,game_aliases::max_chars);swkbdConfigSetStringLenMin(&keyboard,0);
    char answer[game_aliases::max_chars*4+1]{};
    if(host_render_lifecycle::barrier()){menu_stall::Pause modal;rc=swkbdShow(&keyboard,answer,sizeof(answer));}else rc=MAKERESULT(Module_Libnx,LibnxError_BadInput);
    swkbdClose(&keyboard);g_title_service.enable(true);g_menu_repeat.reset();
    if(R_SUCCEEDED(rc)){
        std::string normalized;
        if(game_aliases::normalize(answer,normalized)){
            g_launcher_view.query=std::move(normalized);rebuild_launcher(items,selected);
            status=items.empty()?"没有匹配的游戏":"";g_menu_note_error=false;g_menu_note_neutral=true;
        }else{status="搜索内容无效，请重新输入";g_menu_note_error=true;g_menu_note_neutral=false;}
    }
    draw_menu(items,selected,status);
}
static void toggle_launcher_favorite(std::vector<Entry>& items,int& selected,std::string& status){
    if(selected<0||size_t(selected)>=items.size())return;
    const std::string path=items[selected].path;const bool favorite=!g_game_favorites.contains(path);
    const bool ok=g_game_favorites.set(g_game_favorites_path,path,favorite);
    if(ok)rebuild_launcher(items,selected);
    g_menu_repeat.reset();
    status=ok?(favorite?"已收藏游戏":"已取消收藏"):"收藏保存失败，请检查 SD 卡";
    g_menu_note_error=!ok;g_menu_note_neutral=false;draw_menu(items,selected,status);
}
// The main header has only favorite/all tabs; Search belongs to More Settings.
static bool update_launcher_navigation(u64 down,bool touch,int tx,int ty,std::vector<Entry>& items,int& selected,std::string& status,
        const std::function<int(const std::string&,int)>& advance={}){
    if(down&HidNpadButton_Minus){toggle_launcher_favorite(items,selected,status);return true;}
    if((down&HidNpadButton_B)&&!g_launcher_view.query.empty()){
        g_launcher_view.query.clear();rebuild_launcher(items,selected);
        VMS_PLAT(g_menu_focus)=host_menu::Focus::Games;
        status=items.empty()&&g_launcher_view.favorites_only?"暂无收藏游戏":"";
        g_menu_note_error=false;g_menu_note_neutral=true;draw_menu(items,selected,status);return true;
    }
    const int tab=touch?host_menu::library_tab_hit(tx,ty):-1;
    if((down&HidNpadButton_Y)||tab>=0||(touch&&host_menu::library_toggle_hit(tx,ty,g_launcher_favorite_count,g_launcher_total_count,advance))){
        g_launcher_view.favorites_only=tab>=0?tab==0:!g_launcher_view.favorites_only;
        rebuild_launcher(items,selected);
        g_menu_repeat.reset();VMS_PLAT(g_menu_focus)=host_menu::Focus::Games;
        status=items.empty()?(g_launcher_view.favorites_only?"暂无收藏游戏":"没有匹配的游戏"):"";
        g_menu_note_error=false;g_menu_note_neutral=true;draw_menu(items,selected,status);return true;
    }
    return false;
}
static void poll_menu_name(std::vector<Entry>& items,int& sel) {
    menu_stall::main(menu_stall::Stage::PollTitles);
    auto& catalog = g_menu_name_ui.catalog;
    bool changed=false;
    const int selected=VMS_PLAT(g_menu_selected);
    const bool uncached=selected>=0&&size_t(selected)<items.size()&&!g_title_service.contains(items[size_t(selected)].path);
    if(!g_title_worker_available||uncached){
        if(catalog.poll_name(VMS_PLAT(g_menu_paths),monotonic_ms(),g_menu_repeat.browsing)){
            const size_t i=size_t(std::max(0,VMS_PLAT(g_menu_selected)));
            if(i<items.size()){
                VMS_PLAT(g_menu_labels)[i]=g_game_aliases.label(items[i].path,catalog.title(items[i].path,filename_label(items[i].path)));
                changed=true;
            }
        }
    }
    if(g_title_worker_available)for(const auto& record:g_title_service.poll(8)){
        const auto found=g_title_indices.find(record.path);
        changed=apply_game_title(found==g_title_indices.end()?size_t(-1):found->second,record)||changed;
    }
    changed=refresh_visible_titles()||changed;
    if(changed)VMS_PLAT(g_cpu_cleared)=0;
    if(!items.empty()){
        const size_t selected=std::min(size_t(std::max(0,VMS_PLAT(g_menu_selected))),items.size()-1);
        const size_t first=selected/host_menu::visible_rows*host_menu::visible_rows;
        for(size_t i=first;i<std::min(items.size(),first+host_menu::visible_rows);++i){items[i].label=VMS_PLAT(g_menu_labels)[i];remember_launcher_title(items[i].path,items[i].label);}
    }
    if(g_launcher_names_changed){
        g_launcher_names_changed=false;
        if(!g_launcher_view.query.empty()){rebuild_launcher(items,sel);draw_menu(items,sel,VMS_PLAT(g_menu_status));}
    }
    menu_stall::main(menu_stall::Stage::PollDone);
}
static void draw_import_prompt(const Entry& entry) {
    VMS_PLAT(g_host_surface) = HostSurface::Import;
    VMS_PLAT(g_cpu_cleared) = 0;
    VMS_PLAT(g_import_label) = entry.label;
}
static void update_chapter_import(u64 down,u64 held,const std::vector<Entry>& items,int sel,std::string& status) {
    g_chapter_blocked&=held;down&=~g_chapter_blocked;held&=~g_chapter_blocked;
    const bool up=held&HidNpadButton_AnyUp,down_held=held&HidNpadButton_AnyDown;
    const bool left=held&(HidNpadButton_L|HidNpadButton_ZL),right=held&(HidNpadButton_R|HidNpadButton_ZR);
    const int direction=g_chapter_import.confirming?0:up!=down_held?(down_held?1:-1):
        !up&&!down_held&&left!=right?(right?chapter_ui::State::rows:-chapter_ui::State::rows):0;
    const int repeated=g_menu_repeat.update(direction,monotonic_ms());
    using chapter_ui::Action;using chapter_ui::Event;
    Event event=Event::None;
    if(down&HidNpadButton_B)g_chapter_import.act(Action::Back);
    else if(down&HidNpadButton_A)event=g_chapter_import.act(Action::Confirm);
    else if(repeated)g_chapter_import.act(repeated==chapter_ui::State::rows?Action::NextPage:
        repeated==-chapter_ui::State::rows?Action::PreviousPage:repeated>0?Action::Down:Action::Up);
    if(event==Event::Prepare)g_chapter_import.prepare("sdmc:/switch/j2me/rms",g_chapter_import.note);
    if(event==Event::Import){
        g_chapter_import.busy=true;VMS_PLAT(g_cpu_cleared)=0;present();
        const auto result=g_chapter_import.execute("sdmc:/switch/j2me/rms");
        g_menu_note_error=!result.ok;g_menu_note_neutral=false;
        status=result.ok?"已继承游戏内存档；按 A 开始，勿用 X 继续旧即时存档":result.error;
        log_line("CHAPTER import ok="+std::to_string(result.ok)+" stores="+std::to_string(result.stores)+
            " source="+g_chapter_import.source_id+" target="+g_chapter_import.target_id+" backup="+result.backup+" error="+result.error);
    }
    VMS_PLAT(g_cpu_cleared)=0;
    if(!g_chapter_import.open){g_menu_repeat.reset();draw_menu(items,sel,status);}
}
static void update_menu_delete(u64 down,std::vector<Entry>& items,int& sel,std::string& status,u64 held=0) {
    using menu_actions::Action;using menu_actions::Result;
    Result result=Result::None;
    if(down&HidNpadButton_B){
        const int selected=g_menu_delete.selection;
        g_menu_delete.open=false;g_menu_delete.confirming=false;
        g_display_menu.open=true;g_display_menu.selection=selected==2?2:selected==0?3:4;
    }
    else if(down&HidNpadButton_AnyUp)result=g_menu_delete.act(Action::Up);
    else if(down&HidNpadButton_AnyDown)result=g_menu_delete.act(Action::Down);
    else if(down&HidNpadButton_A)result=g_menu_delete.act(Action::Confirm);
    if(result==Result::InheritSave) {
        g_menu_repeat.reset();g_chapter_blocked=held;
        if(!begin_chapter_import(items,sel))status="至少需要两款游戏，才能选择来源章节";
        if(down)draw_menu(items,sel,status);
        return;
    }
    if(result!=Result::None) {
        const std::string path=g_menu_delete.path;
        bool ok=false;
        g_menu_note_neutral=false;
        if(result==Result::DeleteGame) {
            ok=menu_actions::remove_game(path,"sdmc:/switch/j2me");
            status=ok?"游戏删除成功":"删除游戏失败，请重试";
            if(ok){items=scan_games();sel=std::max(0,std::min(sel,int(items.size())-1));}
        }else {
            const bool exists=instant_state_exists(path);
            ok=exists && delete_instant_state(path);
            status=!exists?"没有即时存档":ok?"即时存档删除成功":"删除即时存档失败，请重试";
            g_menu_note_neutral=!exists;
            g_menu_name_ui.catalog.invalidate_saved();
        }
        g_menu_note_error=!ok && !g_menu_note_neutral;
        log_line(std::string("MENUDELETE kind=")+(result==Result::DeleteGame?"game":"instant")+" ok="+std::to_string(ok)+" path="+path);
    }
    if(down)draw_menu(items,sel,status);
}
static void update_display_menu(u64 down,u64 held,std::vector<Entry>& items,int& sel,std::string& status) {
    using host_display_menu::Action;
    if(down&HidNpadButton_B){
        g_display_menu.open=false;g_menu_repeat.reset();
        draw_menu(items,sel,status);return;
    }
    const bool up=(held|down)&HidNpadButton_AnyUp,downward=(held|down)&HidNpadButton_AnyDown;
    const int moved=g_menu_repeat.update(up!=downward?(downward?1:-1):0,monotonic_ms());
    if(moved)g_display_menu.move(moved);
    if(!(down&HidNpadButton_A)){if(down||moved)draw_menu(items,sel,status);return;}
    g_menu_repeat.reset();
    const auto action=g_display_menu.selected();
    if(action==Action::Back){g_display_menu.open=false;draw_menu(items,sel,status);return;}
    if(action==Action::Search){g_display_menu.open=false;search_launcher(items,sel,status);return;}
    if(items.empty()){g_display_menu.open=false;status="先选择一款游戏";draw_menu(items,sel,status);return;}
    if(action==Action::Rename) {
        g_display_menu.open=false;
        if(!g_game_aliases.writable){status="名称配置无法读取，请检查 SD 卡";g_menu_note_error=true;g_menu_note_neutral=false;draw_menu(items,sel,status);return;}
        g_title_service.enable(false);
        SwkbdConfig keyboard{};Result rc=swkbdCreate(&keyboard,0);
        if(R_FAILED(rc)){g_title_service.enable(true);status="系统键盘不可用，请重试";g_menu_note_error=true;g_menu_note_neutral=false;draw_menu(items,sel,status);return;}
        swkbdConfigMakePresetDefault(&keyboard);
        swkbdConfigSetType(&keyboard,SwkbdType_All);
        swkbdConfigSetReturnButtonFlag(&keyboard,0);
        swkbdConfigSetHeaderText(&keyboard,"重命名游戏");
        swkbdConfigSetGuideText(&keyboard,"可切换中文输入；留空恢复自动名称");
        std::string initial;
        const auto current_name=g_game_aliases.label(items[sel].path,g_menu_name_ui.catalog.title(items[sel].path,filename_label(items[sel].path)));
        if(!game_aliases::normalize(current_name,initial))initial.clear();
        swkbdConfigSetInitialText(&keyboard,initial.c_str());
        swkbdConfigSetStringLenMax(&keyboard,game_aliases::max_chars);
        swkbdConfigSetStringLenMin(&keyboard,0);
        char name[game_aliases::max_chars*4+1]={};
        host_render_lifecycle::barrier();{menu_stall::Pause modal;rc=swkbdShow(&keyboard,name,sizeof(name));}swkbdClose(&keyboard);g_title_service.enable(true);
        if(R_SUCCEEDED(rc)) {
            const bool ok=g_game_aliases.set(g_game_aliases_path,items[sel].path,name);
            if(ok){items[sel].label=g_game_aliases.label(items[sel].path,g_menu_name_ui.catalog.title(items[sel].path,filename_label(items[sel].path)));remember_launcher_title(items[sel].path,items[sel].label);rebuild_launcher(items,sel);g_menu_name_ui.scroll_t0=monotonic_ms();}
            status=ok?"游戏名称已更新":"保存名称失败，请检查名称长度或 SD 卡";
            g_menu_note_error=!ok;g_menu_note_neutral=false;
        }else {status="已取消或未完成重命名";g_menu_note_error=false;g_menu_note_neutral=true;}
        g_menu_repeat.reset();
    } else if(action==Action::Inherit) {
        g_display_menu.open=false;g_menu_repeat.reset();g_chapter_blocked=held;
        if(!begin_chapter_import(items,sel))status="至少需要两款游戏，才能选择来源章节";
    } else if(action==Action::DeleteGame||action==Action::DeleteInstant) {
        g_display_menu.open=false;
        g_menu_delete.begin(items[sel].path,items[sel].label);
        g_menu_delete.selection=action==Action::DeleteGame?0:1;
        g_menu_delete.confirming=true;
    }
    draw_menu(items,sel,status);
}
static std::string button_names(u64 down) {
    std::string s;
    auto add = [&](u64 b, const char* n) { if (down & b) { if (!s.empty()) s += "+"; s += n; } };
    add(HidNpadButton_A, "A"); add(HidNpadButton_B, "B"); add(HidNpadButton_X, "X"); add(HidNpadButton_Y, "Y");
    add(HidNpadButton_StickL, "L3"); add(HidNpadButton_StickR, "R3");
    add(HidNpadButton_Plus, "Plus"); add(HidNpadButton_Minus, "Minus");
    add(HidNpadButton_Up, "Up"); add(HidNpadButton_Down, "Down"); add(HidNpadButton_Left, "Left"); add(HidNpadButton_Right, "Right");
    add(HidNpadButton_StickLUp, "LSUp"); add(HidNpadButton_StickLDown, "LSDown");
    add(HidNpadButton_StickLLeft, "LSLeft"); add(HidNpadButton_StickLRight, "LSRight");
    add(HidNpadButton_StickRUp,"RSUp");add(HidNpadButton_StickRDown,"RSDown");
    add(HidNpadButton_StickRLeft,"RSLeft");add(HidNpadButton_StickRRight,"RSRight");
    add(HidNpadButton_L, "L"); add(HidNpadButton_R, "R");
    add(HidNpadButton_ZL, "ZL"); add(HidNpadButton_ZR, "ZR");
    if (s.empty()) s = "?" + std::to_string(down);
    return s;
}
static bool standard_game_keys() {
    return VMS_CL(g_jar_files).count("BioMain.class") && VMS_CL(g_jar_files).count("BioCanvas.class") && VMS_CL(g_jar_files).count("BioCtrl.class");
}
static uint32_t mapped_buttons(u64 held) {
    static const u64 bits[]={HidNpadButton_A,HidNpadButton_B,HidNpadButton_X,HidNpadButton_L,HidNpadButton_R,
        HidNpadButton_ZL,HidNpadButton_ZR,HidNpadButton_Plus,HidNpadButton_Minus,HidNpadButton_StickR,
        HidNpadButton_Up,HidNpadButton_Down,HidNpadButton_Left,HidNpadButton_Right,
        HidNpadButton_StickLUp,HidNpadButton_StickLDown,HidNpadButton_StickLLeft,HidNpadButton_StickLRight,
        HidNpadButton_Y,HidNpadButton_StickRUp,HidNpadButton_StickRDown,HidNpadButton_StickRLeft,HidNpadButton_StickRRight};
    static_assert(sizeof(bits)/sizeof(bits[0])==host_keymap::InputCount,"input IDs match physical bits");
    uint32_t result=0;for(int i=0;i<host_keymap::InputCount;++i)if(held&bits[i])result|=1u<<i;
    return result;
}
static std::set<int> mapped_codes(u64 held) {
    return host_keymap::codes(g_keymap_active,standard_game_keys(),mapped_buttons(held & ~g_keymap_blocked));
}
static void select_game_keymap(const Entry& entry,const std::string& id) {
    g_keymap_editor=host_keymap::Editor{};
    g_data_editor=host_data::Editor{};g_data_session.reset();g_data_session.set_type('I');
    std::map<std::string,std::string> display_props;
    for(const char* key:{"MIDlet-Name","MIDlet-1"}){auto it=VMS_CL(g_props).find(key);if(it!=VMS_CL(g_props).end())display_props.emplace(it->first,it->second);}
    g_keymap_id=id;g_keymap_name=game_names::choose_name(display_props);
    if(g_keymap_name.empty())g_keymap_name=game_names::strip_jar(entry.path);
    g_keymap_name=g_game_aliases.label(entry.path,g_keymap_name);
    g_keymap_active=host_keymap::lookup(g_keymaps,id);g_keymap_tracker.clear();g_keymap_blocked=~u64(0);
    g_controller_legends.invalidate();
    const auto* text_profile=game_canvas_text::profile(id);
    log_line("TEXTINPUT capability schema=1 standard_midp=1 canvas_profile="+std::string(text_profile?text_profile->id:"none")+" trigger=physical_A");
    log_line("KEYMAP game="+g_keymap_name+" id="+id+" source="+(g_keymaps.games.count(id)?std::string("game"):std::string("global"))+" auto="+std::to_string(g_keymap_active.automatic));
}
#include "switch_text_input.inc"
static void close_keymap(u64 held,bool focus_paused) {
    const bool was_paused=g_keymap_editor.was_paused;
    g_data_editor.open=false;if(g_data_session.busy())g_data_session.cancel();
    g_keymap_editor.open=false;g_keymap_tracker.clear();g_keymap_blocked|=held;
    if(!was_paused && !focus_paused)host_freeze236(false);
    VMS_PLAT(g_cpu_cleared)=0;VMS_RS(g_fb_dirty)=true;VMS_PLAT(g_fps_reset)=true;
    log_line("KEYMAP close blocked_until_release=1");
}
static void open_keymap(u64 held) {
    const bool paused=VMS_PLAT(g_host_paused);
    host_freeze236(true);g_keymap_tracker.clear();g_keymap_blocked|=held;
    g_keymap_editor.begin(g_keymaps,g_keymap_id,g_keymap_name,standard_game_keys(),paused);
    g_data_editor.open=g_keymap_editor.page==host_keymap::Page::Data;
    if(g_data_editor.open){g_data_session.valid();g_data_editor.begin();g_data_editor.clamp(g_data_session.count());}
    if(g_keymap_editor.save_note.empty()&&!g_canvas_notice.empty()) {
        g_keymap_editor.save_note=g_canvas_notice;
        g_keymap_editor.save_note_error=false;g_keymap_editor.save_note_neutral=true;
    }
    VMS_PLAT(g_cpu_cleared)=0;
    log_line("L3MENU open page="+std::string(g_keymap_editor.page==host_keymap::Page::Data?"data":g_keymap_editor.page==host_keymap::Page::Keymap?"keymap":"save_exit")+" L3_reserved=1 game="+g_keymap_name);
}
static host_data::InputResult request_game_data_value(const char* title,const std::string& guide,
                                                     const std::string& initial,int64_t& value) {
    SwkbdConfig keyboard{};Result rc=swkbdCreate(&keyboard,0);
    if(R_FAILED(rc))return host_data::InputResult::Unavailable;
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetType(&keyboard,SwkbdType_NumPad);
    swkbdConfigSetLeftOptionalSymbolKey(&keyboard,"-");
    swkbdConfigSetReturnButtonFlag(&keyboard,0);
    swkbdConfigSetHeaderText(&keyboard,title);
    swkbdConfigSetGuideText(&keyboard,guide.c_str());
    swkbdConfigSetInitialText(&keyboard,initial.c_str());
    swkbdConfigSetStringLenMin(&keyboard,1);swkbdConfigSetStringLenMax(&keyboard,20);
    char answer[128]{};host_render_lifecycle::barrier();{menu_stall::Pause modal;rc=swkbdShow(&keyboard,answer,sizeof answer);}swkbdClose(&keyboard);
    g_keymap_blocked=~u64(0); // Applet dismissal buttons must be released before game input.
    if(R_FAILED(rc))return host_data::InputResult::Cancelled;
    return game_data::parse_decimal(answer,value)?host_data::InputResult::Accepted:host_data::InputResult::Invalid;
}
static host_data::Result update_game_data_editor(u64 down,u64 held) {
    g_data_session.valid();g_data_editor.clamp(g_data_session.count());
    g_keymap_blocked|=held;
    if(down&(HidNpadButton_StickL|HidNpadButton_B)) {
        if(g_data_session.busy())g_data_session.cancel();g_data_editor.open=false;
        return host_data::Result::Resume;
    }
    if(g_data_session.busy()) {
        g_data_session.step(16384);g_data_editor.clamp(g_data_session.count());
        return host_data::Result::None;
    }
    if(down&HidNpadButton_Minus) {
        g_data_session.set_type(host_data::next_type(g_data_session.type()));
        g_data_editor.selection=0;g_data_editor.note.clear();g_data_editor.note_error=false;
    } else if(down&HidNpadButton_Y) {
        g_data_session.reset();g_data_editor.selection=0;g_data_editor.note.clear();g_data_editor.note_error=false;
    } else if(down&(HidNpadButton_Plus|HidNpadButton_X|HidNpadButton_A)) {
        const bool fresh=down&HidNpadButton_Plus,refine=!fresh&&(down&HidNpadButton_X),editing=!fresh&&!refine;
        int64_t old=0;
        if((refine||editing)&&(!g_data_session.ready()||!g_data_session.count())) {
            g_data_editor.note=g_data_session.ready()?"没有候选，请按 + 重新搜索":"请先按 + 完成首次搜索";g_data_editor.note_error=false;
            return host_data::Result::None;
        }
        if(editing&&!g_data_session.read(g_data_editor.selection,old)) {
            g_data_editor.note="选中位置已失效，请重新搜索";g_data_editor.note_error=true;return host_data::Result::None;
        }
        const std::string guide=editing?"旧值："+std::to_string(old)+"；位置："+g_data_session.label(g_data_editor.selection):
            refine?"输入游戏变化后的整数，筛选上一轮候选":"输入游戏中显示的整数（可用负号）";
        int64_t value=0;
        const auto input=request_game_data_value(editing?"修改选中数据":refine?"继续筛选":"首次搜索",guide,editing?std::to_string(old):"",value);
        if(input==host_data::InputResult::Cancelled)return host_data::Result::None;
        g_data_editor.note.clear();g_data_editor.note_error=false;
        if(input==host_data::InputResult::Unavailable) {g_data_editor.note="无法打开数字键盘，请重试";g_data_editor.note_error=true;}
        else if(input==host_data::InputResult::Invalid) {g_data_editor.note="请输入有效的十进制整数（64 位范围）";g_data_editor.note_error=true;}
        else if(editing) {
            const bool ok=g_data_session.write(g_data_editor.selection,value);
            g_data_editor.note=ok?"已修改："+std::to_string(old)+" → "+std::to_string(value)+"；位置："+g_data_session.label(g_data_editor.selection):"修改失败：位置失效或数值超出此类型范围";g_data_editor.note_error=!ok;
        } else {
            g_data_editor.selection=0;
            if(!g_data_session.begin(value,refine)) {g_data_editor.note=g_data_session.note;g_data_editor.note_error=true;}
        }
    } else if(down&HidNpadButton_AnyLeft)g_data_editor.move(-int(host_data::visible_rows),g_data_session.count());
    else if(down&HidNpadButton_AnyRight)g_data_editor.move(int(host_data::visible_rows),g_data_session.count());
    else if(down&HidNpadButton_AnyUp)g_data_editor.move(-1,g_data_session.count());
    else if(down&HidNpadButton_AnyDown)g_data_editor.move(1,g_data_session.count());
    return host_data::Result::None;
}
static bool update_keymap_editor(u64 down,u64 held,bool focus_paused) {
    if((down&(HidNpadButton_L|HidNpadButton_R))&&g_keymap_editor.pending==host_keymap::Pending::None) {
        if(g_data_session.busy())g_data_session.cancel();
        host_keymap::act(g_keymap_editor,down&HidNpadButton_L?host_keymap::Action::PreviousPage:host_keymap::Action::NextPage);
        g_data_editor.open=g_keymap_editor.page==host_keymap::Page::Data;
        if(g_data_editor.open){g_data_session.valid();g_data_editor.begin();g_data_editor.clamp(g_data_session.count());}
        if(g_keymap_editor.page==host_keymap::Page::Keymap&&!g_keymap_pro_attempted) {
            g_keymap_pro_attempted=true;
            log_line("KEYMAP pro_loaded="+std::to_string(g_keymap_pro.load("romfs:/ui/pro-controller.rgba")));
        }
        log_line("L3MENU page="+std::string(g_data_editor.open?"data":g_keymap_editor.page==host_keymap::Page::Keymap?"keymap":"save_exit"));
        g_keymap_blocked|=held;VMS_PLAT(g_cpu_cleared)=0;return false;
    }
    if(g_data_editor.open) {
        const auto result=update_game_data_editor(down,held);
        if(result==host_data::Result::Resume)close_keymap(held,focus_paused);
        return false; // All three tabs share the same pause owner.
    }
    const auto previous_page=g_keymap_editor.page;
    using host_keymap::Action;host_keymap::Result result=host_keymap::Result::None;
    const u64 directions=HidNpadButton_AnyUp|HidNpadButton_AnyDown|HidNpadButton_AnyLeft|HidNpadButton_AnyRight;
    const u64 arrows=(held&directions)?held:down;
    const bool forward=arrows&(HidNpadButton_AnyDown|HidNpadButton_AnyRight),backward=arrows&(HidNpadButton_AnyUp|HidNpadButton_AnyLeft);
    const int direction=forward!=backward?(forward?1:-1):0;
    const bool choosing=g_keymap_editor.open&&g_keymap_editor.page==host_keymap::Page::Keymap&&g_keymap_editor.phase==host_keymap::Phase::Choice;
    const bool command=down&(HidNpadButton_StickL|HidNpadButton_L|HidNpadButton_R|HidNpadButton_Plus|HidNpadButton_Minus|HidNpadButton_B|HidNpadButton_X|HidNpadButton_Y|HidNpadButton_A);
    const int repeated=g_keymap_editor.choice_repeat.update(choosing,direction,bool(down&directions),monotonic_ms(),command);
    if(down&HidNpadButton_StickL)result=host_keymap::act(g_keymap_editor,Action::Close);
    else if(down&HidNpadButton_L)result=host_keymap::act(g_keymap_editor,Action::PreviousPage);
    else if(down&HidNpadButton_R)result=host_keymap::act(g_keymap_editor,Action::NextPage);
    else if(down&HidNpadButton_Plus)result=host_keymap::act(g_keymap_editor,Action::Save);
    else if(down&HidNpadButton_Minus)result=host_keymap::act(g_keymap_editor,Action::Clear);
    else if(down&HidNpadButton_B)result=host_keymap::act(g_keymap_editor,Action::Back);
    else if(down&HidNpadButton_X)result=host_keymap::act(g_keymap_editor,Action::Reset);
    else if(down&HidNpadButton_Y)result=host_keymap::act(g_keymap_editor,Action::Global);
    else if(down&HidNpadButton_A)result=host_keymap::act(g_keymap_editor,Action::Confirm);
    else if(choosing){if(repeated)host_keymap::act(g_keymap_editor,repeated>0?Action::Down:Action::Up);}
    else if(down&HidNpadButton_AnyUp)host_keymap::act(g_keymap_editor,Action::Up);
    else if(down&HidNpadButton_AnyDown)host_keymap::act(g_keymap_editor,Action::Down);
    else if(down&HidNpadButton_AnyLeft)host_keymap::act(g_keymap_editor,Action::Left);
    else if(down&HidNpadButton_AnyRight)host_keymap::act(g_keymap_editor,Action::Right);
    if(previous_page!=g_keymap_editor.page)log_line(std::string("L3MENU page=")+(g_keymap_editor.page==host_keymap::Page::SaveExit?"save_exit":"keymap"));
    if(g_keymap_editor.page==host_keymap::Page::Keymap && !g_keymap_pro_attempted) {
        g_keymap_pro_attempted=true;
        log_line("KEYMAP pro_loaded="+std::to_string(g_keymap_pro.load("romfs:/ui/pro-controller.rgba")));
    }
    if(result==host_keymap::Result::Save) {
        const bool ok=host_keymap::commit(g_keymap_path,g_keymaps,g_keymap_id,g_keymap_editor.draft,g_keymap_editor.global);
        log_line("KEYMAP save ok="+std::to_string(ok)+" global="+std::to_string(g_keymap_editor.global)+" id="+g_keymap_id);
        if(ok){g_keymap_active=host_keymap::lookup(g_keymaps,g_keymap_id);g_controller_legends.invalidate();}
        g_keymap_editor.note=ok?"映射保存成功":"保存失败，请重试";
        g_keymap_editor.note_error=!ok;
    } else if(result==host_keymap::Result::SaveInstant) {
        const bool ok=sheet_save_instant_state(VMS_PLAT(g_launched_path));
        g_menu_name_ui.catalog.invalidate_saved();
        g_keymap_editor.save_note=ok?"已保存即时存档":"即时存档失败";
        g_keymap_editor.save_note_error=!ok;g_keymap_editor.save_note_neutral=false;
        log_line("L3MENU save_instant ok="+std::to_string(ok));
    } else if(result==host_keymap::Result::DeleteInstant) {
        const bool existed=instant_state_exists(VMS_PLAT(g_launched_path));
        const bool ok=existed && delete_instant_state(VMS_PLAT(g_launched_path));
        g_menu_name_ui.catalog.invalidate_saved();
        g_keymap_editor.save_note=!existed?"没有即时存档":ok?"已删除即时存档":"删除即时存档失败";
        g_keymap_editor.save_note_error=existed&&!ok;g_keymap_editor.save_note_neutral=!existed;
        log_line("L3MENU delete_instant existed="+std::to_string(existed)+" ok="+std::to_string(ok));
    } else if(result==host_keymap::Result::ExitGame) {
        g_data_editor.open=false;g_data_session.cancel();g_data_session.reset();
        g_keymap_editor.open=false;g_keymap_tracker.clear();g_keymap_blocked|=held;
        log_line("L3MENU exit confirmed=1");return true;
    } else if(result==host_keymap::Result::Close)close_keymap(held,focus_paused);
    return false;
}

static int jar_class_files() {
    int n = 0;
    for (auto& kv : VMS_CL(g_jar_files))
        if (kv.second.size() >= 4 && ru32(kv.second.data()) == 0xCAFEBABE) ++n;
    return n;
}
static void unpack_nested_jars() {
    if (jar_class_files() != 0) return;
    std::string inner;
    int jars = 0;
    for (auto& kv : VMS_CL(g_jar_files)) {
        std::string lower = kv.first;
        for (char& c : lower) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        bool zip = kv.second.size() >= 4 && kv.second[0] == 'P' && kv.second[1] == 'K';
        if (zip || (lower.size() > 4 && lower.substr(lower.size() - 4) == ".jar")) { ++jars; inner = kv.first; }
    }
    if (jars != 1) return;
    std::unordered_map<std::string, std::vector<uint8_t>> nested;
    std::string nested_error;
    std::vector<uint8_t> blob = VMS_CL(g_jar_files)[inner];
    if (unzip(blob, nested, nested_error)) {
        for (auto& kv : nested)
            if (VMS_CL(g_jar_files).find(kv.first) == VMS_CL(g_jar_files).end() || kv.first.find("MANIFEST.MF") != std::string::npos)
                VMS_CL(g_jar_files)[kv.first] = std::move(kv.second);
        log_line("JAR nested " + inner + " files=" + std::to_string(nested.size()));
    } else log_line("JAR nested failed " + nested_error);
}
static void launch_entry(const Entry& e, bool import_legacy = false) {
    menu_stall::menu(false);
    g_title_service.enable(false);
    reset_game_text_input();
    select_display_preferences(e.path,true);
    VMS_PLAT(g_launched_path) = e.path;
    VMS_PLAT(g_host_surface) = HostSurface::Game;
    VMS_PLAT(g_cpu_cleared) = 0;
    VMS_PLAT(g_fps_reset) = true;
    VMS_EXEC(g_speed_cap) = VMS_DIAG(g_p1_profile).active?0:speed_keys::lookup(g_speed_keys, VMS_PLAT(g_game_speeds), e.path); speed_keys::save_cache(g_speed_keys);
    VMS_EXEC(g_speed_last_paint_us) = 0;
    VMS_EXEC(g_speed_next_us) = 0;
    VMS_EXEC(g_speed_slept_us) = 0;
    VMS_EXEC(g_speed_frame_open) = false;
    log_line(std::string("SPEED cap=") + host_speed::log_name(VMS_EXEC(g_speed_cap)) + " path=" + e.path + " key=" + speed_keys::key_for(g_speed_keys, e.path));
    VMS_CL(g_jar_files).clear(); VMS_CL(g_props).clear();
    std::vector<uint8_t> bytes;
    if (!read_whole(e.path, bytes)) die("cannot read " + e.path);
    std::string err;
    if (!unzip(bytes, VMS_CL(g_jar_files), err)) die(err);
    unpack_nested_jars();
    load_props();
    load_sidecar_jad(e.path);
    const std::string identity=rms_jar_identity(bytes.data(),bytes.size());
    if(!chapter_save::recover(g_chapter_rms_root,identity,err))die("存档恢复失败："+err);
    select_screen_sidecar(e.path,identity);
    std::string main = manifest_midlet();
    if (main.empty()) main = find_midlet_class();
    if (main.empty()) {
        std::string keys, entries;
        int classes = jar_class_files();
        for (auto& kv : VMS_CL(g_props)) {
            if (!keys.empty()) keys += ",";
            keys += kv.first;
            if (keys.size() > 160) break;
        }
        for (auto& kv : VMS_CL(g_jar_files)) {
            if (!entries.empty()) entries += ",";
            entries += kv.first;
            if (entries.size() > 180) break;
        }
        die("no MIDlet-1 keys=" + keys + " classes=" + std::to_string(classes) + " entries=" + entries);
    } else if (!has_midlet_prop()) {
        log_line("MIDlet guessed " + main);
    }
    for (char& c : main) if (c == '.') c = '/';
    std::string files;
    for (auto& kv : VMS_CL(g_jar_files)) {
        if (kv.first.size() > 6 && kv.first.find(".class") != std::string::npos) {
            if (!files.empty()) files += ",";
            auto slash = kv.first.find_last_of('/');
            files += kv.first.substr(slash == std::string::npos ? 0 : slash + 1);
        }
    }
    log_line("JAR " + e.label + " midlet=" + main + " files=" + std::to_string(VMS_CL(g_jar_files).size()) + " class=" + files);
    log_line("LAUNCH class=" + main);
    VMS_CL(g_classpath).clear();
    VMS_CL(g_classpath).push_back("romfs:/classes");
    select_game_keymap(e,identity);
    launch_class(main, identity, import_legacy);
}
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    VMS_PLAT(g_boot_ms) = now_ms();
    appletLockExit();
    appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
    romfsInit();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeAny(&pad);
    hidInitializeTouchScreen();bool launcher_touch_held=false;
    bool log_rotated = false;
    const std::string log_path = n1paths::prepare_log("sdmc:/switch/j2me", &log_rotated);
    mkdir("sdmc:/switch/j2me/games", 0777);
    log_open(log_path.c_str(), true);
    log_line(std::string("==== BOOT ") + kVer + " ====");
    log_line(n1paths::legacy_log_note("sdmc:/switch/j2me-vm.log") + (log_rotated ? " prev_rotated=1" : " prev_rotated=0"));
    // 0.2.107: Switch has no test mode; sdmc:/switch/j2me/test-profile.json is never opened.
    log_line("TESTMODE switch=absent test_profile_read=0");
    configure_runtime_diagnostics();
    init_host_fonts();
    log_line("PROG keys=up2 down8 left4 right6 L1 R3 ZL7 ZR9 minus* plus# A5 RStick0 X=left-soft B=right-soft Ymenu");
    Result audio_rc=VMS_PLAT(g_audio_sink).init();
    log_line(R_SUCCEEDED(audio_rc) ? "AUDIO 48000Hz stereo PCM16 MIDI/WAV; approximate GM synth" : "AUDIO unavailable result="+std::to_string(audio_rc));
    log_line("PROG L3=game settings; Java exception traces enabled");
#ifdef J2ME_ENABLE_DEKO3D
    log_line("GPUFILTER available=Bilinear,Scale2x,LCD,CRT,ScanlinesDX path=offline_fragment_raw cache=epoch_frame_dimensions_filter backend_policy=filter");
    log_line("GPUSCHED policy=shared_cpu submit=new_frame_or_heartbeat acquire=before_java wake_budget=bounded swap_interval=1");
#else
    log_line("RENDER policy=filter effective=cpu reason=cpu_only_build");
#ifdef __SWITCH__
    // Clock policy belongs to the host/user. Both render builds are query-only.

#endif
    log_line("SCHED mode=budgeted-near-wake wait_cap_ms=6 history_frames=8 guard_ms=1 Java_sleep=unchanged");
    log_line("PACE audio=one-block-if-queued<2 sleep_align_ms=8..20 longer_sleep=wallclock host_slice_ms=14 extra_slice=clean-runnable present=skip-unchanged heartbeat_ms=50 quick=pc-site gfx=image-intrinsic speed=owned-deadline poll=verified-gate");
    log_line("GFXPROFILE sample_every=64 native_times=inclusive raster=subset_of_image_native");
#endif
    report_cpu_clock(true);
    u32 fw = 0, fh = 0; nwindowGetDimensions(nwindowGetDefault(), &fw, &fh);
    log_line("SCREEN " + std::to_string(fw) + "x" + std::to_string(fh));
    VMS_RS(g_screen_w) = 240; VMS_RS(g_screen_h) = 320;
    VMS_RS(g_fb).assign((size_t)VMS_RS(g_screen_w) * VMS_RS(g_screen_h) * 3, 0);
    g_title_worker_available=g_title_service.start("sdmc:/switch/j2me/game-titles.cache");
    log_line("GAMETITLES worker="+std::to_string(g_title_worker_available)+" cache=game-titles.cache title_only=1");
    static std::vector<Entry> items = scan_games();
    VMS_PLAT_LOCAL3(sel, main_sel, in_game, main_in_game, last_ks, main_last_ks, static int sel = 0, in_game = 0, last_ks = -1;)
    VMS_PLAT_LOCAL(import_prompt, main_import_prompt, static bool import_prompt = false;)
    bool focus_paused=false,renderer_focused=true;
    const bool config_loaded = host_menu::load_settings("sdmc:/switch/j2me/menu.cfg", VMS_PLAT(g_menu_settings));
    // This test build starts in aspect-fit even when an earlier skin was saved.
    VMS_PLAT(g_menu_settings).bezel = j2me_present::adaptive_bezel;
    select_presenter();
    std::string status;
#ifdef J2ME_ENABLE_DEKO3D
    if(j2me_present::gpu_filter::supported(VMS_PLAT(g_menu_settings).filter)&&!VMS_PLAT(g_gpu_active))
        status=VMS_DIAG(g_p1_profile).active?"P1 诊断配置固定 CPU；停用 p1-profile.cfg 后可测试 GPU":"GPU 初始化失败，已回退 CPU";
#else
    status="CPU 显示后端（此构建不含 GPU）";
#endif
    int sd_jars = 0; for (auto& it : items) if (it.path.find("sdmc:") == 0) sd_jars++;
    log_line("MENU items=" + std::to_string(items.size()) + " sd_jars=" + std::to_string(sd_jars));
    VMS_CL(g_classpath).push_back("romfs:/classes");
    const bool keymaps_loaded=host_keymap::load(g_keymap_path,g_keymaps);
    log_line("KEYMAP config_loaded="+std::to_string(keymaps_loaded)+" per_game="+std::to_string(g_keymaps.games.size())+" L3_reserved=1 Y_reserved=0 inputs=23");
    const bool speeds_loaded = host_speed::load("sdmc:/switch/j2me/speed.cfg", VMS_PLAT(g_game_speeds));
    g_speed_keys.dir = "sdmc:/switch/j2me"; speed_keys::load_cache(g_speed_keys);
    { const auto mr = speed_keys::migrate(g_speed_keys, VMS_PLAT(g_game_speeds)); speed_keys::save_cache(g_speed_keys);
      log_line("SPEEDKEY key=sha256 migrated=" + std::to_string(mr.migrated) + " kept_unresolved=" + std::to_string(mr.kept) +
               " rewritten=" + std::to_string(mr.rewritten) + " bak_written=" + std::to_string(mr.bak_written) + " save_ok=" + std::to_string(mr.save_ok) +
               " hashed=" + std::to_string(g_speed_keys.hashed) + " cache_hits=" + std::to_string(g_speed_keys.hits) + " cache_entries=" + std::to_string(g_speed_keys.cache.size())); }
    log_line("MENU native=1280x720 viewport=aspect-fit first-edge-stop speed=per-game native/30/60 settings_loaded=" + std::to_string(config_loaded) +
             " speeds_loaded=" + std::to_string(speeds_loaded) + " speeds=" + std::to_string(VMS_PLAT(g_game_speeds).size()));
    menu_stall::menu(true);draw_menu(items, sel, status);
    static AppletHookCookie p8_exit_cookie; // P8.2: HOME->close / exit request writes the last P7COUNT summary
    appletHook(&p8_exit_cookie, [](AppletHookType t, void*) { if (t == AppletHookType_OnExitRequest) p8_exit_summary(); }, nullptr);
    g_die_catch = true;
    while (true) {
        menu_stall::main(menu_stall::Stage::Applet);
        if(!appletMainLoop())break;
        menu_stall::main(menu_stall::Stage::Loop);
        diag_host(in_game!=0,VMS_PLAT(g_host_paused)||VMS_PLAT(g_exit_sheet)||g_keymap_editor.open);
        if (setjmp(g_die_jmp) != 0) {
            // An uncaught Java failure can bypass ordinary lifecycle_destroy.
            // Retire the accepted immutable game frames before drawing its menu.
            host_render_lifecycle::barrier();
            reset_game_text_input();
            g_device_diag.owner.phase=runtime_diag::Phase::Error;g_device_diag.live.publish(g_device_diag.owner);
            g_keymap_editor.open=false;g_data_editor.open=false;g_data_session.reset();g_keymap_tracker.clear();
            g_chapter_import=chapter_ui::State{};g_display_menu.open=false;
            stop_thread(); in_game = 0; import_prompt = false; VMS_PLAT(g_exit_sheet) = false; VMS_PLAT(g_sheet_paused_here) = false; VMS_PLAT(g_sheet_swallow) = 0; status = VMS_EXEC(g_error);
            g_menu_note_error=true;g_menu_note_neutral=false;g_menu_repeat.reset();
            draw_menu(items, sel, status); log_line("EX back-to-menu " + VMS_EXEC(g_error)); present(); continue;
        }
        menu_stall::main(menu_stall::Stage::Focus);
        const bool focused=appletGetFocusState()==AppletFocusState_InFocus;
        menu_stall::menu(!in_game&&focused);
        if(renderer_focused&&!focused)host_render_lifecycle::barrier();
        renderer_focused=focused;
        menu_stall::main(menu_stall::Stage::TitleEnable);g_title_service.enable(!in_game&&focused);
        if(in_game && !focused && !focus_paused) {focus_paused=true;host_freeze236(true);}
        if(in_game && focused && focus_paused) {focus_paused=false;if(!g_keymap_editor.open && !VMS_PLAT(g_exit_sheet))host_freeze236(false);}
        host_lifecycle_poll242();
        if(in_game && VMS_EXEC(g_midlet_state)==MidletState::Destroyed) {
            reset_game_text_input();
            in_game=0;focus_paused=false;g_keymap_editor.open=false;g_data_editor.open=false;g_data_session.reset();g_keymap_tracker.clear();VMS_PLAT(g_exit_sheet)=false;VMS_PLAT(g_sheet_paused_here)=false;VMS_PLAT(g_sheet_swallow)=0;items=scan_games();
            if(VMS_EXEC(g_self_exit)) {   // 0.2.112 F2: neutral one-line hint, measured with the menu font at the status-bar size
                VMS_EXEC(g_self_exit)=false;
                status=self_exit_hint::status(VMS_EXEC(g_last_console_line),[](const std::string& s){
                    const auto measured=VMS_PLAT(g_host_font).measure(s,host_menu::status_px);
                    return measured.ok?measured.x+measured.w:host_menu::text_advance(s,host_menu::status_px);},host_menu::status_width);
                g_menu_note_error=false;g_menu_note_neutral=true;log_line("SELFEXIT menu_hint="+status);
            }
            if(!g_canvas_notice.empty()){status=g_canvas_notice;g_menu_note_error=false;g_menu_note_neutral=true;}
            sel=std::max(0,std::min(sel,std::max(0,int(items.size())-1)));draw_menu(items,sel,status);
        }
        if(!focused) {menu_stall::menu(false);g_menu_repeat.reset();if(g_chapter_import.open){g_chapter_blocked=~u64(0);}g_keymap_editor.choice_repeat.reset();{ P7_SCOPE(Idle); svcSleepThread(10000000); }continue;}
        if(in_game)apply_pending_game_text();
        menu_stall::main(menu_stall::Stage::Input);padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        u64 up = padGetButtonsUp(&pad);
        u64 held = padGetButtons(&pad);
        HidTouchScreenState launcher_touch{};hidGetTouchScreenStates(&launcher_touch,1);
        const bool launcher_touch_down=launcher_touch.count>0&&!launcher_touch_held;
        launcher_touch_held=launcher_touch.count>0;
        g_keymap_blocked &= held;
        int ks=0;
        if(in_game && !g_keymap_editor.open && !VMS_PLAT(g_exit_sheet))
            for(int code:mapped_codes(held)){int action=game_key_action(code);if(action)ks|=1<<action;}
        VMS_PLAT(g_key_states)=(VMS_PLAT(g_host_paused)||g_keymap_editor.open||VMS_PLAT(g_exit_sheet)||VMS_EXEC(g_midlet_state)==MidletState::Paused)?0:ks;
        if (in_game && ks != last_ks) {
            log_line("KEY state mask=" + std::to_string(ks) + " held=" + (held ? button_names(held) : std::string("none")));
            last_ks = ks;
        }
        if (!in_game) last_ks = -1;
        if (down) {
            bool stick = down & (HidNpadButton_StickL | HidNpadButton_StickR | HidNpadButton_StickLLeft | HidNpadButton_StickLRight |
                                 HidNpadButton_StickLUp | HidNpadButton_StickLDown);
            bool dpad = down & (HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_Left | HidNpadButton_Right);
            log_line(std::string("KEY ") + (in_game ? "game" : "menu") + " down=" + button_names(down) +
                     (stick && !dpad ? " stick-only" : "") + " sel=" + std::to_string(sel));
        }
        if(!in_game && g_display_menu.open) {
            update_display_menu(down,held,items,sel,status);
            present();{P7_SCOPE(Idle);svcSleepThread(16000000);}continue;
        }
        if(!in_game && g_menu_delete.open) {
            g_menu_repeat.reset();update_menu_delete(down,items,sel,status,held);
            present();{P7_SCOPE(Idle);svcSleepThread(16000000);}continue;
        }
        if (!in_game && g_chapter_import.open) {
            update_chapter_import(down,held,items,sel,status);
            present();{P7_SCOPE(Idle);svcSleepThread(16000000);}continue;
        }
        if (!in_game && import_prompt) {
            g_menu_repeat.reset();
            if (down & (HidNpadButton_B | HidNpadButton_Y)) {
                import_prompt = false; draw_menu(items, sel, status);
            } else if ((down & HidNpadButton_X) && !items.empty()) {
                import_prompt = false;
                log_line("LAUNCH explicit legacy import " + items[sel].label);
                launch_entry(items[sel], true); status=g_canvas_notice; in_game = 1; VMS_EXEC(g_slices) = 0;
            }
            present(); continue;
        }
        if(in_game && g_keymap_editor.open) {
            if(update_keymap_editor(down,held,focus_paused)) {
                lifecycle_destroy();in_game=0;focus_paused=false;items=scan_games();
                sel=std::min(sel,std::max(0,int(items.size())-1));draw_menu(items,sel,status);
            }
            present();{P7_SCOPE(Idle);svcSleepThread(16000000);}continue;
        }
        if(in_game && !VMS_PLAT(g_exit_sheet) && (down&HidNpadButton_StickL)) {
            open_keymap(held);present();continue;
        }
        if (!in_game) {
            // Touch and controller tabs share the exact rectangles drawn by host_menu.
            const int tx=launcher_touch.count?int(launcher_touch.touches[0].x):-1;
            const int ty=launcher_touch.count?int(launcher_touch.touches[0].y):-1;
            if(update_launcher_navigation(down,launcher_touch_down,tx,ty,items,sel,status,
                [](const std::string& s,int px){const auto m=VMS_PLAT(g_host_font).measure(s,px);
                    return m.ok&&m.advance>0?m.advance:host_menu::text_advance(s,px);})){present();continue;}
            // Main-menu library navigation never changes the full game catalog.
            const bool up_held=held&HidNpadButton_AnyUp,down_held=held&HidNpadButton_AnyDown;
            const bool page_back_held=held&(HidNpadButton_L|HidNpadButton_ZL),page_next_held=held&(HidNpadButton_R|HidNpadButton_ZR);
            const int direction=VMS_PLAT(g_menu_focus)!=host_menu::Focus::Games?0:
                up_held!=down_held?(down_held?1:-1):
                !up_held&&!down_held&&page_back_held!=page_next_held?(page_next_held?host_menu::visible_rows:-host_menu::visible_rows):0;
            const int repeated=g_menu_repeat.update(direction,monotonic_ms());
            if(VMS_PLAT(g_menu_focus)==host_menu::Focus::Games) {
                down&=~(HidNpadButton_AnyUp|HidNpadButton_AnyDown|HidNpadButton_L|HidNpadButton_R|HidNpadButton_ZL|HidNpadButton_ZR);
                if(repeated==host_menu::visible_rows)down|=HidNpadButton_R;
                else if(repeated==-host_menu::visible_rows)down|=HidNpadButton_L;
                else if(repeated)down|=repeated>0?HidNpadButton_Down:HidNpadButton_Up;
            }
            bool changed = down && !status.empty(), settings_changed = false;
            if (down) {status.clear();g_menu_note_error=false;g_menu_note_neutral=true;}
            // Horizontal navigation only switches panes; only A changes a setting.
            // Consume horizontal presses before A, including simultaneous presses.
            if (down & HidNpadButton_B) {
                changed = changed || VMS_PLAT(g_menu_focus) != host_menu::Focus::Games;
                VMS_PLAT(g_menu_focus) = host_menu::Focus::Games;
            }
            else if (down & HidNpadButton_AnyLeft) {
                changed = changed || VMS_PLAT(g_menu_focus) != host_menu::Focus::Games;
                VMS_PLAT(g_menu_focus) = host_menu::Focus::Games;
            }
            else if ((down & HidNpadButton_Plus)) {
                g_menu_repeat.reset();
                g_display_menu.begin();
                draw_menu(items,sel,status);present();continue;
            }
            else if (down & HidNpadButton_X) {
                if(VMS_DIAG(g_p1_profile).active)status="P1 测试请正常启动游戏";
                else if (items.empty()) status = "先选择一款游戏";
                else if (!instant_state_exists(items[sel].path)) status = "没有即时存档";
                else {
                    menu_stall::menu(false);select_display_preferences(items[sel].path,true);
                    VMS_PLAT(g_launched_path) = items[sel].path;
                    VMS_PLAT(g_host_surface) = HostSurface::Game;
                    VMS_PLAT(g_cpu_cleared) = 0;
                    VMS_PLAT(g_fps_reset) = true;
                    VMS_EXEC(g_speed_cap) = VMS_DIAG(g_p1_profile).active?0:speed_keys::lookup(g_speed_keys, VMS_PLAT(g_game_speeds), items[sel].path);
                    VMS_EXEC(g_speed_last_paint_us) = 0;
                    VMS_EXEC(g_speed_next_us) = 0;
                    VMS_EXEC(g_speed_slept_us) = 0;
                    VMS_EXEC(g_speed_frame_open) = false;
                    log_line(std::string("CONTINUE begin ") + items[sel].path + " bezel=" + std::to_string(VMS_PLAT(g_menu_settings).bezel) +
                             " fps=" + std::to_string(VMS_PLAT(g_menu_settings).show_fps) + " speed=" + host_speed::log_name(VMS_EXEC(g_speed_cap)));
                    g_title_service.enable(false);
                    VMS_CL(g_jar_files).clear(); VMS_CL(g_props).clear();
                    std::vector<uint8_t> bytes;
                    std::string err;
                    if (!read_whole(items[sel].path, bytes) || !unzip(bytes, VMS_CL(g_jar_files), err)) status = "即时存档无法读取";
                    else {
                        unpack_nested_jars();
                        load_props();
                        load_sidecar_jad(items[sel].path);
                        VMS_CL(g_classpath).clear();
                        VMS_CL(g_classpath).push_back("romfs:/classes");
                        const std::string identity=rms_jar_identity(bytes.data(),bytes.size());
                        select_screen_sidecar(items[sel].path,identity);
                        select_game_keymap(items[sel],identity);
                        if(!chapter_save::recover(g_chapter_rms_root,identity,err)) status="存档恢复失败："+err;
                        else if (restore_instant_state(items[sel].path,identity)) {
                            in_game = 1; VMS_EXEC(g_slices) = 0; status=g_canvas_notice; changed = false;
                            VMS_EXEC(g_last_console_line).clear(); VMS_EXEC(g_self_exit) = false;   // 0.2.112 F2: per-continue
                            VMS_PLAT(g_chrome_bezel) = -2; VMS_PLAT(g_chrome_fps) = -2; VMS_PLAT(g_cpu_cleared) = 0; VMS_RS(g_fb_dirty) = true;
                            log_line("CONTINUE host bezel=" + std::to_string(VMS_PLAT(g_menu_settings).bezel) +
                                     " show_fps=" + std::to_string(VMS_PLAT(g_menu_settings).show_fps ? 1 : 0) +
                                     " speed=" + host_speed::log_name(VMS_EXEC(g_speed_cap)));
                        } else status = "即时存档无法读取（版本不兼容或文件损坏）";
                    }
                }
                if (!in_game) {changed = true;g_menu_note_error=status!="没有即时存档" && status!="先选择一款游戏" && !VMS_DIAG(g_p1_profile).active;g_menu_note_neutral=!g_menu_note_error;}
                log_line("CONTINUE status=" + status + " in_game=" + std::to_string(in_game));
            } else if (down & HidNpadButton_AnyRight) {
                changed = changed || VMS_PLAT(g_menu_focus) == host_menu::Focus::Games;
                VMS_PLAT(g_menu_focus) = VMS_PLAT(g_menu_option_focus);
            } else if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Games) {
                if ((down & HidNpadButton_AnyDown) && !items.empty()) { sel=menu_actions::move(sel,1,int(items.size())); changed = true; }
                else if ((down & HidNpadButton_AnyUp) && !items.empty()) { sel=menu_actions::move(sel,-1,int(items.size())); changed = true; }
                else if ((down & (HidNpadButton_R | HidNpadButton_ZR)) && !items.empty()) {
                    sel=menu_actions::page(sel,1,int(items.size()),host_menu::visible_rows); changed = true;
                } else if ((down & (HidNpadButton_L | HidNpadButton_ZL)) && !items.empty()) {
                    sel=menu_actions::page(sel,-1,int(items.size()),host_menu::visible_rows); changed = true;
                } else if ((down & HidNpadButton_A) && !items.empty()) {
                    log_line("LAUNCH begin " + items[sel].label + " path=" + items[sel].path);
                    launch_entry(items[sel]); status=g_canvas_notice; in_game = 1; VMS_EXEC(g_slices) = 0;
                    log_line("LAUNCH entered " + items[sel].label);
                }
            } else if (down & (HidNpadButton_AnyUp | HidNpadButton_AnyDown)) {
                VMS_PLAT(g_menu_focus) = host_menu::option_step(VMS_PLAT(g_menu_focus),(down & HidNpadButton_AnyDown)?1:-1);
                VMS_PLAT(g_menu_option_focus) = VMS_PLAT(g_menu_focus);
                changed = true;
            } else if (down & HidNpadButton_A) {
                if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Bezel) {
                    VMS_PLAT(g_menu_settings).bezel = host_menu::next_bezel(VMS_PLAT(g_menu_settings).bezel);
                    settings_changed = true;
                } else if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Filter) {
                    change_filter_menu_setting(status);
                } else if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Rotation && !items.empty()) {
                    auto chosen=VMS_PLAT(g_menu_settings);
                    host_menu::change_rotation(chosen,1);
                    const bool saved=host_display::save_rotation(items[sel].path,chosen.rotation);
                    if(saved){VMS_PLAT(g_menu_settings).rotation=chosen.rotation;invalidate_display_preferences();}
                    status=saved?std::string("此游戏旋转 ")+host_menu::rotation_name(chosen.rotation):"旋转保存失败，请检查游戏目录";
                    g_menu_note_error=!saved;g_menu_note_neutral=false;
                    log_line("DISPLAY rotation="+std::to_string(chosen.rotation*90)+" saved="+std::to_string(saved)+" path="+items[sel].path);
                } else if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Fps) {
                    VMS_PLAT(g_menu_settings).show_fps = !VMS_PLAT(g_menu_settings).show_fps; settings_changed = true;
                } else if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Speed && !items.empty()) {
                    if(VMS_DIAG(g_p1_profile).active){status="P1 测试固定 Native";log_line("P1CONFIG speed requested_change=1 effective=native source=P1_profile");}
                    else {
                    const std::string& path = items[sel].path;
                    const std::string skey = speed_keys::key_for(g_speed_keys, path);
                    const int chosen = host_speed::next(speed_keys::lookup(g_speed_keys, VMS_PLAT(g_game_speeds), path));
                    mkdir("sdmc:/switch/j2me", 0777);
                    bool saved = false;
                    if (!skey.empty()) { VMS_PLAT(g_game_speeds)[skey] = chosen; saved = speed_keys::rewrite(g_speed_keys, VMS_PLAT(g_game_speeds)); speed_keys::save_cache(g_speed_keys); }
                    status = items[sel].label + " 速度 " + host_speed::label(chosen) + (saved ? "" : "（未能写入）");
                    g_menu_note_error=!saved;g_menu_note_neutral=false;
                    log_line(std::string("SPEED set cap=") + host_speed::log_name(chosen) +
                             " saved=" + std::to_string(saved) + " path=" + path + " key=" + skey);
                    }
                } else status = "先选择一款游戏";
                changed = true;
            }
            if(settings_changed) {
                const bool saved=save_menu_settings();
                status=saved?"设置保存成功":"设置保存失败，本次运行仍然生效";
                g_menu_note_error=!saved;g_menu_note_neutral=false;
            }
            if (!in_game && changed) draw_menu(items, sel, status);
            // P8.11: an over-wide selected name scrolls; repaint the menu at ~30 Hz only then.
            else if (!in_game && g_menu_name_ui.scrolling && monotonic_ms() - g_menu_name_ui.last_tick >= 33) {
                g_menu_name_ui.last_tick = monotonic_ms(); VMS_PLAT(g_cpu_cleared) = 0;
            }
        } else {
            g_menu_repeat.reset();
            if(try_game_text_input(down,held,focus_paused)){present();continue;}
            g_keymap_tracker.update(mapped_codes(held),[](int code,bool release){dispatch_key(code,release);});
            // Different phone codes can share a GameCanvas action (e.g. Up and 2).
            if(Value* key_held=game_canvas_state(VMS_RS(g_current),"keyHeld"))key_held->i=VMS_PLAT(g_key_states);

        }
        if (!in_game && !import_prompt && !g_menu_delete.open && !g_display_menu.open) poll_menu_name(items,sel);
        if (in_game && VMS_PLAT(g_exit_sheet)) {
            { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(16)); }
        } else if (in_game) {
            if(!sched_legacy()) {
                // P2.6: shared scheduler core; this layer only supplies frame acquisition and host sleeps.
                sched_game_work(VMS_PLAT(g_gpu_active),acquire_game_frame,[](int64_t ms){
                    // idle nap (scheduled-frame wake waits are wait_game_wake inside the core, as before).
                    { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }});
            } else {
                speed_wait_paced_sleep();
                // While the last presented frame is still current and Java is mid-update,
                // run bytecode before taking a framebuffer. A finished paint sets
                // VMS_RS(g_fb_dirty) and is shown below; 8..20ms sleeps still use the vsync path.
                const int64_t pace_now = paint_clock_us();
                const bool present_due = VMS_PLAT(g_last_present_us) != 0 && pace_now - VMS_PLAT(g_last_present_us) >= 50000;
                p1_host_work_turn(VMS_PLAT(g_gpu_active),present_due,[](bool){
                    scheduled_frame(acquire_game_frame,game_wake_delay,wait_game_wake,advance_game_frame,
                        VMS_EXEC(g_frame_wait_budget),paint_clock_us,&VMS_EXEC(g_wake_stats));
                },[](int64_t ms){{ P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }});
            }
            maybe_emit_runtime_stats();
        }
        if(!sched_legacy()) {
            sched_present_turn(in_game,VMS_PLAT(g_exit_sheet),VMS_PLAT(g_gpu_active),VMS_PLAT(g_cpu_cleared),[]{CityHostSpan city_host("present");present();});
        } else {
            const bool heartbeat=VMS_PLAT(g_last_present_us)!=0&&paint_clock_us()-VMS_PLAT(g_last_present_us)>=50000;
            if(p1::should_show(in_game,VMS_PLAT(g_exit_sheet),VMS_PLAT(g_gpu_active),VMS_RS(g_fb_dirty),VMS_PLAT(g_cpu_cleared),heartbeat,speed_group_incomplete())){
                {CityHostSpan city_host("present");present();}VMS_PLAT(g_last_present_us)=paint_clock_us();if(in_game&&VMS_RS(g_fb_dirty))++VMS_RS(g_present_frames);  // 0.2.114 F4: only presents carrying a new frame (heartbeat re-presents not counted); 0.2.113: in-game only
                p1_host_post_present(in_game,VMS_PLAT(g_exit_sheet));
            } else if (VMS_EXEC(g_frame_work_finished_us)) {
                VMS_EXEC(g_frame_wait_budget).observe(VMS_EXEC(g_frame_work_us), 0);
                VMS_EXEC(g_frame_work_finished_us) = 0;
            }
        }
    }
    menu_stall::menu(false);
    g_data_editor.open=false;g_data_session.reset();
    g_title_service.stop();
    lifecycle_destroy();
    native_prep_runtime::shutdown();p7::pool().shutdown(); // P7.2
    VMS_PLAT(g_audio_sink).shutdown();
    close_presenter();
    close_host_fonts();
    romfsExit();
    appletUnlockExit();
    p1_report();p1_flush("exit");
    log_line("exit: return 0");
    log_shutdown();
    if (VMS_PLAT(g_log).is_open()) VMS_PLAT(g_log).close();
    return 0;
}
