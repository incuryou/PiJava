// P2.1-c5: PlatformHost migration checks (desktop): single storage, address stability, host audio path,
// audio pause/resume continuity (no lost / reordered engine samples).
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cstdio>
#include <vector>
static int fails = 0;
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
template <class T> static bool inside(const T& x) {
    auto* lo = reinterpret_cast<const char*>(&j2me_vmstate::g_vm_storage);
    auto* p = reinterpret_cast<const char*>(&x);
    return p >= lo && p + sizeof(T) <= lo + sizeof(j2me_vmstate::g_vm_storage);
}
#define ONE(n) CHECK(&VMS_PLAT(n) == &j2me_vmstate::g_vm_storage.platform.n && inside(VMS_PLAT(n)), "platform." #n " single storage")
static std::vector<int16_t> render_blocks(j2me_audio::Engine& e, int blocks) {
    std::vector<int16_t> out; int16_t pcm[2048];
    for (int b = 0; b < blocks; ++b) { e.render(pcm, 1024); out.insert(out.end(), pcm, pcm + 2048); }
    return out;
}
int main() {
    using namespace j2me_vmstate;
    ONE(g_log); ONE(g_boot_ms); ONE(g_key_states); ONE(g_host_paused); ONE(g_host_frozen236); ONE(g_host_freeze_started236);
    ONE(g_pressed_keys); ONE(g_audio); ONE(g_audio_event_window); ONE(g_audio_event_count); ONE(g_audio_event_suppressed);
    ONE(g_tones); ONE(g_log_pending); ONE(g_cpu_window_start_ns); ONE(g_present_stats); ONE(g_rms_suite); ONE(g_rms_import_legacy);
    CHECK(&acc::platform_audio_pump_previous() == &g_vm_storage.platform.audio_pump_previous, "platform.audio_pump::previous single storage");
    CHECK(std::addressof(vm_state().platform) == std::addressof(g_vm_storage.platform), "facade platform is the storage object");
    CHECK(kVmStorageAddr == &g_vm_storage, "storage address == compile-time address constant");
    // initial values copied 1:1
    CHECK(VMS_PLAT(g_cpu_window_start_ns) == -1 && VMS_PLAT(g_boot_ms) == 0 && !VMS_PLAT(g_host_paused) && VMS_PLAT(g_log_pending) == 0, "initial values");
    // --- host audio path through the migrated storage (desktop audio_pump: render by elapsed time) ---
    auto* audio0 = &VMS_PLAT(g_audio); auto* log0 = &VMS_PLAT(g_log); auto* prev0 = &acc::platform_audio_pump_previous();
    int h = VMS_PLAT(g_audio).playTone(69, 2000, 100);
    CHECK(h > 0, "tone started on the migrated engine");
    audio_pump();                                   // first call only latches the clock
    CHECK(acc::platform_audio_pump_previous() != 0, "audio_pump latched previous in storage");
    int64_t t0 = VMS_PLAT(g_audio).time(h);
    acc::platform_audio_pump_previous() -= 50000;   // pretend 50 ms passed
    audio_pump();
    int64_t t1 = VMS_PLAT(g_audio).time(h);
    CHECK(t1 > t0 && t1 - t0 >= 49000, "audio_pump advanced the migrated engine by elapsed time");
    // --- pause / resume: compare the VM engine stream with an uninterrupted reference engine ---
    audio_reset();
    j2me_audio::Engine ref;
    int hv = VMS_PLAT(g_audio).playTone(64, 3000, 90), hr = ref.playTone(64, 3000, 90);
    CHECK(hv > 0 && hr > 0, "tones for continuity check");
    std::vector<int16_t> want = render_blocks(ref, 12), got = render_blocks(VMS_PLAT(g_audio), 5);
    int64_t tp = VMS_PLAT(g_audio).time(hv);
    audio_suspend(true);
    std::vector<int16_t> silent = render_blocks(VMS_PLAT(g_audio), 4);
    bool all_zero = true; for (int16_t s : silent) all_zero &= s == 0;
    CHECK(all_zero && VMS_PLAT(g_audio).time(hv) == tp, "paused: silence and the stream position does not move");
    audio_suspend(false);
    std::vector<int16_t> rest = render_blocks(VMS_PLAT(g_audio), 7);
    got.insert(got.end(), rest.begin(), rest.end());
    size_t first_diff = got.size();
    for (size_t i = 0; i < got.size(); ++i) if (got[i] != want[i]) { first_diff = i; break; }
    std::printf("INFO continuity samples=%zu first_diff=%zu\n", got.size(), first_diff);
    CHECK(got.size() == want.size() && first_diff == got.size(), "pause/resume: no lost or reordered samples (bit-identical to reference)");
    // repeated pause/resume toggles
    audio_suspend(true); audio_suspend(true); audio_suspend(false); audio_suspend(false);
    std::vector<int16_t> more = render_blocks(VMS_PLAT(g_audio), 3), more_ref = render_blocks(ref, 3);
    CHECK(more == more_ref, "double pause/resume keeps continuity");
    // --- address stability across the whole sequence ---
    CHECK(audio0 == &VMS_PLAT(g_audio) && log0 == &VMS_PLAT(g_log) && prev0 == &acc::platform_audio_pump_previous(), "addresses unchanged after audio activity");
    std::printf("PLATFORM_HOST_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
