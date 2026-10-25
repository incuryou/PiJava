// 0.2.114 item 2: a compiled region that returns an impossible exit after it may have run ops is handed back to the
// interpreter ONLY if it is pure (whitelist of ops that write nothing but the discarded slot copies). A region with
// int[] stores or the fillRect IC writes one forced JITSTALE line (with done=) and stops through die(): never re-run.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <sstream>

static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL JitBadExitSideEffects " << why << "\n"; std::exit(1); } }
static const jitr::Helpers H{jitr_h_getfield, jitr_h_aload, jitr_h_alen, jitr_h_icget, jitr_h_icchk, jitr_h_cast, jitr_h_inst, jitr_h_icgfx, jitr_h_iastore};
static std::vector<jit236::Constant> g_consts;
static bool build_pure(const std::vector<uint8_t>& code, unsigned locals, unsigned stack, const std::vector<jitr::IcSite>* ic, bool stores, bool& pure) {
    static std::vector<int> fs; fs.assign(code.size(), -1);
    jitr::Spec s; s.code = code.data(); s.size = code.size(); s.entry = 0; s.max_locals = locals; s.max_stack = stack; s.constants = &g_consts; s.field_slot = &fs; s.ic = ic; s.int_array_stores = stores;
    jitr::Region r; if (!jitr::build(s, r)) return false; pure = jitr::region_pure(r); return true;
}
// Stub "compiled code" returning a chosen (done, exit_pc) without touching anything.
static uint64_t g_stub_rc = 0; static int g_stub_calls = 0;
static uint64_t stub(int32_t*, uint32_t) { ++g_stub_calls; return g_stub_rc; }
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    auto& eng = VMS_NATIVE(g_native_jit); eng.configure(true); std::string why; check(eng.enabled && jit_region_self_test(why), "JIT enabled");
    {   // A. purity of built regions
        bool p = false;
        const std::vector<uint8_t> sum = {3, 60, 3, 61, 28, 26, 162, 0, 13, 27, 28, 96, 60, 132, 2, 1, 167, 0xff, 0xf4, 27, 172};
        check(build_pure(sum, 3, 2, nullptr, false, p) && p, "A1: int loop (locals, iinc, branches) is pure");
        const std::vector<uint8_t> st = {42, 3, 3, 79, 177};   // aload_0 iconst_0 iconst_0 iastore return
        check(build_pure(st, 1, 3, nullptr, true, p) && !p, "A2: region with iastore is NOT pure");
        check(build_pure(st, 1, 3, nullptr, false, p) && p, "A3: without int_array_stores iastore is an exit, the rest is pure");
        const std::vector<uint8_t> gfx = {42, 3, 3, 3, 3, 182, 0, 1, 177};   // aload_0 4x iconst_0 invokevirtual fillRect
        std::vector<jitr::IcSite> ic(gfx.size()); ic[5].kind = jitr::IC_GFX; ic[5].argc = 4; ic[5].cost = 1; ic[5].cls = 0; ic[5].value = 3;
        check(build_pure(gfx, 1, 5, &ic, false, p) && !p, "A4: region with the fillRect IC is NOT pure");
        const std::vector<uint8_t> get = {42, 182, 0, 1, 87, 177};   // aload_0 invokevirtual getter pop
        std::vector<jitr::IcSite> ic2(get.size()); ic2[1].kind = jitr::IC_GET; ic2[1].argc = 0; ic2[1].cost = 4; ic2[1].cls = 0; ic2[1].slot = 0;
        check(build_pure(get, 1, 1, &ic2, false, p) && p, "A5: inlined getter (IC_GET) + pop is pure");
        jitr::Ins unknown; unknown.kind = jitr::K_OP; unknown.op = 181; check(!jitr::pure_op(unknown), "A6: any op outside the whitelist (putfield) counts as a write");
    }
    {   // B. decision table
        check(!jitr::bad_exit_must_fail(true, 5) && !jitr::bad_exit_must_fail(true, -1) && !jitr::bad_exit_must_fail(true, 0), "B1: pure regions never fail");
        check(!jitr::bad_exit_must_fail(false, 0), "B2: done==0 goes back to the interpreter");
        check(jitr::bad_exit_must_fail(false, 5) && jitr::bad_exit_must_fail(false, -1) && jitr::bad_exit_must_fail(false, 51), "B3: done>0, done<0, done>budget with writes fail");
    }
    const int cls = ensure_class("JitStaleChecks"); Method* bump = find_method(cls, "bump", "([II)I", false); Method* sum = find_method(cls, "sum", "(I)I", false);
    check(bump && sum, "fixtures");
    const int arr = alloc_array("[I", 64);
    auto fill = [&](int v) { for (size_t i = 0; i < 64; ++i) VMS_HEAP(g_heap)[arr].elems.set(i, Value::of_i(v)); };
    auto all = [&](int v) { for (size_t i = 0; i < 64; ++i) if (VMS_HEAP(g_heap)[arr].elems.get(i).i != v) return false; return true; };
    auto bframe = [&]() { Frame f; f.method = bump; f.locals = {Value::of_ref(arr), Value::of_i(64), Value::of_i(0)}; f.pc = 2; return f; };
    int done = 0; for (int i = 0; i < 200 && !done; ++i) { Frame f = bframe(); done = jit_region_try(f, 50); }
    check(done > 0, "bump region compiled and ran");
    auto& e = g_jit_regions[bump].at[2];
    check(e.writes, "C0: bump region (iastore) is recorded as writing at install");
    check(e.depth.size() > 5 && e.depth[5] < 0, "C0: pc 5 is not an exit of this region");
    auto expect_die = [&](uint64_t rc_done, int exit_pc, int budget, const char* want_done, const char* why_) {
        fill(7); g_stub_rc = (uint64_t(uint32_t(int32_t(rc_done))) << 32) | uint32_t(exit_pc);
        auto* saved = e.exe.fn; e.exe.fn = reinterpret_cast<jit236::Executable::Fn>(&stub);
        const uint64_t fatal = g_jit_region_stats.stale_fatal; VMS_EXEC(g_error).clear();
        std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf());
        Frame f = bframe(); bool died = false; const int calls = g_stub_calls;
        g_die_catch = true;
        if (setjmp(g_die_jmp) == 0) { (void)jit_region_try(f, budget); } else died = true;
        g_die_catch = false; std::cerr.rdbuf(old); e.exe.fn = saved;
        const std::string log = out.str();
        check(died && g_stub_calls == calls + 1, why_);
        check(log.find("JITSTALE kind=region reason=bad_exit method=JitStaleChecks.bump([II)I entry_pc=2") != std::string::npos && log.find(want_done) != std::string::npos, "JITSTALE fail line names method, entry pc and done=");
        check(log.find("action=fail_vm_side_effects") != std::string::npos, "action=fail_vm_side_effects");
        check(log.find("EX jit region bad_exit after side effects in JitStaleChecks.bump([II)I entry_pc=2") != std::string::npos && VMS_EXEC(g_error).rfind("jit region bad_exit", 0) == 0, "honest error through die()");
        check(g_jit_region_stats.stale_fatal == fatal + 1 && log.find("UNDERFLOW") == std::string::npos, "stale_fatal counted, no underflow");
        check(f.pc == 2 && f.stack.empty() && f.locals[2].i == 0 && all(7), "nothing re-executed: frame and int[] unchanged");
    };
    {   // C. real foreign machine code with THIS region's id: 5 ops, budget exit at pc 5 (not a pc of bump's region)
        static const std::vector<uint8_t> code = {3, 60, 3, 61, 3, 61, 3, 61, 177};
        static std::vector<int> fs(code.size(), -1);
        jitr::Spec s; s.code = code.data(); s.size = code.size(); s.entry = 0; s.max_locals = 3; s.max_stack = 2; s.constants = &g_consts; s.field_slot = &fs;
        jitr::Region r; check(jitr::build(s, r), "foreign region builds");
        jit236::Executable foreign; check(foreign.install(jitr::emit_host(r, H, e.exe.id), e.exe.id), "foreign code with the same id");
        auto* saved = e.exe.fn; e.exe.fn = foreign.fn; fill(7); VMS_EXEC(g_error).clear();
        std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf());
        Frame f = bframe(); bool died = false;
        g_die_catch = true;
        if (setjmp(g_die_jmp) == 0) { (void)jit_region_try(f, 5); } else died = true;
        g_die_catch = false; std::cerr.rdbuf(old); e.exe.fn = saved;
        const std::string log = out.str();
        check(died, "C1: side-effect region with done>0 bad exit stops the VM");
        check(log.find("JITSTALE kind=region reason=bad_exit method=JitStaleChecks.bump([II)I entry_pc=2 exit_pc=5 done=5") != std::string::npos, "C2: JITSTALE line with exit_pc and done=5");
        check(log.find("action=fail_vm_side_effects") != std::string::npos && log.find("EX jit region bad_exit after side effects in JitStaleChecks.bump([II)I entry_pc=2 exit_pc=5 done=5") != std::string::npos, "C3: forced line + EX line");
        check(f.pc == 2 && f.stack.empty() && f.locals[2].i == 0 && all(7) && g_jit_region_stats.stale_fatal == 1, "C4: frame and array unchanged (no interpreter re-run), stale_fatal=1");
    }
    expect_die(uint64_t(int64_t(-2)), 7, 50, " done=-2 ", "D1: done<0 (untrusted count) with writes stops the VM");
    expect_die(51, 7, 50, " done=51 ", "D2: done>budget (untrusted count) with writes stops the VM");
    {   // D3. done==0 with an impossible exit pc: the code says nothing ran -> back to the interpreter, recompile, no die
        g_stub_rc = 9999; auto* saved = e.exe.fn; e.exe.fn = reinterpret_cast<jit236::Executable::Fn>(&stub); (void)saved;
        std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf());
        Frame f = bframe(); g_die_catch = true; bool died = false; int r = -1;
        if (setjmp(g_die_jmp) == 0) r = jit_region_try(f, 50); else died = true;
        g_die_catch = false; std::cerr.rdbuf(old);
        check(!died && r == 0 && f.pc == 2, "D3: done==0 returns 0 to the interpreter");
        check(out.str().find("reason=bad_exit") != std::string::npos && out.str().find("action=interpret_and_recompile_fresh_page") != std::string::npos, "D3: logged and scheduled for recompile");
        check(e.state == 0 && e.fresh_page && !e.exe.executable(), "D3: region closed (stub never called again)");
    }
    {   // E. pure region (sum): bad exit with done>0 keeps the 0.2.113 behaviour
        auto sframe = [&]() { Frame f; f.method = sum; f.locals = {Value::of_i(100000), Value::of_i(77), Value::of_i(5)}; f.pc = 4; return f; };
        int d2 = 0; for (int i = 0; i < 200 && !d2; ++i) { Frame f = sframe(); d2 = jit_region_try(f, 50); }
        auto& es = g_jit_regions[sum].at[4]; check(d2 > 0 && !es.writes, "E0: sum region is pure");
        g_stub_rc = (uint64_t(7) << 32) | 7u; es.exe.fn = reinterpret_cast<jit236::Executable::Fn>(&stub);
        std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf());
        Frame f = sframe(); g_die_catch = true; bool died = false; int r = -1;
        if (setjmp(g_die_jmp) == 0) r = jit_region_try(f, 50); else died = true;
        g_die_catch = false; std::cerr.rdbuf(old);
        check(!died && r == 0 && f.pc == 4 && f.locals[1].i == 77 && f.locals[2].i == 5, "E: pure region goes back to the interpreter, frame unchanged");
        check(out.str().find("action=interpret_and_recompile_fresh_page") != std::string::npos, "E: as 0.2.113");
    }
    {   // F. normal path: every element is incremented exactly once per call (no double execution anywhere)
        fill(0); VMS_EXEC(g_error).clear();
        for (int k = 0; k < 40; ++k) check(run(bump, {Value::of_ref(arr), Value::of_i(64)}).i == 64, "F: bump returns n");
        check(all(40), "F: 40 calls -> every element is exactly 40");
    }
    jit_region_reset();   // release compiled regions before static destruction (code pool statics die first)
    std::cout << "PASS JitBadExitSideEffects " << checks << " purity / bad-exit side-effect checks\n";
    return 0;
}
