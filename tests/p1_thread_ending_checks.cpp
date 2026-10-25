#include <cassert>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

// P1 diagnostics API contract: these counters describe the current launch.
// Root integration may adapt names, but must preserve each scenario's outcome.
static int value(const char* field) {
    int cls = ensure_class("P1ThreadEndingChecks");
    Field* f = find_field(cls, field, "I");
    assert(f);
    return VMS_CL(g_classes)[f->owner].statics[f->slot].i;
}
static void launch(int mode, const char* suite = "p1-thread-ending-suite",
                   const char* main_class = "P1ThreadEndingChecks") {
    VMS_CL(g_props)["p1.test.mode"] = std::to_string(mode);
    launch_class(main_class, suite);
}
static void drain() {
    const int64_t end = monotonic_ms() + 1000;
    while (VMS_EXEC(g_thread_live) && monotonic_ms() < end) thread_resume();
    assert(!VMS_EXEC(g_thread_live) && VMS_EXEC(g_run_depth) == 0 && VMS_EXEC(g_pump_depth) == 0);
}
int main() {
    std::cout << std::unitbuf;
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"};
    VMS_DIAG(g_log_draw) = false;
    VMS_PLAT(g_boot_ms) = now_ms();
    VMS_EXEC(g_speed_cap) = 0;
    for (int mode : {0, 1, 2, 3, 5}) {
        launch(mode);
        drain();
        const int reason = mode == 2 ? 1 : 0;
        assert(VMS_DIAG(g_p1).reason_counts[reason] == 1);
        for (int other = 0; other < 4; ++other)
            if (other != reason) assert(VMS_DIAG(g_p1).reason_counts[other] == 0);
        assert(VMS_DIAG(g_p1).zero_transitions == 1);
        assert(value("entered") == (mode == 3 ? 0 : 1));
        assert(value("completed") == (mode == 2 || mode == 3 ? 0 : 1));
        assert(value("caught") == (mode == 1 ? 1 : 0));
        if (mode == 1) assert(VMS_DIAG(g_p1).caught_count >= 1);
        std::cout << "PASS P1 ending mode=" << mode << " reason=" << reason << '\n';
    }
    launch(4);
    assert(VMS_EXEC(g_thread_live) && value("entered") == 1 && value("completed") == 0);
    stop_thread();
    assert(!VMS_EXEC(g_thread_live) && VMS_EXEC(g_threads).empty());
    assert(VMS_DIAG(g_p1).reason_counts[2] == 1 && VMS_DIAG(g_p1).reason_counts[0] == 0);
    std::cout << "PASS P1 reset cancels sleeping worker with reason=2\n";

    launch(4);
    assert(VMS_EXEC(g_thread_live));
    lifecycle_destroy();
    assert(!VMS_EXEC(g_thread_live) && VMS_EXEC(g_threads).empty() && VMS_EXEC(g_midlet_state) == MidletState::Destroyed);
    assert(VMS_DIAG(g_p1).reason_counts[3] == 1 && VMS_DIAG(g_p1).reason_counts[0] == 0);
    std::cout << "PASS P1 destroy cancels sleeping worker with reason=3\n";

    launch(1);
    drain();
    const uint64_t prior_launch = VMS_DIAG(g_p1).launch_id;
    assert(VMS_DIAG(g_p1).caught_count >= 1);
    lifecycle_destroy();
    launch(0);
    drain();
    assert(VMS_DIAG(g_p1).launch_id == prior_launch + 1 && VMS_DIAG(g_p1).caught_count == 0);
    assert(VMS_DIAG(g_p1).reason_counts[0] == 1 && VMS_DIAG(g_p1).reason_counts[3] == 0);
    lifecycle_destroy();
    const uint64_t same_suite_launch = VMS_DIAG(g_p1).launch_id;
    launch(3, "p1-second-suite", "P1ThreadEndingOtherChecks");
    drain();
    assert(VMS_DIAG(g_p1).launch_id == same_suite_launch + 1 && VMS_DIAG(g_p1).caught_count == 0);
    assert(VMS_DIAG(g_p1).reason_counts[0] == 1 && VMS_DIAG(g_p1).zero_transitions == 1);
    std::cout << "PASS P1 relaunch same/different suite resets launch diagnostics\n";
}
