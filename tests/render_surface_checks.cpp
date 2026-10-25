// P2.1-c6: RenderSurface + Display migration checks (desktop): single storage for every moved field,
// C09 state machine contract (P2 = single CPU_VALID state), g_host_skin now in platform storage (switch only).
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
    CHECK(&VMS_RS(g_screen_w) == &g_vm_storage.render.g_screen_w && inside(VMS_RS(g_screen_w)), "render.g_screen_w single storage");
    CHECK(&VMS_RS(g_screen_h) == &g_vm_storage.render.g_screen_h && inside(VMS_RS(g_screen_h)), "render.g_screen_h single storage");
    CHECK(&VMS_RS(g_fb) == &g_vm_storage.render.g_fb && inside(VMS_RS(g_fb)), "render.g_fb single storage");
    CHECK(&VMS_RS(g_shown) == &g_vm_storage.render.display.g_shown && inside(VMS_RS(g_shown)), "render.display.g_shown single storage");
    CHECK(&VMS_RS(g_alert_return) == &g_vm_storage.render.display.g_alert_return && inside(VMS_RS(g_alert_return)), "render.display.g_alert_return single storage");
    CHECK(&VMS_RS(g_alert_deadline) == &g_vm_storage.render.display.g_alert_deadline && inside(VMS_RS(g_alert_deadline)), "render.display.g_alert_deadline single storage");
    CHECK(&VMS_RS(g_alert_remaining) == &g_vm_storage.render.display.g_alert_remaining && inside(VMS_RS(g_alert_remaining)), "render.display.g_alert_remaining single storage");
    CHECK(&VMS_RS(g_paint_stats) == &g_vm_storage.render.g_paint_stats && inside(VMS_RS(g_paint_stats)), "render.g_paint_stats single storage");
    CHECK(&VMS_RS(g_graphics_profile) == &g_vm_storage.render.g_graphics_profile && inside(VMS_RS(g_graphics_profile)), "render.g_graphics_profile single storage");
    CHECK(&VMS_RS(g_frame_metrics) == &g_vm_storage.render.g_frame_metrics && inside(VMS_RS(g_frame_metrics)), "render.g_frame_metrics single storage");
    CHECK(&VMS_RS(g_decoded_images) == &g_vm_storage.render.g_decoded_images && inside(VMS_RS(g_decoded_images)), "render.g_decoded_images single storage");
    CHECK(&VMS_RS(g_png_fast) == &g_vm_storage.render.g_png_fast && inside(VMS_RS(g_png_fast)), "render.g_png_fast single storage");
    CHECK(&VMS_RS(g_png_fallback) == &g_vm_storage.render.g_png_fallback && inside(VMS_RS(g_png_fallback)), "render.g_png_fallback single storage");
    CHECK(&VMS_RS(g_image_decode_us) == &g_vm_storage.render.g_image_decode_us && inside(VMS_RS(g_image_decode_us)), "render.g_image_decode_us single storage");
    CHECK(&VMS_RS(g_image_copy_us) == &g_vm_storage.render.g_image_copy_us && inside(VMS_RS(g_image_copy_us)), "render.g_image_copy_us single storage");
    CHECK(&VMS_RS(g_offscreen) == &g_vm_storage.render.g_offscreen && inside(VMS_RS(g_offscreen)), "render.g_offscreen single storage");
    CHECK(&VMS_RS(g_gfx_color) == &g_vm_storage.render.g_gfx_color && inside(VMS_RS(g_gfx_color)), "render.g_gfx_color single storage");
    CHECK(&VMS_RS(g_gfx_target) == &g_vm_storage.render.g_gfx_target && inside(VMS_RS(g_gfx_target)), "render.g_gfx_target single storage");
    CHECK(&VMS_RS(g_gfx_tx) == &g_vm_storage.render.g_gfx_tx && inside(VMS_RS(g_gfx_tx)), "render.g_gfx_tx single storage");
    CHECK(&VMS_RS(g_gfx_ty) == &g_vm_storage.render.g_gfx_ty && inside(VMS_RS(g_gfx_ty)), "render.g_gfx_ty single storage");
    CHECK(&VMS_RS(g_clip) == &g_vm_storage.render.g_clip && inside(VMS_RS(g_clip)), "render.g_clip single storage");
    CHECK(&VMS_RS(g_paint_depth) == &g_vm_storage.render.g_paint_depth && inside(VMS_RS(g_paint_depth)), "render.g_paint_depth single storage");
    CHECK(&VMS_RS(g_drawacc) == &g_vm_storage.render.g_drawacc && inside(VMS_RS(g_drawacc)), "render.g_drawacc single storage");
    CHECK(&VMS_RS(g_cjk) == &g_vm_storage.render.g_cjk && inside(VMS_RS(g_cjk)), "render.g_cjk single storage");
    CHECK(&VMS_RS(g_cjk_tried) == &g_vm_storage.render.g_cjk_tried && inside(VMS_RS(g_cjk_tried)), "render.g_cjk_tried single storage");
    CHECK(&VMS_RS(g_current) == &g_vm_storage.render.display.g_current && inside(VMS_RS(g_current)), "render.display.g_current single storage");
    CHECK(&VMS_RS(g_display) == &g_vm_storage.render.display.g_display && inside(VMS_RS(g_display)), "render.display.g_display single storage");
    CHECK(&VMS_RS(g_repaint) == &g_vm_storage.render.g_repaint && inside(VMS_RS(g_repaint)), "render.g_repaint single storage");
    CHECK(&VMS_RS(g_fb_dirty) == &g_vm_storage.render.g_fb_dirty && inside(VMS_RS(g_fb_dirty)), "render.g_fb_dirty single storage");
    CHECK(&VMS_RS(g_paint_serial) == &g_vm_storage.render.g_paint_serial && inside(VMS_RS(g_paint_serial)), "render.g_paint_serial single storage");
    CHECK(&VMS_RS(g_game_frames) == &g_vm_storage.render.g_game_frames && inside(VMS_RS(g_game_frames)), "render.g_game_frames single storage");
    CHECK(&VMS_RS(g_frame_sig) == &g_vm_storage.render.g_frame_sig && inside(VMS_RS(g_frame_sig)), "render.g_frame_sig single storage");
    CHECK(&VMS_RS(g_m3g_native_backgrounds) == &g_vm_storage.render.g_m3g_native_backgrounds && inside(VMS_RS(g_m3g_native_backgrounds)), "render.g_m3g_native_backgrounds single storage");
    CHECK(&VMS_RS(g_m3g_native_background_pixels) == &g_vm_storage.render.g_m3g_native_background_pixels && inside(VMS_RS(g_m3g_native_background_pixels)), "render.g_m3g_native_background_pixels single storage");
    CHECK(&VMS_RS(g_m3g_native_sorts) == &g_vm_storage.render.g_m3g_native_sorts && inside(VMS_RS(g_m3g_native_sorts)), "render.g_m3g_native_sorts single storage");
    CHECK(&VMS_RS(g_m3g_native_vectors) == &g_vm_storage.render.g_m3g_native_vectors && inside(VMS_RS(g_m3g_native_vectors)), "render.g_m3g_native_vectors single storage");
    CHECK(&VMS_RS(g_m3g_native_vertices) == &g_vm_storage.render.g_m3g_native_vertices && inside(VMS_RS(g_m3g_native_vertices)), "render.g_m3g_native_vertices single storage");
    CHECK(&VMS_RS(g_m3g_native_matrices) == &g_vm_storage.render.g_m3g_native_matrices && inside(VMS_RS(g_m3g_native_matrices)), "render.g_m3g_native_matrices single storage");
    CHECK(&VMS_RS(g_m3g_prep241_enabled) == &g_vm_storage.render.g_m3g_prep241_enabled && inside(VMS_RS(g_m3g_prep241_enabled)), "render.g_m3g_prep241_enabled single storage");
    CHECK(&VMS_RS(g_m3g_mesh242_enabled) == &g_vm_storage.render.g_m3g_mesh242_enabled && inside(VMS_RS(g_m3g_mesh242_enabled)), "render.g_m3g_mesh242_enabled single storage");
    CHECK(&VMS_RS(g_host_boundary242_enabled) == &g_vm_storage.render.g_host_boundary242_enabled && inside(VMS_RS(g_host_boundary242_enabled)), "render.g_host_boundary242_enabled single storage");
    CHECK(&VMS_RS(g_m3g_native_triangles) == &g_vm_storage.render.g_m3g_native_triangles && inside(VMS_RS(g_m3g_native_triangles)), "render.g_m3g_native_triangles single storage");
    CHECK(&VMS_RS(g_m3g_native_pixels) == &g_vm_storage.render.g_m3g_native_pixels && inside(VMS_RS(g_m3g_native_pixels)), "render.g_m3g_native_pixels single storage");
    CHECK(&VMS_RS(g_m3g_fallback_aa) == &g_vm_storage.render.g_m3g_fallback_aa && inside(VMS_RS(g_m3g_fallback_aa)), "render.g_m3g_fallback_aa single storage");
    CHECK(&VMS_RS(g_m3g_fallback_dither) == &g_vm_storage.render.g_m3g_fallback_dither && inside(VMS_RS(g_m3g_fallback_dither)), "render.g_m3g_fallback_dither single storage");
    CHECK(&VMS_RS(g_m3g_fallback_fog) == &g_vm_storage.render.g_m3g_fallback_fog && inside(VMS_RS(g_m3g_fallback_fog)), "render.g_m3g_fallback_fog single storage");
    CHECK(&VMS_RS(g_m3g_fallback_mip) == &g_vm_storage.render.g_m3g_fallback_mip && inside(VMS_RS(g_m3g_fallback_mip)), "render.g_m3g_fallback_mip single storage");
    CHECK(&VMS_RS(g_m3g_row240_enabled) == &g_vm_storage.render.g_m3g_row240_enabled && inside(VMS_RS(g_m3g_row240_enabled)), "render.g_m3g_row240_enabled single storage");
    CHECK(&VMS_RS(g_m3g_span239_enabled) == &g_vm_storage.render.g_m3g_span239_enabled && inside(VMS_RS(g_m3g_span239_enabled)), "render.g_m3g_span239_enabled single storage");
    CHECK(&VMS_RS(g_m3g_sprite241_enabled) == &g_vm_storage.render.g_m3g_sprite241_enabled && inside(VMS_RS(g_m3g_sprite241_enabled)), "render.g_m3g_sprite241_enabled single storage");
    CHECK(&VMS_RS(g_m3g_texture238_enabled) == &g_vm_storage.render.g_m3g_texture238_enabled && inside(VMS_RS(g_m3g_texture238_enabled)), "render.g_m3g_texture238_enabled single storage");
    CHECK(&VMS_RS(g_m3g_texture238_calls) == &g_vm_storage.render.g_m3g_texture238_calls && inside(VMS_RS(g_m3g_texture238_calls)), "render.g_m3g_texture238_calls single storage");
    CHECK(&VMS_RS(g_m3g_texture238_units) == &g_vm_storage.render.g_m3g_texture238_units && inside(VMS_RS(g_m3g_texture238_units)), "render.g_m3g_texture238_units single storage");
    CHECK(&VMS_RS(g_m3g_texture238_mips) == &g_vm_storage.render.g_m3g_texture238_mips && inside(VMS_RS(g_m3g_texture238_mips)), "render.g_m3g_texture238_mips single storage");
    CHECK(&VMS_RS(g_m3g_texture238_declined) == &g_vm_storage.render.g_m3g_texture238_declined && inside(VMS_RS(g_m3g_texture238_declined)), "render.g_m3g_texture238_declined single storage");
    CHECK(&m3g_assembly242::gathered == &g_vm_storage.render.m3g_assembly242_gathered && inside(m3g_assembly242::gathered), "render.m3g_assembly242_gathered alias single storage");
    CHECK(&m3g_assembly242::culled == &g_vm_storage.render.m3g_assembly242_culled && inside(m3g_assembly242::culled), "render.m3g_assembly242_culled alias single storage");
    CHECK(&m3g_assembly242::emitted == &g_vm_storage.render.m3g_assembly242_emitted && inside(m3g_assembly242::emitted), "render.m3g_assembly242_emitted alias single storage");
    CHECK(&m3g_assembly242::declines == &g_vm_storage.render.m3g_assembly242_declines && inside(m3g_assembly242::declines), "render.m3g_assembly242_declines alias single storage");
    CHECK(&m3g_image241::stats == &g_vm_storage.render.m3g_image241_stats && inside(m3g_image241::stats), "render.m3g_image241_stats alias single storage");
    CHECK(&m3g_lighting242::calls == &g_vm_storage.render.m3g_lighting242_calls && inside(m3g_lighting242::calls), "render.m3g_lighting242_calls alias single storage");
    CHECK(&m3g_lighting242::vertices == &g_vm_storage.render.m3g_lighting242_vertices && inside(m3g_lighting242::vertices), "render.m3g_lighting242_vertices alias single storage");
    CHECK(&m3g_lighting242::light_visits == &g_vm_storage.render.m3g_lighting242_light_visits && inside(m3g_lighting242::light_visits), "render.m3g_lighting242_light_visits alias single storage");
    CHECK(&m3g_lighting242::declines == &g_vm_storage.render.m3g_lighting242_declines && inside(m3g_lighting242::declines), "render.m3g_lighting242_declines alias single storage");
    CHECK(&m3g_mesh242::stats == &g_vm_storage.render.m3g_mesh242_stats && inside(m3g_mesh242::stats), "render.m3g_mesh242_stats alias single storage");
    CHECK(&m3g_mesh242::composite_calls == &g_vm_storage.render.m3g_mesh242_composite_calls && inside(m3g_mesh242::composite_calls), "render.m3g_mesh242_composite_calls alias single storage");
    CHECK(&m3g_mesh242::transform_to_calls == &g_vm_storage.render.m3g_mesh242_transform_to_calls && inside(m3g_mesh242::transform_to_calls), "render.m3g_mesh242_transform_to_calls alias single storage");
    CHECK(&m3g_mesh242::transform_to_nodes == &g_vm_storage.render.m3g_mesh242_transform_to_nodes && inside(m3g_mesh242::transform_to_nodes), "render.m3g_mesh242_transform_to_nodes alias single storage");
    CHECK(&m3g_row240::stats == &g_vm_storage.render.m3g_row240_stats && inside(m3g_row240::stats), "render.m3g_row240_stats alias single storage");
    CHECK(&m3g_span239::stats == &g_vm_storage.render.m3g_span239_stats && inside(m3g_span239::stats), "render.m3g_span239_stats alias single storage");
    CHECK(&m3g_sprite241::stats == &g_vm_storage.render.m3g_sprite241_stats && inside(m3g_sprite241::stats), "render.m3g_sprite241_stats alias single storage");
    CHECK(&paint_hotspots241::current == &g_vm_storage.render.paint_hotspots241_current && inside(paint_hotspots241::current), "render.paint_hotspots241_current alias single storage");
    CHECK(&paint_hotspots241::pending == &g_vm_storage.render.paint_hotspots241_pending && inside(paint_hotspots241::pending), "render.paint_hotspots241_pending alias single storage");
    CHECK(&paint_hotspots241::active == &g_vm_storage.render.paint_hotspots241_active && inside(paint_hotspots241::active), "render.paint_hotspots241_active alias single storage");
    CHECK(&paint_hotspots241::have_pending == &g_vm_storage.render.paint_hotspots241_have_pending && inside(paint_hotspots241::have_pending), "render.paint_hotspots241_have_pending alias single storage");
    CHECK(&paint_hotspots241::paints == &g_vm_storage.render.paint_hotspots241_paints && inside(paint_hotspots241::paints), "render.paint_hotspots241_paints alias single storage");
    CHECK(&paint_hotspots241::slow_paints == &g_vm_storage.render.paint_hotspots241_slow_paints && inside(paint_hotspots241::slow_paints), "render.paint_hotspots241_slow_paints alias single storage");
    CHECK(&paint_hotspots241::total_samples == &g_vm_storage.render.paint_hotspots241_total_samples && inside(paint_hotspots241::total_samples), "render.paint_hotspots241_total_samples alias single storage");
    CHECK(&paint_hotspots241::omitted_slow == &g_vm_storage.render.paint_hotspots241_omitted_slow && inside(paint_hotspots241::omitted_slow), "render.paint_hotspots241_omitted_slow alias single storage");
    CHECK(&paint_hotspots241::longest_us == &g_vm_storage.render.paint_hotspots241_longest_us && inside(paint_hotspots241::longest_us), "render.paint_hotspots241_longest_us alias single storage");
    // C09 contract
    j2me_render::SurfaceStateMachine m;
    CHECK(m.state() == j2me_render::SurfaceState::CPU_VALID, "C09 initial state CPU_VALID");
    CHECK(m.transition(j2me_render::SurfaceState::CPU_VALID) && m.illegal_transitions() == 0, "C09 CPU_VALID->CPU_VALID legal");
    CHECK(!m.transition_raw(1) && m.illegal_transitions() == 1 && m.state() == j2me_render::SurfaceState::CPU_VALID, "C09 unknown (P5) state rejected, stays CPU_VALID");
    CHECK(!m.on_release3d() && m.illegal_transitions() == 2, "C09 release without bind counted illegal");
    m.on_bind3d(); CHECK(m.bound3d_depth() == 1 && m.begin_cpu_write() && m.cpu_read(), "C09 bind3d + CPU access in P2 stays CPU_VALID");
    CHECK(m.on_release3d() && m.bound3d_depth() == 0, "C09 release3d");
    auto& s = g_vm_storage.render.surface;
    CHECK(s.state() == j2me_render::SurfaceState::CPU_VALID && inside(s), "render.surface in storage, CPU_VALID");
    // g_fb_dirty keeps its meaning: a plain flag, untouched by the state machine
    bool before = VMS_RS(g_fb_dirty); s.transition(j2me_render::SurfaceState::CPU_VALID);
    CHECK(VMS_RS(g_fb_dirty) == before, "g_fb_dirty not touched by C09");
    CHECK(&vm_state().render == &g_vm_storage.render, "facade render ref");
    std::printf("RENDER_SURFACE_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
