// P8.2 (0.2.69-p8.2): logging-only diagnostics. Nothing here feeds back into scheduling, pacing, draws or game state.
//  - P7COUNT summary on continue / instant save / exit (reason=stop|destroy|save|exit|replaced)
//  - P8CAUGHT: exceptions caught by game code, class + message, first kCaughtPerType per type, rest counted
//  - P8ENABLE: which known .enable files were found at startup
//  - P8FRAME: default off (sdmc:/switch/j2me/p8-frame.enable, desktop J2ME_P8_FRAME=1); one line per second,
//    slowest frame split into paint / gc / present / audio / idle / logic (main host thread, exclusive times)
#pragma once
#include <map>
namespace p8 {
inline bool g_frame_on = false;
inline bool g_exit_written = false;
constexpr int kCaughtPerType = 8;
inline std::map<std::string, uint64_t> g_caught;   // per launch: exception class -> count
inline uint64_t g_caught_total = 0;
struct Cats { uint64_t gc = 0, present = 0, audio = 0, idle = 0; };
inline Cats main_cats() {
    auto& s = p7::g_slots[0]; auto t = [&](int c) { return p7::ticks_to_us(s.t[c].load(std::memory_order_relaxed)); };
    Cats c; c.gc = t(p7::Gc) + t(p7::GcWait); c.present = t(p7::Present) + t(p7::PresentWait);
    c.audio = t(p7::AudioMix) + t(p7::AudioOut) + t(p7::AudioWait); c.idle = t(p7::Idle); return c;
}
struct Frame {
    int64_t sec_start_us = 0, last_us = 0; uint64_t frames = 0, sec_index = 0, sum_us = 0;
    Cats last{}; uint64_t paint_gc_start = 0;
    // slowest frame of the current second
    uint64_t max_paint = 0, m_gfx = 0;
    uint64_t max_us = 0, m_paint = 0, m_gc = 0, m_present = 0, m_audio = 0, m_idle = 0, m_logic = 0;
};
inline Frame g_f;
inline void frame_reset() { g_f = Frame{}; }
}
// Flush the open scope so the slot totals are current (main thread only).
static inline void p8_flush_scope() { if (p7::t_cur >= 0) { const uint64_t now = p7::ticks(); p7::g_slots[p7::t_slot].add(p7::t_cur, now - p7::t_mark); p7::t_mark = now; } }
static inline void p8_paint_begin() { if (!p8::g_frame_on) return; p8_flush_scope(); p8::g_f.paint_gc_start = p8::main_cats().gc; p86::g_frame_ns = 0; }
// Called once per completed game frame (draw_end) with the Java paint wall time of that frame.
static void p8_frame_end(int64_t now_us, int64_t paint_us) {
    if (!p8::g_frame_on) return;
    auto& f = p8::g_f; p8_flush_scope(); const p8::Cats c = p8::main_cats();
    if (!f.last_us) { f.last_us = f.sec_start_us = now_us; f.last = c; return; }
    const uint64_t interval = uint64_t(std::max<int64_t>(0, now_us - f.last_us));
    const uint64_t gc = c.gc - f.last.gc, present = c.present - f.last.present, audio = c.audio - f.last.audio, idle = c.idle - f.last.idle;
    const uint64_t gc_in_paint = c.gc - f.paint_gc_start;
    const uint64_t paint = uint64_t(std::max<int64_t>(0, paint_us)) > gc_in_paint ? uint64_t(paint_us) - gc_in_paint : 0;
    const uint64_t used = paint + gc + present + audio + idle, logic = interval > used ? interval - used : 0;
    ++f.frames; f.sum_us += interval;
    const uint64_t gfx = p86::g_frame_ns / 1000; if (paint > f.max_paint) f.max_paint = paint;
    if (interval > f.max_us) { f.max_us = interval; f.m_paint = paint; f.m_gc = gc; f.m_present = present; f.m_audio = audio; f.m_idle = idle; f.m_logic = logic; f.m_gfx = gfx; }
    f.last_us = now_us; f.last = c;
    if (now_us - f.sec_start_us >= 1000000) {
        char b[400];
        // schema=2 (P8.6): slow_* describe the frame with the longest INTERVAL; max_paint_us is the longest paint of the
        // second (any frame); slow_gfx_us = timed drawImage/drawRegion/fillRect/createImage inside the slow frame's paint.
        std::snprintf(b, sizeof b, "P8FRAME schema=2 sec=%llu frames=%llu avg_us=%llu max_us=%llu slow_paint_us=%llu slow_gc_us=%llu slow_present_us=%llu slow_audio_us=%llu slow_idle_us=%llu slow_logic_us=%llu over_16_7ms=%d max_paint_us=%llu slow_gfx_us=%llu",
            (unsigned long long)f.sec_index, (unsigned long long)f.frames, (unsigned long long)(f.frames ? f.sum_us / f.frames : 0), (unsigned long long)f.max_us,
            (unsigned long long)f.m_paint, (unsigned long long)f.m_gc, (unsigned long long)f.m_present, (unsigned long long)f.m_audio, (unsigned long long)f.m_idle,
            (unsigned long long)f.m_logic, f.max_us > 16700 ? 1 : 0, (unsigned long long)f.max_paint, (unsigned long long)f.m_gfx);
        log_line(b);
        { std::string g = "P86GFX sec=" + std::to_string(f.sec_index) + " unit=n/us/max_us/kpx";
          for (int c = 0; c < p86::kCats; ++c) for (int o = 0; o < 2; ++o) { const auto& a = p86::g_sec[c][o]; if (!a.n) continue;
              g += std::string(" ") + p86::kNames[c] + (o ? "@off=" : "@scr=") + std::to_string(a.n) + "/" + std::to_string(a.ns / 1000) + "/" + std::to_string(a.max_ns / 1000) + "/" + std::to_string(a.px / 1000); }
          log_line(g); p86::reset_sec(); }
        log_line("P86WAKE sec=" + std::to_string(f.sec_index) + " spin=" + (p86w::g_spin ? "1" : "0") + " waits=" + std::to_string(p86w::waits) +
                 " late_avg_us=" + std::to_string(p86w::waits ? p86w::late_sum_us / p86w::waits : 0) + " late_max_us=" + std::to_string(p86w::late_max_us) +
                 " late_ge1ms=" + std::to_string(p86w::late_1ms) + " spin_us=" + std::to_string(p86w::spin_us)); p86w::reset();
        ++f.sec_index; f.sec_start_us = now_us; f.frames = 0; f.sum_us = 0; f.max_paint = f.m_gfx = 0; f.max_us = f.m_paint = f.m_gc = f.m_present = f.m_audio = f.m_idle = f.m_logic = 0;
    }
}
// P7COUNT summary that keeps the window open (instant save) or closes it (exit).
static void p8_count_summary(const char* reason, bool close) {
    if (!p7::g_window_start.load(std::memory_order_relaxed)) return;
    log_line(p7::counters_summary(reason));
    if (close) p7::g_window_start.store(0, std::memory_order_relaxed);
}
// Continue-from-instant-save: the session never passes launch_class, so open the window here.
static void p8_count_open_continue() {
    if (!p7::g_count_on) return;
    p8_count_summary("replaced", true);
    p7::counters_reset();
    log_line("P7COUNT_WINDOW open reason=continue");
}
static void p8_caught_reset() { p8::g_caught.clear(); p8::g_caught_total = 0; p8::frame_reset(); }
static void p8_caught_note(int ref, const char* cls, const char* where_cls, const char* where_method, int bci) {
    ++p8::g_caught_total;
    uint64_t& n = p8::g_caught[cls];
    if (++n > (uint64_t)p8::kCaughtPerType) return;
    std::string msg = "-";
    if (ref > 0 && VMS_HEAP(g_heap)[ref].cls >= 0)
        if (Field* fd = find_field(VMS_HEAP(g_heap)[ref].cls, "detail", "Ljava/lang/String;")) {
            const Value& v = VMS_HEAP(g_heap)[ref].fields[fd->slot];
            if (v.ref > 0) msg = read_string(v.ref);
        }
    if (msg.size() > 160) msg.resize(160);
    for (char& ch : msg) if (ch == '\n' || ch == '\r') ch = ' ';
    log_line(std::string("P8CAUGHT exception=") + cls + " n=" + std::to_string(n) + " message=" + msg + " at=" + where_cls + "." + where_method + " bci=" + std::to_string(bci) +
             (std::strcmp(cls, "java/lang/ClassNotFoundException") == 0 ? " requested_class=" + msg : std::string()));
}
static void p8_caught_report() {
    if (!p8::g_caught_total) return;
    std::string s = "P8CAUGHTSTAT total=" + std::to_string(p8::g_caught_total) + " logged_per_type_max=" + std::to_string(p8::kCaughtPerType);
    for (auto& [k, v] : p8::g_caught) s += " " + k + "=" + std::to_string(v);
    log_line(s);
}
static void p8_enable_report() {
    struct E { const char* file; const char* env; };
    static const E list[] = {{"jit.disable","J2ME_JIT"},{"profile.enable","J2ME_PROFILE"},{"p7-worker.disable","J2ME_P7_WORKER"},{"p7-count.enable","J2ME_P7_COUNT"},
        {"p7-audio.disable","J2ME_P7_AUDIO"},{"p7-gc.disable","J2ME_P7_GC"},{"p7-present.disable","J2ME_P7_PRESENT"},{"p8-frame.enable","J2ME_P8_FRAME"},{"p86-spin.enable","J2ME_P86_SPIN"},{"p1-stat.enable","J2ME_P1_STAT"},
        {"sched_legacy.enable",nullptr},{"p2-s0-trace.enable",nullptr},{"p1-city-diag.enable",nullptr},{"m3g_reference.enable","J2ME_M3G_REFERENCE"},
        {"m3g_span_reference.enable","J2ME_M3G_SPAN_REFERENCE"},{"m3g_row_reference.enable","J2ME_M3G_ROW_REFERENCE"},{"m3g_stage241_reference.enable","J2ME_M3G_STAGE241_REFERENCE"},
        {"paint_handoff_reference.enable","J2ME_PAINT_HANDOFF_REFERENCE"},{"float_reference.enable","J2ME_FLOAT_REFERENCE"},{"m3g_sprite_reference.enable","J2ME_M3G_SPRITE_REFERENCE"},
        {"m3g_prep_reference.enable","J2ME_M3G_PREP_REFERENCE"},{"m3g_stage242_reference.enable","J2ME_M3G_STAGE242_REFERENCE"}};
    std::string found;
    for (const E& e : list) {
#ifdef __SWITCH__
        struct stat st{}; const std::string p = std::string("sdmc:/switch/j2me/") + e.file;
        const bool ok = stat(p.c_str(), &st) == 0; const bool txt = !ok && stat((p + ".txt").c_str(), &st) == 0;
        log_line(std::string("P8ENABLE file=") + p + " found=" + (ok ? "1" : "0") + (txt ? " hint=found_with_.txt_extension_rename_it" : ""));
#else
        const char* v = e.env ? std::getenv(e.env) : nullptr; const bool ok = v && std::strcmp(v, "1") == 0;
        log_line(std::string("P8ENABLE file=") + e.file + " env=" + (e.env ? e.env : "-") + " found=" + (ok ? "1" : "0"));
#endif
        if (ok) found += std::string(found.empty() ? "" : ",") + e.file;
    }
    log_line("P8ENABLE summary found=" + (found.empty() ? std::string("none") : found));
}
// Program exit hook (atexit / Switch appletHook exit request / __appExit): one final summary, never twice.
static void p8_exit_summary() {
    if (p8::g_exit_written) return; p8::g_exit_written = true;
    if (p7::g_window_start.load(std::memory_order_relaxed)) { p8_count_summary("exit", true); }
    p8_caught_report();
}
