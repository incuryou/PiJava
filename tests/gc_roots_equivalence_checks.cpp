// P2.1-c3: root-set equivalence. At the same instant, mark once with the OLD hand-written root list
// (verbatim copy of P2.1-c2 gc_collect, kept only here as a test hook) and once with the new per-module
// roots() (production gc_mark_phase). Requirement: new ⊇ old. Runs on a real commercial JAR (read-only).
// Usage: gc_roots_equivalence_checks <jar> <main-class> <profile.cfg> <log> <ms> <sample_ms>
#include <cassert>
#include <cstdlib>
#include <thread>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "host_turn_harness.h"
static void legacy_mark(std::vector<char>& mark, std::vector<int>& work) {
    auto root = [&](int ref) {
        if (ref <= 0 || ref >= (int)VMS_HEAP(g_heap).size() || mark[ref]) return;
        mark[ref] = 1; work.push_back(ref);
    };
    auto root_val = [&](const Value& v) { if (v.ref > 0) root(v.ref); };
    // ---- BEGIN verbatim P2.1-c2 hand-written root list ----
    root(VMS_RS(g_shown)); root(VMS_RS(g_alert_return)); for (int r:VMS_EXEC(g_serial_calls)) root(r);
    root(VMS_EXEC(g_midlet)); root(VMS_RS(g_current)); root(VMS_RS(g_display)); root(VMS_EXEC(g_main_thread).ref); root(VMS_EXEC(g_main_thread).unhandled); root(VMS_EXEC(g_main_thread).monitor);
    for (const auto& kv : VMS_CL(g_class_objects)) root(kv.second);
    for (const auto& kv : VMS_EXEC(g_monitors)) if (kv.second.owner || !kv.second.waiters.empty()) root(kv.first);
    for (auto& c : VMS_CL(g_classes)) { root(c.init_error);for (auto& s : c.statics) root_val(s); }
    auto root_frames = [&](const std::vector<Frame>* st) {
        if (!st) return;
        for (auto& fr : *st) { root(fr.sync_monitor);root_val(fr.constructed);for (auto& v : fr.locals) root_val(v); for (auto& v : fr.stack) root_val(v); }
    };
    for (auto& t : VMS_EXEC(g_threads)) if (t.state != ThreadState::Terminated || t.executing) {
        root(t.ref); root(t.monitor); root(t.join_target); root(t.unhandled); root_frames(&t.frames);
    }
    for (auto& active : VMS_HEAP(g_root_stacks)) root_frames(active.first);
    // ---- END verbatim root list; trace identical to production ----
    for (size_t sp = 0; sp < work.size(); ++sp) {
        const int ref = work[sp];
        auto target = VMS_RS(g_gfx_target).find(ref); if (target != VMS_RS(g_gfx_target).end()) root(target->second);
        auto buffer = VMS_RS(g_offscreen).find(ref); if (buffer != VMS_RS(g_offscreen).end()) root(buffer->second);
        Obj& o = VMS_HEAP(g_heap)[ref];
        for (auto& v : o.fields) root_val(v);
        if (o.is_array && o.elems.references()) for (size_t i = 0; i < o.elems.size(); ++i) root(o.elems.get(i).ref);
    }
}
static long samples = 0, missing_total = 0, extra_total = 0, old_total = 0, new_total = 0, extra_samples = 0;
static void compare(const char* where) {
    const uint64_t scanned = VMS_HEAP(g_gc_array_refs_scanned);
    std::vector<char> a(VMS_HEAP(g_heap).size(), 0), b(VMS_HEAP(g_heap).size(), 0); std::vector<int> wa, wb;
    legacy_mark(a, wa); gc_mark_phase(b, wb);
    VMS_HEAP(g_gc_array_refs_scanned) = scanned;  // the hook must not perturb stats
    long na = 0, nb = 0, miss = 0, extra = 0;
    for (size_t i = 0; i < a.size(); ++i) { na += a[i]; nb += b[i]; if (a[i] && !b[i]) ++miss; if (b[i] && !a[i]) ++extra; }
    ++samples; missing_total += miss; extra_total += extra; old_total += na; new_total += nb; if (extra) ++extra_samples;
    if (miss || extra || samples % 20 == 1)
        std::printf("SAMPLE %s n=%ld old=%ld new=%ld missing_in_new=%ld extra_in_new=%ld owner_ref=%d yield_canvas=%d\n", where, samples, na, nb, miss, extra, VMS_EXEC(g_repaint_owner_ref), VMS_EXEC(g_repaint_yield_canvas));
}
int main(int argc, char** argv) {
    assert(argc == 7); std::setvbuf(stdout, nullptr, _IONBF, 0);
    VMS_PLAT(g_boot_ms) = now_ms(); VMS_DIAG(g_log_draw) = true;
    VMS_PLAT(g_log).open(argv[4]); setenv("J2ME_P1_PROFILE", argv[3], 1); configure_runtime_diagnostics();
    VMS_CL(g_classpath) = {"romfs/classes"}; VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    std::vector<uint8_t> jb; std::string err; assert(read_whole(argv[1], jb));
    const auto id = rms_jar_identity(jb.data(), jb.size());
    assert(unzip(jb, VMS_CL(g_jar_files), err)); load_props();
    launch_class(argv[2], id);
    const int duration = std::atoi(argv[5]), every = std::atoi(argv[6]);
    int64_t start = monotonic_ms(), last_present = 0, last_sample = -every;
    // scripted key presses so the game leaves its title screen and allocates
    int keys[] = {53, -6, 53, 53, 54, 52, 53}; size_t k = 0; int64_t next_key = 3000; int held = 0; int64_t release_at = 0;
    while (monotonic_ms() - start < duration) {
        host_lifecycle_poll242();
        const int64_t el = monotonic_ms() - start;
        if (held && el >= release_at) { dispatch_key(held, true); held = 0; }
        if (!held && el >= next_key && k < sizeof keys / sizeof *keys) { held = keys[k++]; dispatch_key(held, false); release_at = el + 120; next_key += 2500; }
        harness_host_turn(last_present,false); // P2.6 shared scheduler core (J2ME_SCHED_LEGACY=1: old loop)
        if (el - last_sample >= every) { compare("host_turn"); last_sample = el; }
    }
    compare("final");
    std::printf("ROOTS_EQUIV samples=%ld old_marked_sum=%ld new_marked_sum=%ld missing_in_new_sum=%ld extra_in_new_sum=%ld samples_with_extra=%ld gc_cycles=%llu heap_slots=%zu\n",
        samples, old_total, new_total, missing_total, extra_total, extra_samples, (unsigned long long)VMS_HEAP(g_gc_cycles), VMS_HEAP(g_heap).size());
    const bool ok = samples > 10 && missing_total == 0;
    std::printf("ROOTS_EQUIV_RESULT %s (new ⊇ old)\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
