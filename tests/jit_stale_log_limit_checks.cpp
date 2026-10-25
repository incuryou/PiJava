// 0.2.114 item 3: JITSTALE rate limit. Per session the first 20 events are written in full, then one
// JITSTALE_SUMMARY line per 100 events (counts by kind/reason); forced lines always print; the session limiter is
// reset by jit_region_reset() (with a partial summary for the remainder). Interception and recompilation unchanged.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <sstream>

static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::cerr << "FAIL JitStaleLogLimit " << why << "\n"; std::exit(1); } }
static size_t count(const std::string& s, const std::string& k) { size_t n = 0; for (size_t p = s.find(k); p != std::string::npos; p = s.find(k, p + 1)) ++n; return n; }
static std::string line_with(const std::string& s, const std::string& k, size_t nth) {
    size_t p = std::string::npos, from = 0; for (size_t i = 0; i <= nth; ++i) { p = s.find(k, from); if (p == std::string::npos) return ""; from = p + 1; }
    const size_t b = s.rfind('\n', p), e = s.find('\n', p); return s.substr(b == std::string::npos ? 0 : b + 1, e - (b == std::string::npos ? 0 : b + 1));
}
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    auto& eng = VMS_NATIVE(g_native_jit); eng.configure(true); std::string why; check(eng.enabled && jit_region_self_test(why), "JIT enabled");
    const int cls = ensure_class("JitStaleChecks"); Method* m = find_method(cls, "sum", "(I)I", false); check(m != nullptr, "fixture");
    jit_region_reset();
    {   // A-D. limiter on direct calls
        const uint64_t n0 = g_jitstale_lines;
        std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf());
        for (int i = 0; i < 270; ++i) jitstale_log("region", "id_mismatch", m, 4, -1, 0, 1, 2, "interpret_and_recompile_fresh_page");
        std::string log = out.str();
        check(count(log, "JITSTALE kind=") == 20, "A1: exactly the first 20 events are written in full");
        check(count(log, "JITSTALE_SUMMARY") == 2, "A2: one summary per 100 further events (2 for 250)");
        const std::string s1 = line_with(log, "JITSTALE_SUMMARY", 0), s2 = line_with(log, "JITSTALE_SUMMARY", 1);
        check(s1.find("events=120 suppressed=100 region_id_mismatch=100 region_bad_exit=0 leaf_id_mismatch=0 leaf_reinstall_failed=0 other=0") != std::string::npos, "A3: first summary counts");
        check(s1.find("last=JitStaleChecks.sum(I)I@4:id_mismatch") != std::string::npos && s1.find("partial=0") != std::string::npos, "A4: summary names the last event");
        check(s2.find("events=220 suppressed=100") != std::string::npos && s2.find("n=" + std::to_string(n0 + 220)) != std::string::npos, "A5: second summary, process counter n=");
        check(g_jitstale_lines == n0 + 270, "A6: g_jitstale_lines still counts every event");
        check(line_with(log, "JITSTALE kind=", 19).find(" s=20") != std::string::npos, "A7: full lines carry the session index");
        // B. mixed reasons: 30 leaf + 20 bad_exit -> session 320 -> summary at 320 has 50 pending incl. 50 from A
        for (int i = 0; i < 30; ++i) jitstale_log("leaf", "id_mismatch", m, 0, -1, 0, 1, 2, "interpret_and_reinstall_fresh_page");
        for (int i = 0; i < 20; ++i) jitstale_log("region", "bad_exit", m, 4, 7, 7, 1, 1, "interpret_and_recompile_fresh_page");
        log = out.str();
        const std::string s3 = line_with(log, "JITSTALE_SUMMARY", 2);
        check(s3.find("events=320 suppressed=100 region_id_mismatch=50 region_bad_exit=20 leaf_id_mismatch=30 leaf_reinstall_failed=0 other=0") != std::string::npos, "B: per-reason counts add up to suppressed");
        // C. forced line always printed in full
        jitstale_log("region", "bad_exit", m, 4, 7, 5, 1, 1, "fail_vm_side_effects", true);
        log = out.str();
        check(count(log, "JITSTALE kind=") == 21 && log.find("action=fail_vm_side_effects") != std::string::npos && log.find(" done=5 ") != std::string::npos, "C: forced line written past the limit");
        for (int i = 0; i < 7; ++i) jitstale_log("region", "id_mismatch", m, 4, -1, 0, 1, 2, "interpret_and_recompile_fresh_page");
        // D. session reset: partial summary for the 7 pending, then full lines again
        jit_region_reset();
        jitstale_log("region", "id_mismatch", m, 4, -1, 0, 1, 2, "interpret_and_recompile_fresh_page");
        std::cerr.rdbuf(old); log = out.str();
        const std::string sp = line_with(log, "JITSTALE_SUMMARY", 3);
        check(sp.find("suppressed=7 region_id_mismatch=7") != std::string::npos && sp.find("partial=1") != std::string::npos, "D1: remainder flushed as a partial summary at session reset");
        check(count(log, "JITSTALE kind=") == 22 && line_with(log, "JITSTALE kind=", 21).find(" s=1") != std::string::npos, "D2: new session starts with full lines");
    }
    {   // E. interception / recompilation unchanged past the log limit (fault switch: every Normal reinstall is stale)
        jit_region_reset();
        auto frame = [&]() { Frame f; f.method = m; f.locals = {Value::of_i(100000), Value::of_i(0), Value::of_i(0)}; f.pc = 4; return f; };
        int done = 0; for (int i = 0; i < 200 && !done; ++i) { Frame f = frame(); done = jit_region_try(f, 50); }
        check(done > 0, "E0: region compiled");
        auto& e = g_jit_regions[m].at[4];
        jit236::fault_stale() = 1;
        // keep the code block alive (a block whose last slice closes is unmapped and a new one starts unwritten)
        static const std::vector<uint8_t> kk = {3, 60, 3, 61, 177}; static std::vector<int> kf(kk.size(), -1); static std::vector<jit236::Constant> kc;
        jitr::Spec ks; ks.code = kk.data(); ks.size = kk.size(); ks.max_locals = 2; ks.max_stack = 1; ks.constants = &kc; ks.field_slot = &kf; jitr::Region kr; check(jitr::build(ks, kr), "keep-alive builds");
        const jitr::Helpers H{jitr_h_getfield, jitr_h_aload, jitr_h_alen, jitr_h_icget, jitr_h_icchk, jitr_h_cast, jitr_h_inst, jitr_h_icgfx, jitr_h_iastore};
        const uint32_t kid = jit236::next_install_id(); jit236::Executable keep; check(keep.install(jitr::emit_host(kr, H, kid), kid), "keep-alive installs");
        const uint64_t id0 = g_jit_region_stats.stale_id, rc0 = g_jit_region_stats.stale_recompiles;
        std::ostringstream out; auto* old = std::cerr.rdbuf(out.rdbuf());
        int stale = 0;
        for (int k = 0; k < 150; ++k) {
            if (e.exe.size) { g_jit_region_stats.bytes -= e.exe.size; --g_jit_region_stats.resident; e.exe.close(); }
            e.state = 0; e.hits = JitRegionHot - 1; e.fresh_page = false;
            Frame f = frame(); const uint64_t before = g_jit_region_stats.stale_id; const int r = jit_region_try(f, 50);
            if (g_jit_region_stats.stale_id == before + 1 && r == 0 && f.pc == 4) ++stale;
        }
        std::cerr.rdbuf(old); jit236::fault_stale() = 0;
        const std::string log = out.str();
        if (stale != 150) std::cerr << "stale=" << stale << " ids=" << (g_jit_region_stats.stale_id - id0) << " state=" << int(e.state) << "\n";
        check(stale == 150, "E1: every one of 150 stale events was intercepted");
        check(g_jit_region_stats.stale_id - id0 == 150 && g_jit_region_stats.stale_recompiles - rc0 == 150, "E2: every event scheduled a fresh-page recompile");
        check(count(log, "JITSTALE kind=") == 20 && count(log, "JITSTALE_SUMMARY") == 1, "E3: log limited to 20 lines + 1 summary");
        auto& rm = g_jit_regions[m]; Frame f = frame();
        check(!rm.dead[4] && !jitr_skipped(f), "E4: no per-pc / per-method permanent fallback to the interpreter");
        Frame g = frame(); const int r2 = jit_region_try(g, 50);
        check(r2 > 0, "E5: fresh-page recompile runs");
        for (int k = 0; k < 20; ++k) check(run(m, {Value::of_i(3000 + k)}).i == int((3000 + k) * int64_t(2999 + k) / 2), "E6: sum exact");
    }
    jit_region_reset();   // release compiled regions before static destruction (code pool statics die first)
    std::cout << "PASS JitStaleLogLimit " << checks << " JITSTALE rate-limit checks\n";
    return 0;
}
