#include "../jit_region.h"
static void jitstale_log(const char* kind, const char* reason, const Method* m, int entry_pc, int exit_pc, int done, uint32_t expected, uint32_t got, const char* action, bool force = false);
static void jitstale_session_reset();   // 0.2.114: per-session JITSTALE rate limit (defined after jitstale_log)
// Called only for invokestatic after the existing class-initialization check.
// No Java heap allocation, monitor or callback can occur in admitted leaves.
static bool jit_try_call(Method* m,const Value* values,int count,unsigned budget,Value& value,unsigned& executed) {
    if(!VMS_NATIVE(g_native_jit).enabled||!VMS_NATIVE(g_native_jit).available||m->native||!(m->flags&8)||count<0||count>16)return false;
    if(m->jit_site.epoch==VMS_NATIVE(g_native_jit).epoch()&&m->jit_site.entry==-2)return false;
    std::vector<jit236::Constant> constants;
    // Constant pool entries are copied only once, after a method becomes hot.
    if(m->jit_site.epoch==VMS_NATIVE(g_native_jit).epoch()&&m->jit_site.entry==-1&&m->jit_site.calls>=jit236::Engine::HotThreshold){
        const auto& cp=VMS_CL(g_classes)[m->class_id].cp;constants.resize(cp.size());
        for(size_t i=0;i<cp.size();++i)if(cp[i].tag==3)constants[i]={true,int32_t(cp[i].a)};
    }
    const jit236::Spec spec{m->code.data(),m->code.size(),&m->desc,m->flags,m->max_locals,m->max_stack,
        !m->handlers.empty(),!m->name.empty()&&m->name[0]=='<',&constants};
    if(!VMS_NATIVE(g_native_jit).prepare(m->jit_site,spec))return false;
    int32_t args[16]{};for(int i=0;i<count;++i){if(values[i].ref||values[i].is_long)return false;args[i]=values[i].i;}
    int32_t result=0;if(!VMS_NATIVE(g_native_jit).call(m->jit_site,args,unsigned(count),budget,result,executed)){
        auto& st=VMS_NATIVE(g_native_jit).last_stale;   // 0.2.113: one JITSTALE line per caught stale leaf call
        if(st.hit){st.hit=false;jitstale_log("leaf","id_mismatch",m,0,-1,0,st.expected,st.got,st.reinstalled?"interpret_and_reinstall_fresh_page":"interpret_reinstall_failed");}
        return false;}
    value=Value::of_i(result);return true;
}

// ---------------- P6.2 T2: region JIT glue (only reached when the JIT is enabled) ----------------
// Helpers mirror execute_fast_batch's checks exactly; a failed check means "do not execute this op here".
static uint64_t jitr_h_getfield(int32_t ref, int32_t slot, int32_t) {
    if (ref <= 0 || size_t(ref) >= VMS_HEAP(g_heap).size() || slot < 0 || size_t(slot) >= VMS_HEAP(g_heap)[ref].fields.size()) return jitr::Bail;
    const Value& v = VMS_HEAP(g_heap)[ref].fields[size_t(slot)];
    if (v.is_long || v.l != 0 || v.ref < 0) return jitr::Bail;          // only (i,ref) values are representable
    return jitr::pack(v.i, v.ref);
}
static uint64_t jitr_h_aload(int32_t ref, int32_t idx, int32_t) {
    if (ref <= 0 || size_t(ref) >= VMS_HEAP(g_heap).size() || idx < 0 || size_t(idx) >= VMS_HEAP(g_heap)[ref].elems.size()) return jitr::Bail;
    const Value v = VMS_HEAP(g_heap)[ref].elems.get(size_t(idx));
    if (v.is_long || v.l != 0 || v.ref < 0) return jitr::Bail;
    return jitr::pack(v.i, v.ref);
}
static uint64_t jitr_h_alen(int32_t ref, int32_t, int32_t) {
    if (ref <= 0 || size_t(ref) >= VMS_HEAP(g_heap).size()) return jitr::Bail;
    return jitr::pack(int32_t(VMS_HEAP(g_heap)[ref].elems.size()), 0);
}
// Only primitive int[] writes: no reference barrier, allocation or Java call.
// Check everything before mutating; the interpreter owns null/bounds/type failures.
static uint64_t jitr_h_iastore(int32_t ref, int32_t idx, int32_t value) {
    if(ref<=0 || size_t(ref)>=VMS_HEAP(g_heap).size() || idx<0)return jitr::Bail;
    auto& a=VMS_HEAP(g_heap)[ref];
    if(!a.is_array || a.array_desc!="[I" || size_t(idx)>=a.elems.size())return jitr::Bail;
    a.elems.set(size_t(idx),Value::of_i(value));
    return 0;
}
// ---- P6.3 T3 helpers: monomorphic IC guard / inlined trivial getter / checkcast+instanceof per-site cache ----
static int32_t* g_jitr_slots = nullptr; static unsigned g_jitr_locals = 0;
static uint64_t jitr_h_icchk(int32_t ref, int32_t cls, int32_t) {
    if (ref <= 0 || size_t(ref) >= VMS_HEAP(g_heap).size()) return jitr::Bail;
    const auto& o = VMS_HEAP(g_heap)[ref]; if (o.is_array || o.cls < 0) return jitr::Bail;
    if (cls >= 0 && o.cls != cls) return jitr::Bail;   // cls < 0: invokespecial — any concrete class
    return 0;
}
static uint64_t jitr_h_icget(int32_t ref, int32_t cls, int32_t slot) {
    if (jitr_h_icchk(ref, cls, 0) == jitr::Bail) return jitr::Bail;
    return jitr_h_getfield(ref, slot, 0);
}
// fillRect ONLY (gfx kind 3: never throws, never allocates Java objects, no callbacks). Graphics intrinsic via the same gfx_intrinsic() the interpreter uses. Args are read from the live region slots
// (g_jitr_slots); a class miss or null returns Bail so the invoke is re-executed by the interpreter.
static uint64_t g_jitr_gfx_bail_slots = 0, g_jitr_gfx_bail_cls = 0, g_jitr_gfx_ok = 0;
static uint64_t jitr_h_icgfx(int32_t ref, int32_t cls, int32_t pack) {
    const unsigned argc = (uint32_t(pack) >> 8) & 0xff, depth = (uint32_t(pack) >> 16) & 0xffff, id = uint32_t(pack) & 0xff;
    if (!g_jitr_slots || id != 3 || argc > 8 || depth < argc + 1) {
        ++g_jitr_gfx_bail_slots;
        
        return jitr::Bail; }
    if (jitr_h_icchk(ref, cls, 0) == jitr::Bail) {
        ++g_jitr_gfx_bail_cls;
        
        return jitr::Bail; }
    Value a[9]; const unsigned base = depth - argc - 1;
    for (unsigned i = 0; i <= argc; ++i) { a[i].i = g_jitr_slots[2 + 2 * (g_jitr_locals + base + i)]; a[i].ref = g_jitr_slots[3 + 2 * (g_jitr_locals + base + i)]; }
    gfx_intrinsic(uint8_t(id), a);
    if (VMS_EXEC(g_thrown)) return jitr::Bail;
    ++g_jitr_gfx_ok; return 0;
}
struct JitCastSite { std::string name; int ok_cls = -1, no_cls = -1; };
static std::vector<JitCastSite> g_jitr_casts;
static uint64_t jitr_h_cast(int32_t ref, int32_t site, int32_t) {
    if (ref == 0) return 0;                                                   // null passes checkcast
    if (ref < 0 || size_t(ref) >= VMS_HEAP(g_heap).size() || size_t(site) >= g_jitr_casts.size()) return jitr::Bail;
    const auto& o = VMS_HEAP(g_heap)[ref]; if (!o.is_array && o.cls >= 0 && o.cls == g_jitr_casts[size_t(site)].ok_cls) return 0;
    return jitr::Bail;                                                        // unknown / failing -> interpreter decides and throws
}
static uint64_t jitr_h_inst(int32_t ref, int32_t site, int32_t) {
    if (ref == 0) return jitr::pack(0, 0);
    if (ref < 0 || size_t(ref) >= VMS_HEAP(g_heap).size() || size_t(site) >= g_jitr_casts.size()) return jitr::Bail;
    const auto& o = VMS_HEAP(g_heap)[ref]; const auto& c = g_jitr_casts[size_t(site)];
    if (o.is_array || o.cls < 0) return jitr::Bail;
    if (o.cls == c.ok_cls) return jitr::pack(1, 0); if (o.cls == c.no_cls) return jitr::pack(0, 0);
    return jitr::Bail;
}
// Learn a cast site's receiver class in VM context, at the exact point the interpreter is about to evaluate the
// same type_compatible() for the same op (identical side effects, which are idempotent class loads).
static void jitr_learn_cast(int site, int ref) {
    if (site < 0 || size_t(site) >= g_jitr_casts.size() || ref <= 0 || size_t(ref) >= VMS_HEAP(g_heap).size()) return;
    const auto& o = VMS_HEAP(g_heap)[ref]; if (o.is_array || o.cls < 0) return;
    auto& c = g_jitr_casts[size_t(site)]; if (o.cls == c.ok_cls || o.cls == c.no_cls) return;
    const int cls = o.cls; const bool ok = type_compatible(ref, c.name);
    if (ok && c.ok_cls < 0) c.ok_cls = cls; else if (!ok && c.no_cls < 0) c.no_cls = cls;
}
static bool city_method_traced_name(const Method* m) {   // same set as city_method_enter(); keep those calls interpreted
    const std::string& owner = VMS_CL(g_classes)[m->class_id].name;
    return (owner=="ca"&&((m->name=="c"&&m->desc=="(I)V")||(m->name=="d"&&m->desc=="()V")))||((owner=="i"||owner=="ac")&&m->name=="a"&&m->desc=="(I)V")||
        (owner=="ac"&&m->name=="<init>")||(owner=="j"&&((m->name=="c"&&m->desc=="(I)V")||(m->name=="g"&&m->desc=="()V")));
}
// Recognise a trivial callee for the monomorphic IC. Only bodies whose every op is a pure read are inlined.
static jitr::IcSite jitr_ic_for(const Method* caller, int pc) {
    jitr::IcSite ic; const auto& c = caller->code; const int op = c[size_t(pc)];
    if (size_t(pc) >= caller->quick.size()) return ic;
    const QuickSite& q = caller->quick[size_t(pc)];
    if ((q.flags >> 1) || q.library_intrinsic) return ic;
    if (q.intrinsic) {   // monomorphic IC of Graphics.drawImage/drawRegion/fillRect (same gfx_intrinsic as interpreter)
        if (!(q.flags & 1) || q.argc != 4 || q.intrinsic != 3) return ic;    // fillRect(IIII)V only: drawImage/Region can throw NPE
        if (op == 182 && q.kind == 4 && q.vclass >= 0) { ic.kind = jitr::IC_GFX; ic.argc = q.argc; ic.cls = q.vclass; ic.value = q.intrinsic; ic.cost = 1; }
        return ic;
    }
    Method* t = nullptr; int cls = -2;
    if (op == 182 && q.kind == 4 && q.vclass >= 0) { t = method_at((uint32_t(q.va) << 16) | uint32_t(q.vb)); cls = q.vclass; }
    else if (op == 183 && q.kind == 3) { t = method_at((uint32_t(q.a) << 16) | uint32_t(q.b)); cls = -2; }
    if (!t || t->native || (t->flags & (0x0008 | 0x0020 | 0x0400 | 0x0100)) || t->name == "<init>" || t->name == "<clinit>" || !t->handlers.empty()) return ic;
    if (city_method_traced_name(t)) return ic;
    if (t->desc.find('J') != std::string::npos || t->desc.find('D') != std::string::npos) return ic;   // no long/double args/returns
    if (q.argc > 8) return ic;
    const auto& b = t->code; const char rt = t->desc.back();
    ic.argc = q.argc; ic.cls = cls;
    auto field_ok = [&](int fpc) -> int {
        if (size_t(fpc) >= t->quick.size()) return -1; const QuickSite& fq = t->quick[size_t(fpc)];
        if (fq.kind != 1 || fq.b < 0) return -1;
        std::string fc, fn, fd; resolve_member(VMS_CL(g_classes)[t->class_id], (b[size_t(fpc) + 1] << 8) | b[size_t(fpc) + 2], fc, fn, fd);
        if (fd.empty() || fd[0] == 'J' || fd[0] == 'D') return -1; return fq.b; };
    if (b.size() == 5 && b[0] == 42 && b[1] == 180 && (b[4] == 172 || b[4] == 176) && rt != 'V') {
        const int slot = field_ok(1); if (slot < 0) return ic; ic.kind = jitr::IC_GET; ic.slot = slot; ic.cost = 4; }
    else if (b.size() == 2 && b[0] >= 2 && b[0] <= 8 && b[1] == 172) { ic.kind = jitr::IC_CONST; ic.value = b[0] - 3; ic.cost = 3; }
    else if (b.size() == 3 && b[0] == 16 && b[2] == 172) { ic.kind = jitr::IC_CONST; ic.value = int8_t(b[1]); ic.cost = 3; }
    else if (b.size() == 4 && b[0] == 17 && b[3] == 172) { ic.kind = jitr::IC_CONST; ic.value = int16_t((b[1] << 8) | b[2]); ic.cost = 3; }
    else if (b.size() == 2 && b[0] == 1 && b[1] == 176) { ic.kind = jitr::IC_CONST; ic.value = 0; ic.cost = 3; }
    else if (b.size() == 1 && b[0] == 177 && rt == 'V') { ic.kind = jitr::IC_VOID; ic.cost = 2; }
    else if (b.size() == 2 && b[0] == 42 && b[1] == 176 && q.argc == 0) { ic.kind = jitr::IC_SELF; ic.cost = 3; }
    return ic;
}
struct JitRegionStatsT3 { uint64_t ic_sites = 0, cast_sites = 0, stack_entries = 0, recompiles = 0; };
static JitRegionStatsT3 g_jit_region_t3;
struct JitRegionEntry { int16_t entry_depth = -1; uint32_t hits = 0; uint8_t state = 0; uint32_t runs = 0; uint64_t run_ops = 0; jit236::Executable exe; std::vector<int16_t> depth; std::vector<uint8_t> stored; std::vector<int> cast_idx; std::vector<uint8_t> ic_kind; uint8_t recompiles = 0; bool stale = false; unsigned locals = 0; bool fresh_page = false; uint32_t stale_events = 0;
    bool writes = true;   // 0.2.114: region contains an op with effects outside the frame (int[] store / fillRect); set at install
    int32_t maj_pc = -1; uint32_t maj_cnt = 0, inv_exits = 0; };   // P6.7: per-region exit statistics (majority exit pc, exits at non-IC invokes)
struct JitRegionMethod { uint64_t epoch = 0; bool scanned = false; std::vector<uint8_t> banned; std::vector<uint8_t> dead; std::unordered_map<int, JitRegionEntry> at; };
struct JitRegionStats { uint64_t skip_marked = 0, full_skipped = 0, retired_invoke = 0, retired_short = 0, compiled = 0, rejected = 0, banned_wait = 0, entries = 0, ops = 0, zero = 0, bytes = 0, resident = 0;
    uint64_t stale_id = 0, stale_exit = 0, stale_recompiles = 0;   // 0.2.113: caught stale / mismatched code events
    uint64_t stale_fatal = 0; };   // 0.2.114: bad exits of regions with side effects (VM stopped, never re-run)
static std::unordered_map<const Method*, JitRegionMethod> g_jit_regions; static const Method* g_jit_region_last_m = nullptr; static JitRegionMethod* g_jit_region_last = nullptr; static JitRegionStats g_jit_region_stats; static uint64_t g_jit_region_epoch = 0;
// 0.2.112 F3: the most recent compiled-region exit, reported only by the UNDERFLOW diagnostic line.
struct JitrLastExit { const Method* m = nullptr; int exit_pc = -1, exit_depth = -1, entry_depth = -1, done = -1; };
static JitrLastExit g_jitr_last_exit;
// The budget describes live executable memory, not lifetime compilation totals.
// A suite change or snapshot restore releases all regions; release its budget,
// receiver caches and diagnostics together before the next suite becomes hot.
static void jit_region_reset() {
    g_jit_regions.clear(); g_jitr_casts.clear();
    g_jit_region_last_m=nullptr; g_jit_region_last=nullptr;
    g_jit_region_stats={}; g_jit_region_t3={};
    g_jitr_gfx_ok=g_jitr_gfx_bail_cls=g_jitr_gfx_bail_slots=0;
    g_jitr_slots=nullptr; g_jitr_locals=0;
    g_jit_region_epoch=VMS_NATIVE(g_native_jit).epoch();
    g_jitr_last_exit = {};      // 0.2.114: never report a Method* of an earlier session (launch / restore / exit reach here via stop_thread)
    jitstale_session_reset();   // 0.2.114: each session starts with full JITSTALE lines again
}
// P6.8: per-pc 'do not try' bit on the Method itself (cheap check before jit_region_try). Set for retired / dead /
// wait-loop pcs and, once the code budget is full, for every pc without compiled code. Bits are tied to the engine
// epoch (restore / clear bumps it), so a stale bit is never honoured; never serialized (not part of J2S5).
static inline void jitr_mark_skip(Method* m, int pc) {
    const uint64_t ep = VMS_NATIVE(g_native_jit).epoch();
    if (m->jitr_skip_epoch != ep) { m->jitr_skip.assign(m->code.size(), 0); m->jitr_skip_epoch = ep; }
    if (size_t(pc) < m->jitr_skip.size()) m->jitr_skip[size_t(pc)] = 1;
}
static inline bool jitr_skipped(const Frame& fr) {
    const Method* m = fr.method;
    return m->jitr_skip_epoch == VMS_NATIVE(g_native_jit).epoch() && size_t(fr.pc) < m->jitr_skip.size() && m->jitr_skip[size_t(fr.pc)];
}
constexpr uint32_t JitRegionInvokeRetireOps = 32; constexpr uint32_t JitRegionHot = 64; constexpr size_t JitRegionMaxBytes = jit_budget::RegionBytes; constexpr size_t JitRegionMax = jit_budget::RegionEntries;
static int jvm_op_length(const std::vector<uint8_t>& c, size_t pc) {
    static const int8_t len[256] = {
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 2,3,2,3,3,2,2,2,2,2,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,2,2,2,2,2,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,3,2,0,0,1,1,1,1, 1,1,3,3,3,3,3,3,3,5,5,3,2,3,1,1,
        3,3,1,1,0,4,3,3,5,5,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const int op = c[pc];
    auto u32at = [&](size_t q) -> int64_t { if (q + 3 >= c.size()) return -1; return int32_t((uint32_t(c[q]) << 24) | (uint32_t(c[q + 1]) << 16) | (uint32_t(c[q + 2]) << 8) | c[q + 3]); };
    if (op == 170 || op == 171) { size_t q = (pc + 4) & ~size_t(3);
        if (op == 170) { int64_t lo = u32at(q + 4), hi = u32at(q + 8); if (lo < 0 && hi < lo) return -1; if (q + 12 > c.size() || hi < lo) return -1; return int(q + 12 + size_t(hi - lo + 1) * 4 - pc); }
        int64_t n = u32at(q + 4); if (n < 0) return -1; return int(q + 8 + size_t(n) * 8 - pc); }
    if (op == 196) { if (pc + 1 >= c.size()) return -1; return c[pc + 1] == 132 ? 6 : 4; }
    return len[op];
}
// Wait loops are never compiled: every pc inside a loop (backward-branch range) that calls a clock / yield / sleep /
// wait is banned, and methods with poll/time-gate patterns are banned entirely.
static void jit_region_scan(const Method* m, JitRegionMethod& rm) {
    rm.scanned = true; rm.banned.assign(m->code.size(), 0); rm.dead.assign(m->code.size(), 0);
    if (!m->time_gates.empty()) { std::fill(rm.banned.begin(), rm.banned.end(), 1); return; }
    const Class& owner = VMS_CL(g_classes)[m->class_id]; const auto& c = m->code;
    std::vector<int> waits;
    for (size_t pc = 0; pc + 2 < c.size(); ) {
        const int op = c[pc]; const int len = jvm_op_length(c, pc); if (len <= 0) { std::fill(rm.banned.begin(), rm.banned.end(), 1); return; }
        if (op >= 182 && op <= 185) { std::string cls, name, desc; resolve_member(owner, (c[pc + 1] << 8) | c[pc + 2], cls, name, desc);
            if ((cls == "java/lang/System" && (name == "currentTimeMillis" || name == "nanoTime")) || (cls == "java/lang/Thread" && (name == "yield" || name == "sleep")) || name == "wait")
                waits.push_back(int(pc)); }
        pc += size_t(len);
    }
    if (waits.empty()) return;
    for (size_t pc = 0; pc < c.size(); ) {
        const int op = c[pc]; const int len = jvm_op_length(c, pc); if (len <= 0) break;
        if (((op >= 153 && op <= 168) || op == 198 || op == 199) && pc + 2 < c.size()) {
            const int t = int(pc) + int(int16_t((c[pc + 1] << 8) | c[pc + 2]));
            if (t >= 0 && size_t(t) <= pc) for (int w : waits) if (w >= t && size_t(w) <= pc) { for (size_t q = size_t(t); q <= pc; ++q) rm.banned[q] = 1; break; }
        }
        pc += size_t(len);
    }
}
// 0.2.112 F3: one line written just before an operand-stack underflow aborts the VM (no switch, no recovery).
static void underflow_log(const char* kind, const Frame& fr, int need, const std::string& callee) {
    const std::string who = fr.method ? VMS_CL(g_classes)[fr.method->class_id].name + "." + fr.method->name + fr.method->desc : std::string("-");
    const int op = fr.method && fr.op_pc >= 0 && size_t(fr.op_pc) < fr.method->code.size() ? int(fr.method->code[size_t(fr.op_pc)]) : -1;
    const auto* frames = VMS_EXEC(g_active_stack);
    const JitrLastExit& x = g_jitr_last_exit;
    log_line(std::string("UNDERFLOW kind=") + kind + " method=" + who + " op_pc=" + std::to_string(fr.op_pc) + " op=" + std::to_string(op) +
             " callee=" + (callee.empty() ? std::string("-") : callee) + " stack=" + std::to_string(fr.stack.size()) + " need=" + std::to_string(need) +
             " frames=" + std::to_string(frames ? frames->size() : 0) + " paint_depth=" + std::to_string(VMS_RS(g_paint_depth)) +
             " lastjit=" + (x.m ? VMS_CL(g_classes)[x.m->class_id].name + "." + x.m->name + x.m->desc : std::string("-")) +
             " exit_pc=" + std::to_string(x.exit_pc) + " exit_depth=" + std::to_string(x.exit_depth) +
             " entry_depth=" + std::to_string(x.entry_depth) + " done=" + std::to_string(x.done));
}
// 0.2.113: one JITSTALE line per caught stale or mismatched compiled-code event (region and leaf code alike).
// reason=id_mismatch: the code at the called address carries another install id (it never ran an op);
// reason=bad_exit: guarded code returned an exit pc this region cannot produce (results discarded, stack untouched).
// 0.2.114 rate limit (log volume only; interception and recompilation are unchanged): per session the first
// JitStaleFullLines events are written in full, then one JITSTALE_SUMMARY line per JitStaleSummaryEvery events.
// force=true lines (the fatal bad_exit line) are always written. g_jitstale_lines counts every event (n=).
static uint64_t g_jitstale_lines = 0;
constexpr uint64_t JitStaleFullLines = 20, JitStaleSummaryEvery = 100;
struct JitStaleLimit { uint64_t session = 0, pending = 0, region_id = 0, region_bad = 0, leaf_id = 0, leaf_failed = 0, other = 0, shown = 0, suppressed = 0, summaries = 0; std::string last; };
static JitStaleLimit g_jitstale_limit;
static void jitstale_summary(bool partial) {
    auto& L = g_jitstale_limit;
    log_line("JITSTALE_SUMMARY events=" + std::to_string(L.session) + " suppressed=" + std::to_string(L.pending) +
             " region_id_mismatch=" + std::to_string(L.region_id) + " region_bad_exit=" + std::to_string(L.region_bad) +
             " leaf_id_mismatch=" + std::to_string(L.leaf_id) + " leaf_reinstall_failed=" + std::to_string(L.leaf_failed) + " other=" + std::to_string(L.other) +
             " last=" + (L.last.empty() ? std::string("-") : L.last) + " jit_type=" + jit236::jit_type_name() + " partial=" + (partial ? "1" : "0") + " n=" + std::to_string(g_jitstale_lines));
    ++L.summaries; L.pending = L.region_id = L.region_bad = L.leaf_id = L.leaf_failed = L.other = 0;
}
static void jitstale_session_reset() {
    if (g_jitstale_limit.pending) jitstale_summary(true);   // the remainder is never lost
    g_jitstale_limit = {};
}
static void jitstale_log(const char* kind, const char* reason, const Method* m, int entry_pc, int exit_pc, int done, uint32_t expected, uint32_t got, const char* action, bool force) {
    ++g_jitstale_lines; auto& L = g_jitstale_limit; ++L.session;
    const std::string who = m ? VMS_CL(g_classes)[m->class_id].name + "." + m->name + m->desc : std::string("-");
    if (force || L.session <= JitStaleFullLines) {
        ++L.shown;
        char ids[64]; std::snprintf(ids, sizeof ids, "expected_id=%08x got_id=%08x", expected, got);
        log_line(std::string("JITSTALE kind=") + kind + " reason=" + reason + " method=" + who +
                 " entry_pc=" + std::to_string(entry_pc) + " exit_pc=" + (exit_pc < 0 ? std::string("-") : std::to_string(exit_pc)) + " done=" + std::to_string(done) + " " + ids +
                 " action=" + action + " jit_type=" + jit236::jit_type_name() + " n=" + std::to_string(g_jitstale_lines) + " s=" + std::to_string(L.session));
        return;
    }
    const bool region = std::strcmp(kind, "region") == 0;
    if (region && std::strcmp(reason, "id_mismatch") == 0) ++L.region_id;
    else if (region && std::strcmp(reason, "bad_exit") == 0) ++L.region_bad;
    else if (!region && std::strcmp(action, "interpret_reinstall_failed") == 0) ++L.leaf_failed;
    else if (!region) ++L.leaf_id;
    else ++L.other;
    L.last = who + "@" + std::to_string(entry_pc) + ":" + reason;
    ++L.suppressed;
    if (++L.pending >= JitStaleSummaryEvery) jitstale_summary(false);
}
// Runs a compiled region from fr.pc (operand stack must be empty). Returns ops executed (0 = nothing done).
static int jit_region_try(Frame& fr, int budget) {
    auto& eng = VMS_NATIVE(g_native_jit); if (!eng.enabled || !eng.available || budget <= 0) return 0;
    if (fr.stack.size() > fr.method->max_stack) return 0;
    for (const Value& v : fr.stack) if (v.is_long || v.l != 0 || v.ref < 0) return 0;   // only plain (i,ref) stack values
    if (g_jit_region_epoch != eng.epoch()) jit_region_reset();
    Method* m = fr.method; if (m->native || fr.pc < 0 || size_t(fr.pc) >= m->code.size()) return 0;
    if (m != g_jit_region_last_m) { g_jit_region_last = &g_jit_regions[m]; g_jit_region_last_m = m; }
    auto& rm = *g_jit_region_last; if (!rm.scanned) jit_region_scan(m, rm);
    if (rm.banned[size_t(fr.pc)] || rm.dead[size_t(fr.pc)]) { jitr_mark_skip(m, fr.pc); ++g_jit_region_stats.skip_marked; return 0; }   // O(1) reject for wait loops / retired pcs
    // P6.8 B: once the code budget is full, no new candidates are collected: a pc without compiled code is marked
    // 'do not try' until the next epoch (no budget increase, no eviction).
    const bool budget_full = eng.admission_closed || g_jit_region_stats.resident >= JitRegionMax || g_jit_region_stats.bytes >= JitRegionMaxBytes;
    if (budget_full) { auto it = rm.at.find(fr.pc); if (it == rm.at.end() || it->second.state != 1) { jitr_mark_skip(m, fr.pc); ++g_jit_region_stats.full_skipped; return 0; } }
    auto& e = rm.at[fr.pc];
    if (e.state == 2) { rm.dead[size_t(fr.pc)] = 1; jitr_mark_skip(m, fr.pc); return 0; }
    if (e.entry_depth < 0) e.entry_depth = int16_t(fr.stack.size());
    if (size_t(e.entry_depth) != fr.stack.size()) { e.state = 2; rm.dead[size_t(fr.pc)] = 1; jitr_mark_skip(m, fr.pc); return 0; }   // verifier guarantees one depth
    if (e.state == 1 && e.stale && e.recompiles < 2 && !eng.admission_closed) { e.state = 0; e.stale = false; ++e.recompiles; e.hits = JitRegionHot - 1; ++g_jit_region_t3.recompiles; }
    if (e.state == 0) {
        if (++e.hits < JitRegionHot) return 0;
        e.state = 2;
        const size_t old_bytes=e.exe.size;
        if ((!old_bytes && g_jit_region_stats.resident >= JitRegionMax) || g_jit_region_stats.bytes-old_bytes >= JitRegionMaxBytes) { ++g_jit_region_stats.rejected; return 0; }
        const auto& cp = VMS_CL(g_classes)[m->class_id].cp; std::vector<jit236::Constant> consts(cp.size());
        for (size_t i = 0; i < cp.size(); ++i) if (cp[i].tag == 3) consts[i] = {true, int32_t(cp[i].a)};
        std::vector<int> fslot(m->code.size(), -1);
        for (size_t pc = 0; pc < m->quick.size() && pc < m->code.size(); ++pc) { const auto& q = m->quick[pc];
            if (m->code[pc] == 180 && q.kind == 1 && q.a >= 0 && size_t(q.a) < VMS_CL(g_classes).size() && q.b >= 0) fslot[pc] = q.b;
        }
        std::vector<jitr::IcSite> ics(m->code.size()); std::vector<int> casts(m->code.size(), -1);
        for (size_t pc = 0; pc < m->code.size(); ) { const int op = m->code[pc]; const int len = jvm_op_length(m->code, pc); if (len <= 0) break;
            if ((op == 182 || op == 183) && pc + 2 < m->code.size()) { ics[pc] = jitr_ic_for(m, int(pc)); if (ics[pc].kind) ++g_jit_region_t3.ic_sites; }
            if ((op == 192 || op == 193) && pc + 2 < m->code.size()) { const std::string nm = cp_class(VMS_CL(g_classes)[m->class_id], (m->code[pc + 1] << 8) | m->code[pc + 2]);
                if (!nm.empty() && nm[0] != '[') { casts[pc] = int(g_jitr_casts.size()); g_jitr_casts.push_back({nm, -1, -1}); ++g_jit_region_t3.cast_sites; } }
            pc += size_t(len); }
        jitr::Spec s; s.code = m->code.data(); s.size = m->code.size(); s.entry = fr.pc; s.max_locals = m->max_locals; s.max_stack = m->max_stack; s.constants = &consts; s.field_slot = &fslot;
        s.entry_depth = unsigned(e.entry_depth); s.ic = &ics; s.cast_site = &casts; s.int_array_stores = true;
        jitr::Region r;
        bool ok = jitr::build(s, r);
        // a banned (wait-loop) pc must never be compiled code, so the region stops before it
        if (ok) for (const auto& n : r.ins) if (n.kind == jitr::K_OP && rm.banned[size_t(n.pc)]) { ok = false; ++g_jit_region_stats.banned_wait; break; }
        if (ok) { const jitr::Helpers h{jitr_h_getfield, jitr_h_aload, jitr_h_alen, jitr_h_icget, jitr_h_icchk, jitr_h_cast, jitr_h_inst, jitr_h_icgfx, jitr_h_iastore};
            const uint32_t code_id = jit236::next_install_id(); auto bytes = jitr::emit_host(r, h, code_id);
            const size_t pages=(bytes.size()+4095)&~size_t(4095);
            if(bytes.empty() || pages>JitRegionMaxBytes-(g_jit_region_stats.bytes-old_bytes)) {++g_jit_region_stats.rejected;return 0;}
            // install replaces the old executable, including on allocation failure.
            // a region recompiled after a stale event goes to a never-used page (0.2.113)
            ok=e.exe.install(bytes,code_id,e.fresh_page?jit236::InstallMode::Fresh:jit236::InstallMode::Normal); e.fresh_page=false;
            g_jit_region_stats.bytes-=old_bytes;
            if(old_bytes)--g_jit_region_stats.resident;
            if(!ok){eng.admission_closed=true;++eng.stats.permission_failures;} }
        if (!ok) { ++g_jit_region_stats.rejected; return 0; }
        e.writes = !jitr::region_pure(r);   // 0.2.114: decided per install from the compiled op list (whitelist)
        e.depth = std::move(r.depth); e.stored = std::move(r.stored); e.cast_idx = std::move(casts); e.ic_kind.assign(m->code.size(), 0); for (size_t q = 0; q < ics.size(); ++q) e.ic_kind[q] = ics[q].kind; e.locals = r.locals; e.state = 1;
        ++g_jit_region_stats.compiled; ++g_jit_region_stats.resident; g_jit_region_stats.bytes += e.exe.size;
    }
    // stored locals must hold plain (i,ref) values at entry; the region never writes long/double slots
    if (fr.locals.size() < e.locals) return 0;
    for (unsigned k = 0; k < e.locals; ++k) if (e.stored[k] && (fr.locals[k].is_long || fr.locals[k].l != 0)) return 0;
    static thread_local std::vector<int32_t> slots; const size_t need = 2 + 2 * (size_t(e.locals) + m->max_stack + 1); if (slots.size() < need) slots.resize(need);
    for (unsigned k = 0; k < e.locals; ++k) { slots[2 + 2 * k] = fr.locals[k].i; slots[3 + 2 * k] = fr.locals[k].ref; }
    for (size_t d = 0; d < fr.stack.size(); ++d) { slots[2 + 2 * (e.locals + d)] = fr.stack[d].i; slots[3 + 2 * (e.locals + d)] = fr.stack[d].ref; }
    g_jitr_slots = slots.data(); g_jitr_locals = e.locals;
    if(!e.exe.executable()){g_jitr_slots=nullptr;return 0;}
    slots[1] = int32_t(e.exe.id);   // 0.2.113: the code must carry this id, else it bails before its first op
    uint64_t rc;
    {jit236::CodeCall active;rc=reinterpret_cast<jitr::Fn>(e.exe.fn)(slots.data(),uint32_t(budget));}
    g_jitr_slots = nullptr;
    // 0.2.113: stale / mismatched code. Nothing is copied back (frame locals and operand stack stay exactly as they
    // were), the interpreter continues at fr.pc, and the region is recompiled onto a never-used page at its next entry.
    auto stale_recompile = [&](const char* reason, int xpc, int xdone, uint32_t got) {
        ++e.stale_events; if (std::strcmp(reason, "id_mismatch") == 0) ++g_jit_region_stats.stale_id; else ++g_jit_region_stats.stale_exit;
        jitstale_log("region", reason, m, fr.pc, xpc, xdone, e.exe.id, got, "interpret_and_recompile_fresh_page");
        g_jit_region_stats.bytes -= e.exe.size; --g_jit_region_stats.resident; e.exe.close();
        e.state = 0; e.hits = JitRegionHot - 1; e.fresh_page = true; e.runs = 0; e.run_ops = 0; e.maj_pc = -1; e.maj_cnt = 0; e.inv_exits = 0;
        ++g_jit_region_stats.stale_recompiles;
        return 0; };
    if (jitr::stale_result(rc)) return stale_recompile("id_mismatch", -1, 0, uint32_t(rc));
    const int exit_pc = int(uint32_t(rc)), done = int(rc >> 32);
    if (exit_pc < 0 || size_t(exit_pc) >= e.depth.size() || done < 0 || done > budget || (done > 0 && e.depth[size_t(exit_pc)] < 0)) {
        // 0.2.114: done != 0 (incl. a count outside [0,budget], which cannot be trusted) means ops may have run. A region
        // that may have stored to int[] or drawn must never be executed a second time by the interpreter: stop honestly.
        if (jitr::bad_exit_must_fail(!e.writes, done)) {
            ++g_jit_region_stats.stale_fatal;
            jitstale_log("region", "bad_exit", m, fr.pc, exit_pc, done, e.exe.id, e.exe.id, "fail_vm_side_effects", true);
            die("jit region bad_exit after side effects in " + VMS_CL(g_classes)[m->class_id].name + "." + m->name + m->desc +
                " entry_pc=" + std::to_string(fr.pc) + " exit_pc=" + std::to_string(exit_pc) + " done=" + std::to_string(done));
        }
        return stale_recompile("bad_exit", exit_pc, done, e.exe.id);   // pure region or done==0: as 0.2.113
    }
#if defined(J2ME_JITR_EXITS)
    jitr_exit_hook(m, exit_pc, done, budget, slots.data(), e.locals, e.depth, e.ic_kind);   // test-side histogram only
#endif
    ++g_jit_region_stats.entries;
    // Regions that average < 8 ops per entry cost more in entry/exit glue than they save: retire them (perf-only;
    // the interpreter's fast batch runs the same ops with identical semantics).
    ++e.runs; e.run_ops += uint64_t(done > 0 ? done : 0);
    // P6.7 per-region adaptive retire (perf-only; state lives only in g_jit_regions, never in J2S5, cleared by the epoch):
    // track the region's majority exit pc (Boyer-Moore) and how many exits land on an invoke compiled without IC.
    if (e.runs <= 256) {
        if (exit_pc == e.maj_pc) ++e.maj_cnt; else if (e.maj_cnt == 0) { e.maj_pc = exit_pc; e.maj_cnt = 1; } else --e.maj_cnt;
        if (size_t(exit_pc) < m->code.size() && size_t(exit_pc) < e.ic_kind.size() && !e.ic_kind[size_t(exit_pc)]) {
            const int xo = m->code[size_t(exit_pc)]; if (xo >= 182 && xo <= 185) ++e.inv_exits; } }
    if (e.runs == 256) {
        if (e.run_ops < 8u * 256u) { e.state = 2; rm.dead[size_t(fr.pc)] = 1; jitr_mark_skip(m, fr.pc); ++g_jit_region_stats.retired_short; }
        else if (!e.stale && e.inv_exits >= 192u && e.maj_pc >= 0 && size_t(e.maj_pc) < m->code.size() && m->code[size_t(e.maj_pc)] >= 182 && m->code[size_t(e.maj_pc)] <= 185 &&
                 e.run_ops < uint64_t(JitRegionInvokeRetireOps) * 256u) {
            // >= 75% of this region's entries end at one unsupported call and each entry does little work: the entry/exit
            // glue around that call costs more than the compiled ops save, so this region alone goes back to the interpreter.
            e.state = 2; rm.dead[size_t(fr.pc)] = 1; jitr_mark_skip(m, fr.pc); ++g_jit_region_stats.retired_invoke; } }
    if(e.state==2 && e.exe.size){g_jit_region_stats.bytes-=e.exe.size;--g_jit_region_stats.resident;e.exe.close();}
    if (done <= 0) { ++g_jit_region_stats.zero; return 0; }
    for (unsigned k = 0; k < e.locals; ++k) if (e.stored[k]) { Value v; v.i = slots[2 + 2 * k]; v.ref = slots[3 + 2 * k]; fr.locals[k] = v; }
    if (e.entry_depth > 0) ++g_jit_region_t3.stack_entries;
    const int depth = e.depth[size_t(exit_pc)];   // >= 0: checked above, never a silent stack clear (0.2.113)
    fr.stack.clear();
    for (int d = 0; d < depth; ++d) { Value v; v.i = slots[2 + 2 * (e.locals + unsigned(d))]; v.ref = slots[3 + 2 * (e.locals + unsigned(d))]; fr.stack.push_back(v); }
    fr.op_pc = slots[0]; fr.pc = exit_pc;
    g_jitr_last_exit.m = m; g_jitr_last_exit.exit_pc = exit_pc; g_jitr_last_exit.exit_depth = depth; g_jitr_last_exit.entry_depth = e.entry_depth; g_jitr_last_exit.done = done;
    // A call site that was still unresolved (cold QuickSite) when the region was compiled: once the interpreter has
    // resolved it, recompile the region (at most twice) so the site can use the monomorphic IC.
    if (!e.stale && e.recompiles < 2 && size_t(exit_pc) < e.ic_kind.size() && !e.ic_kind[size_t(exit_pc)]) {
        const int xo = m->code[size_t(exit_pc)];
        if ((xo == 182 || xo == 183) && jitr_ic_for(m, exit_pc).kind != jitr::IC_NONE) e.stale = true; }
    if (depth > 0 && size_t(exit_pc) < e.cast_idx.size() && e.cast_idx[size_t(exit_pc)] >= 0)   // learn before interpreter runs it
        jitr_learn_cast(e.cast_idx[size_t(exit_pc)], slots[3 + 2 * (e.locals + unsigned(depth - 1))]); g_jit_region_stats.ops += uint64_t(done); eng.stats.ops += uint64_t(done);
    return done;
}

// ---------------- P6.5: region JIT startup self-test (real compile + execute, known results) ----------------
// Returns true when the region backend compiled and executed two known programs with exact results:
//  (1) counted loop sum 0..n-1 (back-edge, iinc, iadd, if_icmpge) and a budget-limited run of the same code;
//  (2) aconst_null; getfield -> must bail at the getfield pc without executing it (exception stays in interpreter).
static int g_jit_region_selftest_fault = 0;   // desktop fault injection only (J2ME_JIT_FAULT=region)
static bool jit_region_self_test(std::string& why) {
    if (g_jit_region_selftest_fault) { why = "fault_injected_region"; return false; }
    const std::vector<uint8_t> code = {3, 60, 3, 61, 28, 26, 162, 0, 13, 27, 28, 96, 60, 132, 2, 1, 167, 0xff, 0xf4, 27, 172};
    std::vector<jit236::Constant> consts; std::vector<int> fslot(code.size(), -1);
    jitr::Spec s; s.code = code.data(); s.size = code.size(); s.entry = 0; s.max_locals = 3; s.max_stack = 2; s.constants = &consts; s.field_slot = &fslot;
    jitr::Region r; if (!jitr::build(s, r)) { why = "region_build_failed"; return false; }
    const jitr::Helpers h{jitr_h_getfield, jitr_h_aload, jitr_h_alen, jitr_h_icget, jitr_h_icchk, jitr_h_cast, jitr_h_inst, jitr_h_icgfx};
    // 0.2.113: self-test code is guarded and lives in a dedicated block whose pages never hold game code.
    const uint32_t id1 = jit236::next_install_id();
    auto bytes = jitr::emit_host(r, h, id1); if (bytes.empty()) { why = "region_emit_failed"; return false; }
    jit236::Executable exe; if (!exe.install(bytes, id1, jit236::InstallMode::Dedicated)) { why = std::string("region_install_failed:") + jit236::Executable::last_error(); return false; }
    auto run = [&](int32_t n, uint32_t budget, std::vector<int32_t>& slots, uint32_t id) { slots.assign(2 + 2 * (3 + 2 + 1), 0); slots[1] = int32_t(id); slots[2] = n; return reinterpret_cast<jitr::Fn>(exe.fn)(slots.data(), budget); };
    std::vector<int32_t> sl;
    uint64_t rc = run(10, 1000, sl, id1);
    if (uint32_t(rc) != 20 || sl[2 + 2 * 1] != 45 || sl[2 + 2 * 2] != 10 || sl[2 + 2 * 3] != 45) { why = "region_loop_result_mismatch"; return false; }
    rc = run(10, 5, sl, id1);
    if (uint32_t(rc >> 32) != 5) { why = "region_budget_mismatch"; return false; }
    {   // 0.2.113: the guard must bail with the stale mark and the code's own id, before touching any slot
        rc = run(10, 1000, sl, id1 ^ 0x5a5a5a5au); bool untouched = sl[0] == 0 && sl[2] == 10;
        for (size_t k = 3; k < sl.size(); ++k) if (k != 1 && sl[k] != 0) untouched = false;
        if (!jitr::stale_result(rc) || uint32_t(rc) != id1 || !untouched) { why = "region_guard_mismatch"; return false; } }
    const std::vector<uint8_t> code2 = {1, 180, 0, 1, 172}; std::vector<int> fs2(code2.size(), -1); fs2[1] = 0;
    jitr::Spec s2; s2.code = code2.data(); s2.size = code2.size(); s2.entry = 0; s2.max_locals = 1; s2.max_stack = 1; s2.constants = &consts; s2.field_slot = &fs2;
    jitr::Region r2; if (!jitr::build(s2, r2)) { why = "region_build2_failed"; return false; }
    const uint32_t id2 = jit236::next_install_id();
    auto b2 = jitr::emit_host(r2, h, id2); jit236::Executable e2; if (b2.empty() || !e2.install(b2, id2, jit236::InstallMode::Dedicated)) { why = std::string("region_install2_failed:") + jit236::Executable::last_error(); return false; }
    std::vector<int32_t> s3(2 + 2 * 3, 0); s3[1] = int32_t(id2); rc = reinterpret_cast<jitr::Fn>(e2.fn)(s3.data(), 100);
    if (uint32_t(rc) != 1 || uint32_t(rc >> 32) != 1) { why = "region_null_bail_mismatch"; return false; }
    why = "ok"; return true;
}
