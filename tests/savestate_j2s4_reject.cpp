// P2.1 FINAL gate: a REAL J2S4 instant save (made by the c3 append1 build from city.jar) must be rejected cleanly:
// clear log line, no crash, file kept byte-for-byte, VM untouched, game keeps running.
// Usage: savestate_j2s4_reject <city.jar> <profile.cfg> <log> <state-dir> <j2s4-sample>
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
static uint64_t fnv(const std::vector<uint8_t>& v) { uint64_t h = 1469598103934665603ull; for (auto c : v) { h ^= c; h *= 1099511628211ull; } return h; }
struct Snap { uint64_t draw, thrown, fb; size_t heap, classes, threads; int live, current, midlet; std::string jarfiles;
  bool operator==(const Snap& o) const { return draw==o.draw && thrown==o.thrown && fb==o.fb && heap==o.heap && classes==o.classes && threads==o.threads && live==o.live && current==o.current && midlet==o.midlet; } };
static Snap snap() { return Snap{VMS_DIAG(g_p1).draw_id, (uint64_t)VMS_EXEC(g_thrown), fnv(VMS_RS(g_fb)), VMS_HEAP(g_heap).size(), VMS_CL(g_classes).size(),
  VMS_EXEC(g_threads).size(), int(VMS_EXEC(g_thread_live)), VMS_RS(g_current), int(VMS_EXEC(g_midlet_state)), ""}; }
int main(int argc, char** argv) {
    assert(argc == 6); std::setvbuf(stdout, nullptr, _IONBF, 0);
    VMS_PLAT(g_boot_ms) = now_ms(); g_state_dir = argv[4];
    VMS_PLAT(g_log).open(argv[3]); setenv("J2ME_P1_PROFILE", argv[2], 1); configure_runtime_diagnostics();
    VMS_CL(g_classpath) = {"romfs/classes"}; VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    const std::string jar = argv[1];
    std::vector<uint8_t> jb; std::string err; assert(read_whole(jar, jb));
    const auto id = rms_jar_identity(jb.data(), jb.size());
    CHECK(id == "563f753243eefeeef25f392f1cb13c22725ad3cdf019a232c6314064cf8897ab", "jar identity city.jar 563f7532");
    assert(unzip(jb, VMS_CL(g_jar_files), err)); load_props();
    const std::string sample = slurp(argv[5]);
    CHECK(sample.size() > 1000 && sample.compare(0, 4, "J2S4") == 0, "sample is a real J2S4 save");
    std::printf("INFO sample bytes=%zu magic=%.4s\n", sample.size(), sample.c_str());
    launch_class("Main", id); g_launched_path = jar;
    pump(8000);
    const std::string path = instant_state_path(jar); dump(path, sample);
    const Snap before = snap();
    VMS_PLAT(g_log).flush();
    const bool restored = restore_instant_state(jar, id);
    VMS_PLAT(g_log).flush();
    const Snap after = snap();
    CHECK(!restored, "restore_instant_state(J2S4) returns false");
    CHECK(before == after, "VM untouched by the rejected load (draw/heap/classes/threads/fb/current/midlet)");
    CHECK(slurp(path) == sample, "J2S4 file kept byte-for-byte");
    const std::string log = slurp(argv[3]);
    CHECK(log.find("SAVE rejected: savestate format J2S4 is from an older build; this build reads J2S5 only") != std::string::npos, "clear log line for the J2S4 rejection");
    pump(3000);
    CHECK(VMS_DIAG(g_p1).draw_id > after.draw && VMS_EXEC(g_thread_live) > 0, "game keeps running after the rejection");
    std::printf("SAVESTATE_J2S4_REJECT_RESULT %s fails=%d draw_before=%llu draw_after=%llu\n", fails ? "FAIL" : "PASS", fails,
        (unsigned long long)after.draw, (unsigned long long)VMS_DIAG(g_p1).draw_id);
    lifecycle_destroy(); VMS_PLAT(g_log).close();
    return fails ? 1 : 0;
}
