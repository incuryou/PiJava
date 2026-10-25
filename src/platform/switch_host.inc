// Horizon frontend, input, layered presenter and application lifecycle.
#include <switch.h>
#include <dirent.h>
#include "../n1_paths.h"
#include "../host_menu.h"
#include "../game_names.h"
#include "../menu_catalog.h"
#include "../host_menu_settings.h"
#include "../host_keymap_ui.h"
#include "../controller_skin_labels.h"
static host_keymap::Store g_keymaps;
static host_keymap::Editor g_keymap_editor;
static host_pro::Preview g_keymap_pro;
static host_keymap::Config g_keymap_active;
static host_controller::Legends g_controller_legends;
static bool standard_game_keys();
static host_keymap::Tracker g_keymap_tracker;
static std::string g_keymap_id, g_keymap_name;
static std::vector<uint32_t> g_keymap_phone;
static bool g_keymap_phone_attempted=false;
static u64 g_keymap_blocked=0;
static const char* g_keymap_path="sdmc:/switch/j2me/keymap.cfg";
#include "../host_speed.h"
#include "../speed_keys.h"
static speed_keys::State g_speed_keys; // P8.3: SHA256-keyed speed.cfg
#include "../host_skin.h"
#include "../host_font.h"
#ifdef J2ME_ENABLE_DEKO3D
#include "../switch_gpu_present.h"
 VMS_PLAT_ACCESSOR(g_gpu_present, SwitchGpuPresent)
struct GpuHostLayerCache {
    std::vector<uint32_t> rgba,canvas; uint64_t generation=0;
    int bezel=-1,fps=-1; uint64_t mapping_generation=0; bool was_layered=false; std::string previous_label;
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
static menu_actions::Deletion g_menu_delete;
static menu_actions::Repeat g_menu_repeat;
static bool g_menu_note_error=false,g_menu_note_neutral=true;
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
    if(g_controller_legends.update(g_keymap_active,standard_game_keys(),VMS_PLAT(g_menu_settings).bezel,
        [](const std::string& text,int size){
            const auto measured=VMS_PLAT(g_host_font).measure(text,size);
            return measured.ok?measured.advance:host_menu::text_advance(text,size);
        }))VMS_PLAT(g_cpu_cleared)=0;
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
    VMS_PLAT(g_gpu_present).close();VMS_PLAT(g_gpu_layers).clear();
#endif
    VMS_PLAT(g_gpu_active) = false;
    if(VMS_PLAT(g_cpu_frame) && VMS_PLAT(g_win_ready)) {framebufferEnd(&VMS_PLAT(g_win));VMS_PLAT(g_cpu_frame)=nullptr;}
    VMS_PLAT(g_cpu_stride)=0;VMS_PLAT(g_cpu_acquire_us)=0;
    if(VMS_PLAT(g_win_ready)) { framebufferClose(&VMS_PLAT(g_win)); VMS_PLAT(g_win_ready)=false; }
    VMS_PLAT(g_cpu_cleared)=0;
    VMS_PLAT(g_present_stats)={};
}
static bool toggle_presenter() {
    if(VMS_DIAG(g_p1_profile).active){log_line("P1CONFIG renderer requested_toggle=1 effective=cpu source=P1_profile");return false;}
    const bool request_gpu = !VMS_PLAT(g_gpu_active);
    log_line(std::string("RENDER request=")+(request_gpu?"gpu":"cpu"));
    log_drain();
    close_presenter();
#ifdef J2ME_ENABLE_DEKO3D
    if(request_gpu) {
        VMS_PLAT(g_gpu_active)=VMS_PLAT(g_gpu_present).init();
        if(!VMS_PLAT(g_gpu_active))log_line("RENDER GPU initialization returned failure; CPU selected");
    }
#else
    if(request_gpu)log_line("RENDER GPU unavailable in CPU-only build");
#endif
    log_line(std::string("RENDER backend=")+(VMS_PLAT(g_gpu_active)?"deko3d":"cpu"));
    return VMS_PLAT(g_gpu_active);
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

// Shared host chrome and overlay implementation: CPU and GPU use the same
// menus, phone skins, FPS placement, text metrics and pause sheet.
static void compose_host_surface(u32* frame,u32 pitch,const std::string& label,
                                 bool include_game,bool force_chrome) {
    if (g_keymap_editor.open) {
        host_keymap::render(frame,pitch,g_keymap_editor,g_keymap_phone,host_text_callback(frame,pitch),host_text_callback(frame,pitch,true),
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
                               g_menu_note_error,g_menu_note_neutral);
        host_menu::render_delete(frame,pitch,g_menu_delete,host_text_callback(frame,pitch),host_text_callback(frame,pitch,true));
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
    const int fps_bit = VMS_PLAT(g_menu_settings).show_fps ? 1 : 0;
    if (VMS_PLAT(g_chrome_bezel) != VMS_PLAT(g_menu_settings).bezel || VMS_PLAT(g_chrome_fps) != fps_bit) {
        VMS_PLAT(g_chrome_bezel) = VMS_PLAT(g_menu_settings).bezel;
        VMS_PLAT(g_chrome_fps) = fps_bit;
        VMS_PLAT(g_cpu_cleared) = 0;
        log_line("HOST chrome bezel=" + std::to_string(VMS_PLAT(g_menu_settings).bezel) +
                 " show_fps=" + std::to_string(fps_bit) +
                 " speed=" + host_speed::log_name(VMS_EXEC(g_speed_cap)));
    }
    if (force_chrome || VMS_PLAT(g_cpu_cleared) < 3) {
        VMS_PLAT(g_host_skin).select(VMS_PLAT(g_menu_settings).bezel, [](const std::string& message) { log_line(message); });
        host_menu::render_bezel(frame, pitch, VMS_PLAT(g_menu_settings).bezel, host_text_callback(frame, pitch),
                                VMS_PLAT(g_host_skin).data(), VMS_PLAT(g_host_skin).size());
        if(VMS_PLAT(g_host_skin).data())g_controller_legends.render(frame,pitch,VMS_PLAT(g_menu_settings).bezel,host_text_callback(frame,pitch),host_controller::Context::Game,host_text_callback(frame,pitch,true));
        ++VMS_PLAT(g_cpu_cleared);
    }
    const uint8_t* src = VMS_RS(g_fb).data();
    const size_t fb_need = (size_t)VMS_RS(g_screen_w) * (size_t)VMS_RS(g_screen_h) * 3;
    if (include_game && VMS_RS(g_fb).size() >= fb_need) {
    p7present::blit_game(reinterpret_cast<uint32_t*>(frame), uint32_t(pitch), src, VMS_RS(g_screen_w), VMS_RS(g_screen_h), VMS_PLAT(g_menu_settings).filter); // original retains P7.5 row split
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

static void present() {
    const int64_t present_started=paint_clock_us();
    VMS_PLAT_LOCAL(fps_shown, present_fps_shown, static int fps_shown = 0;)
    VMS_PLAT_LOCAL(fps_seen, present_fps_seen, static uint64_t fps_seen = 0;)
    VMS_PLAT_LOCAL(fps_t0, present_fps_t0, static int64_t fps_t0 = 0;)
    int64_t now = monotonic_ms();
    const uint64_t fps_frames=VMS_RS(g_present_frames);  // 0.2.112 F4: frames delivered to the screen, not paint calls
    if (!fps_t0 || VMS_PLAT(g_fps_reset) || fps_frames < fps_seen) {
        fps_t0 = now; fps_seen = fps_frames; fps_shown = 0; VMS_PLAT(g_fps_reset) = false;
    }
    int64_t dt = now - fps_t0;
    if (dt >= 500) {
        const uint64_t paints = fps_frames - fps_seen;
        fps_shown = (int)((paints * 1000) / (dt > 0 ? dt : 1));
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
            (VMS_PLAT(g_menu_settings).show_fps && previous_label!=label) || VMS_PLAT(g_cpu_cleared)==0;
        if(rebuild) {
            canvas.resize(j2me_present::canvas_pixels);
            compose_host_surface(canvas.data(),1280,label,!layered,true);
            ++generation;bezel=VMS_PLAT(g_menu_settings).bezel;fps=int(VMS_PLAT(g_menu_settings).show_fps);previous_label=label;
            cache.mapping_generation=g_controller_legends.generation();
        }
        was_layered=layered;
        bool valid=!layered || j2me_present::game_rgba(VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h),rgba,VMS_PLAT(g_menu_settings).filter);
        const int64_t prepared=paint_clock_us();
        const bool ok=valid && VMS_PLAT(g_gpu_present).present_layers(reinterpret_cast<const uint8_t*>(canvas.data()),generation,
                        layered?reinterpret_cast<const uint8_t*>(rgba.data()):nullptr);
        const int64_t done=paint_clock_us();const auto timing=VMS_PLAT(g_gpu_present).lastTimings();
        const uint64_t prepare=uint64_t(prepared-present_started)+timing.upload_us;
        const uint64_t total=uint64_t(done-present_started);
        const uint64_t accounted=timing.acquire_wait_us+prepare+timing.gpu_wait_us;
        VMS_PLAT(g_present_stats).add(total,timing.acquire_wait_us,prepare,total>accounted?total-accounted:0,timing.gpu_wait_us);
        if(ok) {
            // Same scheduler feedback contract as CPU presentation.
            if(VMS_EXEC(g_frame_work_finished_us)){VMS_EXEC(g_frame_wait_budget).observe(VMS_EXEC(g_frame_work_us),done-VMS_EXEC(g_frame_work_finished_us));VMS_EXEC(g_frame_work_finished_us)=0;}
            return;
        }
        log_line("RENDER GPU submission returned failure; falling back to CPU");close_presenter();
    }
#endif
    { P7_SCOPE(PresentWait); acquire_cpu_frame(); }
    P7_SCOPE(Present);
    const int64_t copy_started=paint_clock_us();
    u32* frame=VMS_PLAT(g_cpu_frame);
    const u32 stride_bytes=VMS_PLAT(g_cpu_stride);
    u32 pitch = stride_bytes / 4;
    compose_host_surface(frame,pitch,label,true,false);
    const int64_t prepared=paint_clock_us();
    framebufferEnd(&VMS_PLAT(g_win));VMS_PLAT(g_cpu_frame)=nullptr;VMS_PLAT(g_cpu_stride)=0;
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
static std::vector<Entry> scan_games() {
    std::vector<Entry> items;
    for (auto& g : n1paths::scan_games("sdmc:/switch/j2me")) items.push_back({g.label, g.path});
    log_line("GAMES scan=games_only dir=sdmc:/switch/j2me/games count=" + std::to_string(items.size()));
    g_menu_name_ui.catalog.dirty = true;
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
    VMS_PLAT(g_host_surface) = HostSurface::Menu;
    VMS_PLAT(g_cpu_cleared) = 0;
    VMS_PLAT(g_menu_selected) = std::max(0, std::min(sel, std::max(0, int(items.size()) - 1)));
    VMS_PLAT(g_menu_status) = status;
    auto& catalog = g_menu_name_ui.catalog;
    if (catalog.dirty) {
        catalog.rebuild(items, VMS_PLAT(g_menu_labels), VMS_PLAT(g_menu_paths), VMS_PLAT(g_menu_saved));
        g_menu_name_ui.last_sel = -1;
    }
    if (VMS_PLAT(g_menu_selected) != g_menu_name_ui.last_sel) { g_menu_name_ui.last_sel = VMS_PLAT(g_menu_selected); g_menu_name_ui.scroll_t0 = monotonic_ms(); }
    catalog.prepare_page(VMS_PLAT(g_menu_paths), VMS_PLAT(g_menu_saved), VMS_PLAT(g_menu_selected), host_menu::visible_rows, instant_state_exists);
    catalog.select(VMS_PLAT(g_menu_paths), VMS_PLAT(g_menu_selected), monotonic_ms());
}
static void poll_menu_name() {
    const int64_t start = paint_clock_us();
    auto& catalog = g_menu_name_ui.catalog;
    if (!catalog.poll_name(VMS_PLAT(g_menu_paths), monotonic_ms(),g_menu_repeat.browsing)) return;
    g_menu_name_ui.scroll_t0 = monotonic_ms();
    VMS_PLAT(g_cpu_cleared) = 0;
    log_line("MENUNAME read=1 entries=" + std::to_string(VMS_PLAT(g_menu_paths).size()) +
             " fallback_total=" + std::to_string(catalog.names.misses) + " cached=" + std::to_string(catalog.names.names.size()) +
             " us=" + std::to_string(paint_clock_us() - start));
}
static void draw_import_prompt(const Entry& entry) {
    VMS_PLAT(g_host_surface) = HostSurface::Import;
    VMS_PLAT(g_cpu_cleared) = 0;
    VMS_PLAT(g_import_label) = entry.label;
}
static void update_menu_delete(u64 down,std::vector<Entry>& items,int& sel,std::string& status) {
    using menu_actions::Action;using menu_actions::Result;
    Result result=Result::None;
    if(down&HidNpadButton_B)result=g_menu_delete.act(Action::Back);
    else if(down&HidNpadButton_AnyUp)result=g_menu_delete.act(Action::Up);
    else if(down&HidNpadButton_AnyDown)result=g_menu_delete.act(Action::Down);
    else if(down&HidNpadButton_A)result=g_menu_delete.act(Action::Confirm);
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
    std::map<std::string,std::string> display_props;
    for(const char* key:{"MIDlet-Name","MIDlet-1"}){auto it=VMS_CL(g_props).find(key);if(it!=VMS_CL(g_props).end())display_props.emplace(it->first,it->second);}
    g_keymap_id=id;g_keymap_name=game_names::choose_name(display_props);
    if(g_keymap_name.empty())g_keymap_name=game_names::strip_jar(entry.path);
    g_keymap_active=host_keymap::lookup(g_keymaps,id);g_keymap_tracker.clear();g_keymap_blocked=~u64(0);
    g_controller_legends.invalidate();
    log_line("KEYMAP game="+g_keymap_name+" id="+id+" source="+(g_keymaps.games.count(id)?std::string("game"):std::string("global"))+" auto="+std::to_string(g_keymap_active.automatic));
}
static void close_keymap(u64 held,bool focus_paused) {
    const bool was_paused=g_keymap_editor.was_paused;
    g_keymap_editor.open=false;g_keymap_tracker.clear();g_keymap_blocked|=held;
    if(!was_paused && !focus_paused)host_freeze236(false);
    VMS_PLAT(g_cpu_cleared)=0;VMS_RS(g_fb_dirty)=true;VMS_PLAT(g_fps_reset)=true;
    log_line("KEYMAP close blocked_until_release=1");
}
static void open_keymap(u64 held) {
    const bool paused=VMS_PLAT(g_host_paused);
    host_freeze236(true);g_keymap_tracker.clear();g_keymap_blocked|=held;
    g_keymap_editor.begin(g_keymaps,g_keymap_id,g_keymap_name,standard_game_keys(),paused);
    VMS_PLAT(g_cpu_cleared)=0;
    log_line("L3MENU open page=save_exit L3_reserved=1 game="+g_keymap_name);
}
static bool update_keymap_editor(u64 down,u64 held,bool focus_paused) {
    const auto previous_page=g_keymap_editor.page;
    using host_keymap::Action;host_keymap::Result result=host_keymap::Result::None;
    const u64 directions=HidNpadButton_AnyUp|HidNpadButton_AnyDown|HidNpadButton_AnyLeft|HidNpadButton_AnyRight;
    const u64 arrows=(held&directions)?held:down;
    const bool forward=arrows&(HidNpadButton_AnyDown|HidNpadButton_AnyRight),backward=arrows&(HidNpadButton_AnyUp|HidNpadButton_AnyLeft);
    const int direction=forward!=backward?(forward?1:-1):0;
    const bool choosing=g_keymap_editor.open&&g_keymap_editor.page==host_keymap::Page::Keymap&&g_keymap_editor.phase==host_keymap::Phase::Choice;
    const bool command=down&(HidNpadButton_StickL|HidNpadButton_L|HidNpadButton_R|HidNpadButton_Plus|HidNpadButton_B|HidNpadButton_X|HidNpadButton_Y|HidNpadButton_A);
    const int repeated=g_keymap_editor.choice_repeat.update(choosing,direction,bool(down&directions),monotonic_ms(),command);
    if(down&HidNpadButton_StickL)result=host_keymap::act(g_keymap_editor,Action::Close);
    else if(down&HidNpadButton_L)result=host_keymap::act(g_keymap_editor,Action::PreviousPage);
    else if(down&HidNpadButton_R)result=host_keymap::act(g_keymap_editor,Action::NextPage);
    else if(down&HidNpadButton_Plus)result=host_keymap::act(g_keymap_editor,Action::Save);
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
    if(g_keymap_editor.page==host_keymap::Page::Keymap && !g_keymap_phone_attempted) {
        g_keymap_phone_attempted=true;g_keymap_phone=host_keymap::load_phone("romfs:/ui/nokia5300.rgba");
        log_line("KEYMAP phone_loaded="+std::to_string(!g_keymap_phone.empty()));
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
    log_line("PROG L3=pause/resume; Y=destroyApp and menu; Java exception traces enabled");
#ifdef J2ME_ENABLE_DEKO3D
    log_line("RENDER backend=cpu; menu L3 toggles experimental GPU layered-ring presentation (all skins)");
#else
    log_line("RENDER backend=cpu; GPU work paused for compatibility release");
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
    static std::vector<Entry> items = scan_games();
    VMS_PLAT_LOCAL3(sel, main_sel, in_game, main_in_game, last_ks, main_last_ks, static int sel = 0, in_game = 0, last_ks = -1;)
    VMS_PLAT_LOCAL(import_prompt, main_import_prompt, static bool import_prompt = false;)
    bool focus_paused=false;
    std::string status;
    int sd_jars = 0; for (auto& it : items) if (it.path.find("sdmc:") == 0) sd_jars++;
    log_line("MENU items=" + std::to_string(items.size()) + " sd_jars=" + std::to_string(sd_jars));
    VMS_CL(g_classpath).push_back("romfs:/classes");
    const bool keymaps_loaded=host_keymap::load(g_keymap_path,g_keymaps);
    log_line("KEYMAP config_loaded="+std::to_string(keymaps_loaded)+" per_game="+std::to_string(g_keymaps.games.size())+" L3_reserved=1 Y_reserved=0 inputs=23");
    const bool config_loaded = host_menu::load_settings("sdmc:/switch/j2me/menu.cfg", VMS_PLAT(g_menu_settings));
    const bool speeds_loaded = host_speed::load("sdmc:/switch/j2me/speed.cfg", VMS_PLAT(g_game_speeds));
    g_speed_keys.dir = "sdmc:/switch/j2me"; speed_keys::load_cache(g_speed_keys);
    { const auto mr = speed_keys::migrate(g_speed_keys, VMS_PLAT(g_game_speeds)); speed_keys::save_cache(g_speed_keys);
      log_line("SPEEDKEY key=sha256 migrated=" + std::to_string(mr.migrated) + " kept_unresolved=" + std::to_string(mr.kept) +
               " rewritten=" + std::to_string(mr.rewritten) + " bak_written=" + std::to_string(mr.bak_written) + " save_ok=" + std::to_string(mr.save_ok) +
               " hashed=" + std::to_string(g_speed_keys.hashed) + " cache_hits=" + std::to_string(g_speed_keys.hits) + " cache_entries=" + std::to_string(g_speed_keys.cache.size())); }
    log_line("MENU native=1280x720 viewport=480x640 speed=per-game native/30/60 settings_loaded=" + std::to_string(config_loaded) +
             " speeds_loaded=" + std::to_string(speeds_loaded) + " speeds=" + std::to_string(VMS_PLAT(g_game_speeds).size()));
    draw_menu(items, sel, status);
    static AppletHookCookie p8_exit_cookie; // P8.2: HOME->close / exit request writes the last P7COUNT summary
    appletHook(&p8_exit_cookie, [](AppletHookType t, void*) { if (t == AppletHookType_OnExitRequest) p8_exit_summary(); }, nullptr);
    g_die_catch = true;
    while (appletMainLoop()) {
        if (setjmp(g_die_jmp) != 0) {
            g_keymap_editor.open=false;g_keymap_tracker.clear();
            stop_thread(); in_game = 0; import_prompt = false; VMS_PLAT(g_exit_sheet) = false; VMS_PLAT(g_sheet_paused_here) = false; VMS_PLAT(g_sheet_swallow) = 0; status = VMS_EXEC(g_error);
            g_menu_note_error=true;g_menu_note_neutral=false;g_menu_repeat.reset();
            draw_menu(items, sel, status); log_line("EX back-to-menu " + VMS_EXEC(g_error)); present(); continue;
        }
        const bool focused=appletGetFocusState()==AppletFocusState_InFocus;
        if(in_game && !focused && !focus_paused) {focus_paused=true;host_freeze236(true);}
        if(in_game && focused && focus_paused) {focus_paused=false;if(!g_keymap_editor.open && !VMS_PLAT(g_exit_sheet))host_freeze236(false);}
        host_lifecycle_poll242();
        if(in_game && VMS_EXEC(g_midlet_state)==MidletState::Destroyed) {
            in_game=0;focus_paused=false;g_keymap_editor.open=false;g_keymap_tracker.clear();VMS_PLAT(g_exit_sheet)=false;VMS_PLAT(g_sheet_paused_here)=false;VMS_PLAT(g_sheet_swallow)=0;items=scan_games();
            if(VMS_EXEC(g_self_exit)) {   // 0.2.112 F2: neutral one-line hint, measured with the menu font at the status-bar size
                VMS_EXEC(g_self_exit)=false;
                status=self_exit_hint::status(VMS_EXEC(g_last_console_line),[](const std::string& s){
                    const auto measured=VMS_PLAT(g_host_font).measure(s,host_menu::status_px);
                    return measured.ok?measured.x+measured.w:host_menu::text_advance(s,host_menu::status_px);},host_menu::status_width);
                g_menu_note_error=false;g_menu_note_neutral=true;log_line("SELFEXIT menu_hint="+status);
            }
            sel=std::max(0,std::min(sel,std::max(0,int(items.size())-1)));draw_menu(items,sel,status);
        }
        if(!focused) {g_keymap_editor.choice_repeat.reset();{ P7_SCOPE(Idle); svcSleepThread(10000000); }continue;}
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        u64 up = padGetButtonsUp(&pad);
        u64 held = padGetButtons(&pad);
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
        if(!in_game && g_menu_delete.open) {
            g_menu_repeat.reset();update_menu_delete(down,items,sel,status);
            present();{P7_SCOPE(Idle);svcSleepThread(16000000);}continue;
        }
        if (!in_game && import_prompt) {
            g_menu_repeat.reset();
            if (down & (HidNpadButton_B | HidNpadButton_Y)) {
                import_prompt = false; draw_menu(items, sel, status);
            } else if ((down & HidNpadButton_X) && !items.empty()) {
                import_prompt = false;
                log_line("LAUNCH explicit legacy import " + items[sel].label);
                launch_entry(items[sel], true); in_game = 1; VMS_EXEC(g_slices) = 0;
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
            // Games plus bezel, filter, per-game speed cap and FPS options.
            // Minus retains the legacy-import flow as a secondary shortcut.
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
            else if ((down & HidNpadButton_Plus) && VMS_PLAT(g_menu_focus)==host_menu::Focus::Games) {
                g_menu_repeat.reset();
                if(items.empty())status="先选择一款游戏";
                else g_menu_delete.begin(items[sel].path,game_names::strip_jar(items[sel].path));
                draw_menu(items,sel,status);present();continue;
            }
            else if (down & HidNpadButton_X) {
                if(VMS_DIAG(g_p1_profile).active)status="P1 测试请正常启动游戏";
                else if (items.empty()) status = "先选择一款游戏";
                else if (!instant_state_exists(items[sel].path)) status = "没有即时存档";
                else {
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
                        if (restore_instant_state(items[sel].path,identity)) {
                            in_game = 1; VMS_EXEC(g_slices) = 0; status.clear(); changed = false;
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
                } else if ((down & HidNpadButton_Minus) && !items.empty()) {
                    import_prompt = true; draw_import_prompt(items[sel]); present(); continue;
                } else if ((down & HidNpadButton_A) && !items.empty()) {
                    log_line("LAUNCH begin " + items[sel].label + " path=" + items[sel].path);
                    launch_entry(items[sel]); in_game = 1; VMS_EXEC(g_slices) = 0;
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
                    settings_changed=host_menu::change_filter(VMS_PLAT(g_menu_settings),1);
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
            g_keymap_tracker.update(mapped_codes(held),[](int code,bool release){dispatch_key(code,release);});
            // Different phone codes can share a GameCanvas action (e.g. Up and 2).
            if(Value* key_held=game_canvas_state(VMS_RS(g_current),"keyHeld"))key_held->i=VMS_PLAT(g_key_states);

        }
        if (!in_game && !import_prompt && !g_menu_delete.open) poll_menu_name();
        if (in_game && VMS_PLAT(g_exit_sheet)) {
            { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(16)); }
        } else if (in_game) {
            if(!sched_legacy()) {
                // P2.6: shared scheduler core; this layer only supplies frame acquisition and host sleeps.
                sched_game_work(VMS_PLAT(g_gpu_active),acquire_cpu_frame,[](int64_t ms){
                    // idle nap (scheduled-frame wake waits are wait_game_wake inside the core, as before).
                    { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }});
            } else {
                speed_wait_paced_sleep();
                // While the last presented frame is still current and Java is mid-update,
                // run bytecode before taking a framebuffer. A finished paint sets
                // VMS_RS(g_fb_dirty) and is shown below; 8..20ms sleeps still use the vsync path.
                const int64_t pace_now = paint_clock_us();
                const bool present_due = VMS_PLAT(g_last_present_us) != 0 && pace_now - VMS_PLAT(g_last_present_us) >= 50000;
                p1_host_work_turn(VMS_PLAT(g_gpu_active),present_due,[](bool gpu){
                    if(gpu)scheduled_frame(acquire_cpu_frame,game_wake_delay,wait_game_wake,advance_game_frame,0);
                    else scheduled_frame(acquire_cpu_frame,game_wake_delay,wait_game_wake,advance_game_frame,
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
    lifecycle_destroy();
    p7::pool().shutdown(); // P7.2
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
