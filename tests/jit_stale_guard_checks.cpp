// 0.2.113 item 1: the install-id guard of compiled code, page quarantine, dedicated self-test blocks,
// the desktop/test-only J2ME_JIT_FAULT_STALE switch, and the JITSTALE line written by the VM glue.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <sstream>

static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL JitRegionStaleGuard " << why << "\n"; std::exit(1); } }
static const jitr::Helpers H{jitr_h_getfield, jitr_h_aload, jitr_h_alen, jitr_h_icget, jitr_h_icchk, jitr_h_cast, jitr_h_inst, jitr_h_icgfx, jitr_h_iastore};
// counted loop sum 0..n-1 (the boot self-test program): locals n,s,i
static const std::vector<uint8_t> kSum = {3, 60, 3, 61, 28, 26, 162, 0, 13, 27, 28, 96, 60, 132, 2, 1, 167, 0xff, 0xf4, 27, 172};
static std::vector<jit236::Constant> g_consts; static std::vector<int> g_fslot(kSum.size(), -1);
static jitr::Region sum_region() {
    jitr::Spec s; s.code = kSum.data(); s.size = kSum.size(); s.entry = 0; s.max_locals = 3; s.max_stack = 2; s.constants = &g_consts; s.field_slot = &g_fslot;
    jitr::Region r; check(jitr::build(s, r), "sum region builds"); return r;
}
static uint64_t run_region(const jit236::Executable& e, std::vector<int32_t>& slots, uint32_t id, int32_t n, uint32_t budget = 1000) {
    slots.assign(2 + 2 * (3 + 2 + 1), 0); slots[1] = int32_t(id); slots[2] = n;
    jit236::CodeCall call; return reinterpret_cast<jitr::Fn>(e.fn)(slots.data(), budget);
}
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    const jitr::Region r = sum_region();
    {   // A. region guard: the right id runs, any other id bails before the first op with the code's own id
        const uint32_t id = jit236::next_install_id(); jit236::Executable e; check(e.install(jitr::emit_host(r, H, id), id), "guarded region installs");
        std::vector<int32_t> sl; uint64_t rc = run_region(e, sl, id, 10);
        check(!jitr::stale_result(rc) && uint32_t(rc) == 20 && sl[2 + 2 * 1] == 45, "guarded region runs with its own id");
        rc = run_region(e, sl, id ^ 0x00ff00ffu, 10); std::vector<int32_t> expect(sl.size(), 0); expect[1] = int32_t(id ^ 0x00ff00ffu); expect[2] = 10;
        check(jitr::stale_result(rc) && uint32_t(rc) == id, "mismatched id returns StaleMark and the code's own id");
        check(sl == expect, "stale bail writes no slot (pc, locals and stack untouched)");
        check(jitr::emit_host(r, H, 0).size() < jitr::emit_host(r, H, id).size(), "id 0 emits the unguarded body (old tests)");
    }
    {   // B. leaf guard (args[16] carries the id)
        const uint8_t b[] = {26, 16, 17, 130, 172}; std::string d = "(I)I"; jit236::Spec s{b, sizeof(b), &d, 8, 1, 2, false, false, nullptr}; jit236::Program p;
        check(jit236::verify(s, p), "leaf verifies");
        const uint32_t id = jit236::next_install_id(); jit236::Executable e; check(e.install(jit236::emit_host(p, id), id), "guarded leaf installs");
        int32_t a[jit236::StaleIdSlot + 1]{}; a[0] = 5; a[jit236::StaleIdSlot] = int32_t(id);
        uint64_t v; { jit236::CodeCall c; v = e.fn(a); } check(uint32_t(v) == (5u ^ 17u) && uint32_t(v >> 32) == 4, "guarded leaf runs with its own id");
        a[jit236::StaleIdSlot] = int32_t(id + 1); { jit236::CodeCall c; v = e.fn(a); } check(jit236::stale_result(v) && uint32_t(v) == id, "leaf with another id bails");
    }
    {   // C. quarantine: a freed page is not handed to the next install; dedicated self-test pages never hold game code
        const uint32_t ia = jit236::next_install_id(), ib = jit236::next_install_id();
        jit236::Executable a; check(a.install(jitr::emit_host(r, H, ia), ia), "install A"); auto* addr = reinterpret_cast<void*>(a.fn); a.close();
        check(jit236::code_pool_stats().quarantined_pages > 0, "closed slice is quarantined");
        jit236::Executable b; check(b.install(jitr::emit_host(r, H, ib), ib), "install B"); check(reinterpret_cast<void*>(b.fn) != addr, "B does not reuse A's just-freed page");
        const uint32_t idd = jit236::next_install_id(); jit236::Executable dd; check(dd.install(jitr::emit_host(r, H, idd), idd, jit236::InstallMode::Dedicated), "dedicated install");
        check(jit236::code_pool_stats().dedicated_blocks >= 1, "dedicated block counted");
        auto* dedicated = reinterpret_cast<void*>(dd.fn); dd.close();
        std::vector<jit236::Executable> many(40); bool hit = false;
        for (auto& x : many) { const uint32_t i = jit236::next_install_id(); check(x.install(jitr::emit_host(r, H, i), i), "normal install"); hit |= reinterpret_cast<void*>(x.fn) == dedicated; }
        check(!hit, "normal installs never land on a self-test page");
    }
    {   // D. J2ME_JIT_FAULT_STALE (desktop/test only): reuse keeps the old bytes; the guard catches it; Fresh avoids it
        jit236::fault_stale() = 1;
        const uint32_t ix = jit236::next_install_id(), iy = jit236::next_install_id(), iz = jit236::next_install_id();
        jit236::Executable x; check(x.install(jitr::emit_host(r, H, ix), ix), "fault: install X"); auto* addr = reinterpret_cast<void*>(x.fn); x.close();
        jit236::Executable y; check(y.install(jitr::emit_host(r, H, iy), iy), "fault: install Y");
        check(reinterpret_cast<void*>(y.fn) == addr, "fault: Y reuses X's page at once (0.2.112 first fit)");
        std::vector<int32_t> sl; uint64_t rc = run_region(y, sl, iy, 10);
        check(jitr::stale_result(rc) && uint32_t(rc) == ix, "fault: Y's address still runs X's code and the guard reports X's id");
        check(y.install(jitr::emit_host(r, H, iz), iz, jit236::InstallMode::Fresh), "fault: fresh reinstall");
        check(reinterpret_cast<void*>(y.fn) != addr, "fresh reinstall uses another page");
        rc = run_region(y, sl, iz, 10); check(!jitr::stale_result(rc) && sl[2 + 2 * 1] == 45, "fresh copy runs correctly");
        jit236::fault_stale() = 0;
    }
    {   // E. VM glue under the fault switch: every caught event logs one JITSTALE line; the frame is untouched; Java results stay exact
        auto& eng = VMS_NATIVE(g_native_jit); eng.configure(true); std::string why; check(eng.enabled && jit_region_self_test(why), "JIT enabled with guarded self-test");
        jit236::fault_stale() = 1;
        const int cls = ensure_class("JitStaleChecks"); Method* m = find_method(cls, "sum", "(I)I", false); check(m != nullptr, "fixture method");
        auto frame = [&]() { Frame f; f.method = m; f.locals = {Value::of_i(100000), Value::of_i(0), Value::of_i(0)}; f.pc = 4; return f; };
        int done = 0; for (int i = 0; i < 80 && !done; ++i) { Frame f = frame(); done = jit_region_try(f, 50); }
        check(done > 0, "region compiled and ran");
        auto& e = g_jit_regions[m].at[4]; check(e.state == 1 && e.exe.id != 0, "region resident with an install id");
        const uint32_t old_id = e.exe.id;
        // retire it the way the glue does, then let it recompile: under the fault the new install reuses a written page
        g_jit_region_stats.bytes -= e.exe.size; --g_jit_region_stats.resident; e.exe.close(); e.state = 0; e.hits = JitRegionHot - 1;
        std::ostringstream captured; auto* old_buf = std::cerr.rdbuf(captured.rdbuf());
        const uint64_t lines_before = g_jitstale_lines;
        Frame f = frame(); const Frame before = f; const int r1 = jit_region_try(f, 50);
        std::cerr.rdbuf(old_buf);
        const std::string log = captured.str();
        check(r1 == 0 && g_jitstale_lines == lines_before + 1, "stale call does nothing and logs exactly one line");
        check(f.pc == before.pc && f.stack.size() == before.stack.size() && f.locals.size() == before.locals.size() &&
              f.locals[1].i == before.locals[1].i && f.locals[2].i == before.locals[2].i, "frame untouched after the stale event");
        check(log.find("JITSTALE kind=region reason=id_mismatch method=JitStaleChecks.sum(I)I entry_pc=4 exit_pc=- done=0 expected_id=") != std::string::npos, "JITSTALE line names method, entry pc and reason");
        check(log.find("got_id=") != std::string::npos && log.find("action=interpret_and_recompile_fresh_page") != std::string::npos, "JITSTALE line carries got_id and action");
        check(e.exe.id != old_id || e.state == 0, "stale region is scheduled for recompilation");
        Frame g = frame(); const int r2 = jit_region_try(g, 50);
        check(r2 > 0 && g.pc >= 0, "recompiled region (fresh page) runs");
        { int s = 0; for (int i = 0; i < g.locals[2].i; ++i) s += i; check(g.locals[1].i == s, "recompiled region state is exact"); }
        // end to end through the interpreter (regions retire and reinstall freely under the fault)
        for (int k = 0; k < 40; ++k) {
            check(run(m, {Value::of_i(3000 + k)}).i == int((3000 + k) * int64_t(2999 + k) / 2), "sum exact under the fault switch");
            check(run(find_method(cls, "other", "(I)I", false), {Value::of_i(2000 + k)}).i == int((2000 + k) * int64_t(2001 + k) / 2), "other exact under the fault switch");
        }
        jit236::fault_stale() = 0;
        std::cout << "PASS JitRegionStaleGuard " << checks << " guard, quarantine, dedicated self-test, fault switch and JITSTALE checks (jitstale_lines=" << g_jitstale_lines << ")\n";
    }
    return 0;
}
