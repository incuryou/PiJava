// P2.1-c1 savestate round-trip + unsupported-version rejection on the real commercial JAR (read-only).
// Usage: savestate_roundtrip_c1 <city.jar> <profile.cfg> <log> <state-dir>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
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
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
static void pump(int ms) {
    int64_t start = monotonic_ms(), last_present = 0;
    while (monotonic_ms() - start < ms) {
        host_lifecycle_poll242();
        const bool heartbeat = paint_clock_us() - last_present >= 50000;
        p1_host_work_turn(false, heartbeat, [](bool){ scheduled_frame([](){}, game_wake_delay,
            [](int64_t d){ std::this_thread::sleep_for(std::chrono::milliseconds(d)); }, advance_game_frame,
            VMS_EXEC(g_frame_wait_budget), paint_clock_us, &VMS_EXEC(g_wake_stats)); },
            [](int64_t d){ std::this_thread::sleep_for(std::chrono::milliseconds(d)); });
        if (p1::should_show(true, false, false, VMS_RS(g_fb_dirty), 3, heartbeat, speed_group_incomplete())) {
            p1_host_post_present(true, false); last_present = paint_clock_us();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
}
static std::string slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), {}); }
static void dump(const std::string& p, const std::string& b) { std::ofstream f(p, std::ios::binary); f << b; }
int main(int argc, char** argv) {
    assert(argc == 5); std::setvbuf(stdout, nullptr, _IONBF, 0);
    VMS_PLAT(g_boot_ms) = now_ms(); VMS_DIAG(g_log_draw) = true; g_state_dir = argv[4];
    VMS_PLAT(g_log).open(argv[3]); setenv("J2ME_P1_PROFILE", argv[2], 1); configure_runtime_diagnostics();
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
    CHECK(save_instant_state(jar), "save #1 (new version)");
    const std::string s1 = slurp(instant_state_path(jar)); dump(g_state_dir + "/save1.j2s", s1);
    std::printf("INFO save1 bytes=%zu magic=%.4s\n", s1.size(), s1.c_str());
    // 1b) pure format round-trip: decode -> encode must reproduce the file byte-for-byte
    SavState d1; CHECK(decode_state(s1, d1), "decode save #1");
    std::string re1; CHECK(encode_state(d1, re1) && re1 == s1, "decode+encode(save #1) byte-identical");
    // 2) load it, 3) save again immediately -> byte-identical
    CHECK(restore_instant_state(jar, id), "load save #1");
    CHECK(save_instant_state(jar), "save #2 right after load");
    const std::string s2 = slurp(instant_state_path(jar)); dump(g_state_dir + "/save2.j2s", s2);
    size_t diff = 0, first = std::string::npos;
    for (size_t i = 0; i < std::min(s1.size(), s2.size()); ++i) if (s1[i] != s2[i]) { ++diff; if (first == std::string::npos) first = i; }
    std::printf("INFO save2 bytes=%zu differing_bytes=%zu first_diff=%lld\n", s2.size(), diff, first == std::string::npos ? -1LL : (long long)first);
    // Raw bytes differ by design of the existing J2S4 writer (also in P2.1-b): wall-clock-relative deltas
    // (thread wake/sample, alert) and unordered_map iteration order. Reported, not counted; see self-check.
    std::printf("INFO raw_identical=%d (known pre-existing, not a migration effect)\n", int(s1 == s2));
    CHECK(canonical(s1) == canonical(s2), "save #1 == save #2 byte-identical after canonicalisation (hash-container order + wall-clock deltas)");
    // game keeps running after restore
    pump(4000);
    CHECK(VMS_DIAG(g_p1).draw_id > 0 && VMS_EXEC(g_thread_live) > 0, "game alive after restore (draw>0, live>0)");
    std::printf("INFO after_restore draw=%llu live=%d thrown=%llu\n", (unsigned long long)VMS_DIAG(g_p1).draw_id, int(VMS_EXEC(g_thread_live)), (unsigned long long)VMS_EXEC(g_thrown));
    // 4) unsupported/old-version state is rejected cleanly (no crash, no file deleted, VM untouched)
    for (const char* magic : {"J2S0", "J2S9", "XXXX"}) {
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
    VMS_PLAT(g_log).flush();
    std::printf("SAVESTATE_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
