// 0.2.114 item 6: the boot self-test also proves the LEAF install-id guard (another id must bail with the stale mark
// and the code's own id). The desktop-only fault J2ME_JIT_FAULT=leafguard (default off, absent on Switch) shows the
// failure path: JIT off, reason jit236_guard_mismatch, JITSELFTEST leaf_selftest=fail.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL JitLeafSelfTestGuard " << why << "\n"; std::exit(1); } }
int main() {
    {   jit236::Engine eng; check(eng.guard_fault == 0, "A0: fault switch off by default");
        check(eng.configure(true) && eng.enabled && eng.self_test && eng.reason == "ok", "A: self-test incl. leaf guard passes"); }
    {   jit236::Engine eng; eng.guard_fault = 1;
        check(!eng.configure(true) && !eng.enabled && !eng.self_test && eng.reason == "jit236_guard_mismatch", "B: a guard that does not bail disables the JIT honestly"); }
    {   jit236::Engine eng; check(!eng.configure(false) && eng.reason == "not_requested", "C: not requested -> no self-test"); }
    std::cout << "PASS JitLeafSelfTestGuard " << checks << " leaf guard self-test checks\n";
    return 0;
}
