// 0.2.113 item 1: the region glue never trusts an exit pc the region cannot produce. Guarded code with the right id
// that returns such an exit (negative depth) is discarded: no copy-back, no stack clear, one JITSTALE line, recompile.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <sstream>

static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL JitNegativeDepthGlue " << why << "\n"; std::exit(1); } }
static const jitr::Helpers H{jitr_h_getfield, jitr_h_aload, jitr_h_alen, jitr_h_icget, jitr_h_icchk, jitr_h_cast, jitr_h_inst, jitr_h_icgfx, jitr_h_iastore};
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    auto& eng = VMS_NATIVE(g_native_jit); eng.configure(true); std::string why; check(eng.enabled && jit_region_self_test(why), "JIT enabled");
    const int cls = ensure_class("JitStaleChecks"); Method* m = find_method(cls, "sum", "(I)I", false); check(m != nullptr, "fixture");
    auto frame = [&]() { Frame f; f.method = m; f.locals = {Value::of_i(100000), Value::of_i(77), Value::of_i(5)}; f.pc = 4; return f; };
    int done = 0; for (int i = 0; i < 80 && !done; ++i) { Frame f = frame(); done = jit_region_try(f, 50); }
    check(done > 0, "sum region compiled");
    auto& e = g_jit_regions[m].at[4];
    check(e.depth.size() > 7 && e.depth[7] < 0, "precondition: pc 7 (inside if_icmpge) is not a pc of this region");
    // Foreign guarded code carrying THIS region's id: 7 ops (0..6), then a budget exit at pc 7 with done=7.
    static const std::vector<uint8_t> code = {3, 60, 3, 61, 3, 61, 3, 61, 3, 61, 3, 61, 177};
    static std::vector<jit236::Constant> consts; static std::vector<int> fslot(code.size(), -1);
    jitr::Spec s; s.code = code.data(); s.size = code.size(); s.entry = 0; s.max_locals = 3; s.max_stack = 2; s.constants = &consts; s.field_slot = &fslot;
    jitr::Region fr_region; check(jitr::build(s, fr_region), "foreign region builds");
    jit236::Executable foreign; check(foreign.install(jitr::emit_host(fr_region, H, e.exe.id), e.exe.id), "foreign code installs with the same id");
    e.exe.fn = foreign.fn;   // the address now holds code from another region (what a stale CPU/emulator view looks like)
    std::ostringstream captured; auto* old_buf = std::cerr.rdbuf(captured.rdbuf());
    const uint64_t before_lines = g_jitstale_lines;
    Frame f = frame(); const int r = jit_region_try(f, 7);
    std::cerr.rdbuf(old_buf);
    const std::string log = captured.str();
    check(r == 0, "bad exit reports no progress");
    check(f.pc == 4 && f.stack.empty() && f.locals[1].i == 77 && f.locals[2].i == 5, "no copy-back: locals, pc and operand stack unchanged");
    check(g_jitstale_lines == before_lines + 1, "exactly one JITSTALE line");
    check(log.find("JITSTALE kind=region reason=bad_exit method=JitStaleChecks.sum(I)I entry_pc=4 exit_pc=7 done=7") != std::string::npos, "JITSTALE bad_exit line");
    check(log.find("UNDERFLOW") == std::string::npos, "no underflow");
    check(e.state == 0 && e.fresh_page && !e.exe.executable(), "region closed and scheduled for a fresh-page recompile");
    Frame g = frame(); int r2 = jit_region_try(g, 50);
    check(r2 > 0, "recompiled region runs again");
    // depth >= 0 at every legitimate exit: pushes exactly depth values
    check(g.stack.size() == size_t(std::max<int16_t>(0, e.depth[size_t(g.pc)])), "legitimate exit restores exactly depth[exit_pc] values");
    std::cout << "PASS JitNegativeDepthGlue " << checks << " negative-depth / bad-exit glue checks\n";
    return 0;
}
