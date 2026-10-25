// 0.2.114 item 5: a stale leaf whose fresh reinstall fails leaves the leaf budget (bytes_) only if its old code was
// really closed. An install refused during a compiled call keeps the old code open, so it stays counted.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
namespace jit236 { struct EngineTestAccess { static Executable& code(Engine& e, size_t i) { return e.entries_[i].code; } }; }

static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL JitLeafReinstallAccounting " << why << "\n"; std::exit(1); } }
static const uint8_t kLeaf[] = {26, 16, 17, 130, 172};   // (I)I: x ^ 17
static std::string kDesc = "(I)I";
static bool make_stale_leaf(jit236::Engine& eng, jit236::Site& site, jit236::Executable& foreign) {
    if (!eng.configure(true)) return false;
    jit236::Spec s{kLeaf, sizeof(kLeaf), &kDesc, 8, 1, 2, false, false, nullptr};
    for (unsigned i = 0; i <= jit236::Engine::HotThreshold + 1 && site.entry < 0; ++i) eng.prepare(site, s);
    if (site.entry < 0) return false;
    jit236::Program p; if (!jit236::verify(s, p)) return false;
    const uint32_t fid = jit236::next_install_id(); if (!foreign.install(jit236::emit_host(p, fid), fid)) return false;
    jit236::EngineTestAccess::code(eng, size_t(site.entry)).fn = foreign.fn;   // the address now holds code with another id
    return true;
}
int main() {
    int32_t args[1] = {5}; int32_t result = 0; unsigned ops = 0;
    {   // A. reinstall refused during a compiled call: old code stays open and counted
        jit236::Engine eng; jit236::Site site; jit236::Executable foreign; check(make_stale_leaf(eng, site, foreign), "A0: stale leaf prepared");
        auto& code = jit236::EngineTestAccess::code(eng, size_t(site.entry)); const size_t bytes0 = eng.bytes();
        bool ok; { jit236::CodeCall outer; ok = eng.call(site, args, 1, 100, result, ops); }
        check(!ok && eng.stats.stale == 1 && eng.last_stale.hit && !eng.last_stale.reinstalled, "A1: stale caught, reinstall refused");
        check(std::string(jit236::Executable::last_error()) == "install_during_compiled_call", "A2: refusal reason");
        check(code.executable() && eng.bytes() == bytes0, "A3: old code still open -> bytes unchanged (0.2.113 subtracted it)");
    }
    {   // B. reinstall fails after close: closed code leaves the budget
        jit236::Engine eng; jit236::Site site; jit236::Executable foreign; check(make_stale_leaf(eng, site, foreign), "B0: stale leaf prepared");
        auto& code = jit236::EngineTestAccess::code(eng, size_t(site.entry)); const size_t bytes0 = eng.bytes(), old = code.size;
        jit236::Executable::fault() = 1; const bool ok = eng.call(site, args, 1, 100, result, ops); jit236::Executable::fault() = 0;
        check(!ok && !code.executable() && eng.bytes() == bytes0 - old, "B1: closed -> bytes reduced by the old size");
        const uint64_t ex = eng.stats.executed; check(!eng.call(site, args, 1, 100, result, ops) && eng.stats.executed == ex, "B2: closed leaf is never called again");
    }
    {   // C. normal fresh reinstall
        jit236::Engine eng; jit236::Site site; jit236::Executable foreign; check(make_stale_leaf(eng, site, foreign), "C0: stale leaf prepared");
        auto& code = jit236::EngineTestAccess::code(eng, size_t(site.entry)); const size_t bytes0 = eng.bytes(), old = code.size;
        check(!eng.call(site, args, 1, 100, result, ops) && eng.last_stale.reinstalled && eng.stats.stale_reinstalls == 1, "C1: reinstalled on a fresh page");
        check(eng.bytes() == bytes0 - old + code.size, "C2: bytes = before - old + new");
        check(eng.call(site, args, 1, 100, result, ops) && result == (5 ^ 17), "C3: fresh copy runs correctly");
    }
    std::cout << "PASS JitLeafReinstallAccounting " << checks << " leaf reinstall accounting checks\n";
    return 0;
}
