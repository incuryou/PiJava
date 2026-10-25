// 0.2.114 item 1: g_jitr_last_exit (the UNDERFLOW line's "lastjit") is cleared by jit_region_reset(), which launch,
// instant-state restore, exit-to-menu and every engine epoch change reach (stop_thread -> jit_region_reset), so the
// diagnostic never dereferences a Method* of an earlier session.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <sstream>

static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL JitLastExitReset " << why << "\n"; std::exit(1); } }
static bool cleared() { const auto& x = g_jitr_last_exit; return !x.m && x.exit_pc == -1 && x.exit_depth == -1 && x.entry_depth == -1 && x.done == -1; }
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    auto& eng = VMS_NATIVE(g_native_jit); eng.configure(true); std::string why; check(eng.enabled && jit_region_self_test(why), "JIT enabled");
    const int cls = ensure_class("JitStaleChecks"); Method* m = find_method(cls, "sum", "(I)I", false); check(m != nullptr, "fixture");
    auto run_region = [&]() { int done = 0; for (int i = 0; i < 200 && !done; ++i) { Frame f; f.method = m; f.locals = {Value::of_i(100000), Value::of_i(0), Value::of_i(0)}; f.pc = 4; done = jit_region_try(f, 50); } return done; };
    check(cleared(), "A0: starts cleared");
    check(run_region() > 0 && g_jitr_last_exit.m == m && g_jitr_last_exit.exit_pc >= 0 && g_jitr_last_exit.done > 0, "A: a region exit is recorded");
    jit_region_reset();
    check(cleared(), "B: jit_region_reset clears every field (m=null, pcs/depths/done=-1)");
    check(run_region() > 0 && g_jitr_last_exit.m == m, "C0: recorded again");
    stop_thread();   // launch (j2mevm.cpp stop_thread before g_classes.clear), restore (instant_state.inc), exit-to-menu
    check(cleared(), "C: stop_thread clears it");
    check(run_region() > 0 && g_jitr_last_exit.m == m, "D0: recorded again");
    eng.clear();     // engine epoch bump: the next jit_region_try resets lazily
    { Frame f; f.method = m; f.locals = {Value::of_i(100000), Value::of_i(0), Value::of_i(0)}; f.pc = 4; const int r = jit_region_try(f, 50);
      check(r == 0 && cleared(), "D: the lazy epoch reset clears it before anything new is recorded"); }
    // E. use-after-free shape: a region of session 1 recorded an exit, then the session's classes are freed.
    check(run_region() > 0 && g_jitr_last_exit.m == m, "E0: recorded again");
    stop_thread(); VMS_CL(g_classes).clear(); VMS_CL(g_class_index).clear(); VMS_EXEC(g_active_stack) = nullptr;
    std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf());
    Frame f; underflow_log("pop", f, 1, std::string());
    std::cerr.rdbuf(old);
    check(out.str().find("UNDERFLOW kind=pop method=-") != std::string::npos, "E1: UNDERFLOW line written");
    check(out.str().find("lastjit=- exit_pc=-1 exit_depth=-1 entry_depth=-1 done=-1") != std::string::npos, "E: no stale Method* is read after the classes are gone");
    std::cout << "PASS JitLastExitReset " << checks << " g_jitr_last_exit reset checks\n";
    return 0;
}
