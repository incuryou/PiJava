// P2.1-c4 J2S5 savestate RAW round-trip + unsupported-version rejection on the real commercial JAR (read-only).
// Usage: savestate_roundtrip_c4 <city.jar> <profile.cfg> <log> <state-dir>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "host_turn_harness.h"
#ifndef VMS_DIAG
#define VMS_DIAG(n) n  // lets the same harness build against P2.1-b for the pre-existing comparison
#endif
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string g_state_dir;
static std::string instant_state_path(const std::string&) { return g_state_dir + "/city.j2s"; }
#include "../src/instant_state.inc"
static int fails = 0;
// Canonical form: hash-order containers sorted, wall-clock-relative deltas zeroed. Everything else untouched.
static std::string canonical(const std::string& bytes) {
    SavState st; if (!decode_state(bytes, st)) return "decode-failed";
    std::sort(st.classes.begin(), st.classes.end(), [](const SavClass& a, const SavClass& b) { return a.name < b.name; });
    auto zero = [](SavThread& t) { t.wake_delta = 0; t.sample_delta = 0; t.wake_us_delta = 0; };
    zero(st.main); for (auto& t : st.threads) zero(t);
    st.alert_deadline_delta = 0;
    // unordered_map iteration order (class_objects, monitors, gfx_*, offscreen, clips) is not semantic
    std::sort(st.class_objects.begin(), st.class_objects.end());
    std::sort(st.monitors.begin(), st.monitors.end(), [](const SavMonitor& a, const SavMonitor& b) { return a.ref < b.ref; });
    for (auto* v : {&st.gfx_color, &st.gfx_target, &st.gfx_tx, &st.gfx_ty, &st.offscreen}) std::sort(v->begin(), v->end());
    std::sort(st.clips.begin(), st.clips.end(), [](const SavState::SavClip& a, const SavState::SavClip& b) { return a.ref < b.ref; });
    std::string out; return encode_state(st, out) ? out : "encode-failed";
}
// P2.2 audio fix: every snapshot goes through the production Y-sheet save path
// sheet_save_instant_state() (instant_state.inc), which pauses audio around save_instant_state.
// The game clock is paused once for the whole save/load block, as while the Y sheet is open.
static bool snap_save(const std::string& jar) { return sheet_save_instant_state(jar); }
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
static void pump(int ms) {
    int64_t start = monotonic_ms(), last_present = 0;
    while (monotonic_ms() - start < ms) {
        host_lifecycle_poll242();
        harness_host_turn(last_present,false); // P2.6 shared scheduler core (J2ME_SCHED_LEGACY=1: old loop)
    }
}
static std::string slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), {}); }
static void dump(const std::string& p, const std::string& b) { std::ofstream f(p, std::ios::binary); f << b; }
int main(int argc, char** argv) {
    assert(argc == 5); std::setvbuf(stdout, nullptr, _IONBF, 0);
    VMS_PLAT(g_boot_ms) = now_ms(); VMS_DIAG(g_log_draw) = true; g_state_dir = argv[4];
    VMS_PLAT(g_log).open(argv[3]); setenv("J2ME_P1_PROFILE", argv[2], 1); setenv("J2ME_S0_TRACE", "1", 1); configure_runtime_diagnostics();
    VMS_CL(g_classpath) = {"romfs/classes"}; VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    const std::string jar = argv[1];
    std::vector<uint8_t> jb; std::string err; assert(read_whole(jar, jb));
    const auto id = rms_jar_identity(jb.data(), jb.size());
    CHECK(id == "563f753243eefeeef25f392f1cb13c22725ad3cdf019a232c6314064cf8897ab", "jar identity city.jar 563f7532");
    assert(unzip(jb, VMS_CL(g_jar_files), err)); load_props();
    launch_class("Main", id); g_launched_path = jar;
    pump(12000);
    const auto draw0 = VMS_DIAG(g_p1).draw_id; const auto thrown0 = VMS_EXEC(g_thrown);
    std::printf("INFO after_launch draw=%llu live=%d thrown=%llu\n", (unsigned long long)draw0, int(VMS_EXEC(g_thread_live)), (unsigned long long)thrown0);
    // 1) save new-version state
    speed_pause_clock(true); // Y sheet opened
    CHECK(snap_save(jar), "save #1 (new version)");
    const std::string s1 = slurp(instant_state_path(jar)); dump(g_state_dir + "/save1.j2s", s1);
    std::printf("INFO save1 bytes=%zu magic=%.4s\n", s1.size(), s1.c_str());
    // Audio fix must not change J2S5 bytes: the unwrapped save of the same state is byte-identical.
    const auto plain_before = s0.snap_v_audio_running;
    CHECK(restore_instant_state(jar, id) && save_instant_state(jar) && slurp(instant_state_path(jar)) == s1, "sheet save (audio paused) -> load -> plain save (no audio pause) RAW byte-identical");
    dump(instant_state_path(jar), s1);
    const auto plain_unpaused = s0.snap_v_audio_running - plain_before; // the deliberate plain (audio running) save above
    // 1b) pure format round-trip: decode -> encode must reproduce the file byte-for-byte
    SavState d1; CHECK(decode_state(s1, d1), "decode save #1");
    std::string re1; CHECK(encode_state(d1, re1) && re1 == s1, "decode+encode(save #1) byte-identical");
    // 2) load it, 3) save again immediately -> byte-identical
    CHECK(restore_instant_state(jar, id), "load save #1");
    CHECK(snap_save(jar), "save #2 right after load");
    const std::string s2 = slurp(instant_state_path(jar)); dump(g_state_dir + "/save2.j2s", s2);
    size_t diff = 0, first = std::string::npos;
    for (size_t i = 0; i < std::min(s1.size(), s2.size()); ++i) if (s1[i] != s2[i]) { ++diff; if (first == std::string::npos) first = i; }
    std::printf("INFO save2 bytes=%zu differing_bytes=%zu first_diff=%lld\n", s2.size(), diff, first == std::string::npos ? -1LL : (long long)first);
    // J2S5 (P2.1-c4, C10): raw bytes must be identical (logical clock + sorted tables).
    CHECK(s1.compare(0, 4, "J2S5") == 0, "save #1 magic is J2S5");
    CHECK(s1 == s2, "save #1 == save #2 RAW byte-identical (save -> load -> save)");
    // third cycle: load #2, save #3 -> still identical
    CHECK(restore_instant_state(jar, id) && snap_save(jar) && slurp(instant_state_path(jar)) == s2, "save -> load -> save again (cycle 2) RAW byte-identical");
    speed_pause_clock(false); // B: sheet closed
    // game keeps running after restore
    pump(4000);
    CHECK(VMS_DIAG(g_p1).draw_id > 0 && VMS_EXEC(g_thread_live) > 0, "game alive after restore (draw>0, live>0)");
    std::printf("INFO after_restore draw=%llu live=%d thrown=%llu\n", (unsigned long long)VMS_DIAG(g_p1).draw_id, int(VMS_EXEC(g_thread_live)), (unsigned long long)VMS_EXEC(g_thrown));
    // 4) old (J2S1..J2S4) and unknown formats are rejected cleanly (no crash, no file deleted, VM untouched)
    for (const char* magic : {"J2S4", "J2S3", "J2S2", "J2S1", "J2S0", "J2S9", "XXXX"}) {
        std::string bad = s1; bad.replace(0, 4, magic);
        dump(instant_state_path(jar), bad);
        const int midlet = VMS_EXEC(g_midlet); const size_t heap = VMS_HEAP(g_heap).size();
        const bool loaded = restore_instant_state(jar, id);
        char what[128]; std::snprintf(what, sizeof what, "reject magic=%s (returns false, VM untouched, file kept)", magic);
        CHECK(!loaded && VMS_EXEC(g_midlet) == midlet && VMS_HEAP(g_heap).size() == heap && slurp(instant_state_path(jar)) == bad, what);
    }
    // after rejection the game starts normally from scratch
    launch_class("Main", id); pump(6000);
    CHECK(VMS_EXEC(g_thread_live) > 0 && VMS_DIAG(g_p1).draw_id > 0, "normal launch after rejection");
    std::printf("INFO snapshot_safepoint checks=%llu audio_running=%llu audio_ends=%llu violations=%llu\n", (unsigned long long)s0.snap_checks, (unsigned long long)s0.snap_v_audio_running, (unsigned long long)s0.snap_v_audio_ends, (unsigned long long)s0.snap_violation);
    CHECK(s0.snap_checks > 0 && s0.snap_v_audio_running == plain_unpaused && plain_unpaused == 1, "snapshot safepoint: audio_running=0 at every sheet-path snapshot (only the deliberate plain save counted 1)");
    { // Real Y-sheet order: Y -> speed_pause_clock(true); A -> sheet_save_instant_state (production code path).
        const auto before = s0.snap_v_audio_running; const bool paused_before = VMS_PLAT(g_audio).paused();
        speed_pause_clock(true); const bool ok = sheet_save_instant_state(jar); speed_pause_clock(false);
        std::printf("INFO real_sheet_flow audio_running_at_snapshot=%llu audio_paused_after=%d paused_before=%d\n",
                    (unsigned long long)(s0.snap_v_audio_running - before), int(VMS_PLAT(g_audio).paused()), int(paused_before));
        CHECK(ok && s0.snap_v_audio_running == before && VMS_PLAT(g_audio).paused() == paused_before, "real Y-sheet save: audio paused at snapshot, previous audio state restored");
    }
    VMS_PLAT(g_log).flush();
    std::printf("SAVESTATE_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
