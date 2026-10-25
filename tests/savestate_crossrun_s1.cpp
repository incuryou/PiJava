// P2.2-S1 J2S5 cross-run gate: a save written by ANOTHER process loads here and the game continues.
// Usage: savestate_crossrun_s1 <city.jar> <profile.cfg> <log> <state-dir> <save-from-other-run.j2s>
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
    assert(argc == 6); std::setvbuf(stdout, nullptr, _IONBF, 0);
    VMS_PLAT(g_boot_ms) = now_ms(); VMS_DIAG(g_log_draw) = true; g_state_dir = argv[4];
    VMS_PLAT(g_log).open(argv[3]); setenv("J2ME_P1_PROFILE", argv[2], 1); setenv("J2ME_S0_TRACE", "1", 1); configure_runtime_diagnostics();
    VMS_CL(g_classpath) = {"romfs/classes"}; VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    const std::string jar = argv[1];
    std::vector<uint8_t> jb; std::string err; assert(read_whole(jar, jb));
    const auto id = rms_jar_identity(jb.data(), jb.size());
    CHECK(id == "563f753243eefeeef25f392f1cb13c22725ad3cdf019a232c6314064cf8897ab", "jar identity city.jar 563f7532");
    assert(unzip(jb, VMS_CL(g_jar_files), err)); load_props();
    launch_class("Main", id); g_launched_path = jar;
    pump(3000);
    const std::string other = slurp(argv[5]);
    CHECK(other.size() > 4 && other.compare(0, 4, "J2S5") == 0, "other-run save is J2S5");
    dump(instant_state_path(jar), other);
    speed_pause_clock(true); // Y sheet opened
    CHECK(restore_instant_state(jar, id), "load save from other run");
    const auto draw_r = VMS_DIAG(g_p1).draw_id;
    CHECK(snap_save(jar), "save A right after cross-run load");
    const std::string a = slurp(instant_state_path(jar)); dump(g_state_dir + "/xa.j2s", a);
    CHECK(restore_instant_state(jar, id) && snap_save(jar), "load A + save B");
    const std::string b = slurp(instant_state_path(jar)); dump(g_state_dir + "/xb.j2s", b);
    CHECK(a == b, "within this run: save A == save B RAW byte-identical");
    speed_pause_clock(false); // B: sheet closed
    pump(4000);
    const auto draw_p = VMS_DIAG(g_p1).draw_id;
    CHECK(draw_p > draw_r && VMS_EXEC(g_thread_live) > 0, "game continues after cross-run load (draws advance, live>0)");
    dispatch_key(-5, false); pump(300); dispatch_key(-5, true); pump(2000);
    CHECK(VMS_DIAG(g_p1).draw_id > draw_p && VMS_EXEC(g_thread_live) > 0 && VMS_EXEC(g_midlet_state) != MidletState::Destroyed, "key input handled after cross-run load, still running");
    std::printf("INFO draws restore=%llu after_pump=%llu after_key=%llu\n", (unsigned long long)draw_r, (unsigned long long)draw_p, (unsigned long long)VMS_DIAG(g_p1).draw_id);
    p1_report(); std::printf("INFO snapshot_safepoint checks=%llu audio_running=%llu audio_ends=%llu violations=%llu\n", (unsigned long long)s0.snap_checks, (unsigned long long)s0.snap_v_audio_running, (unsigned long long)s0.snap_v_audio_ends, (unsigned long long)s0.snap_violation);
    CHECK(s0.snap_checks > 0 && s0.snap_v_audio_running == 0, "snapshot safepoint: audio_running=0 at every snapshot");
    VMS_PLAT(g_log).flush();
    std::printf("CROSSRUN_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
