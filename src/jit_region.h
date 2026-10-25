#pragma once
// P6.2 T2 / P6.3 T3: region JIT (+ T3: non-empty-stack entry, checkcast/instanceof, monomorphic IC inlining of
// trivial Java instance methods). Native callees (e.g. Graphics.fillRect) are NEVER called from compiled code:
// region slots are not GC roots and natives may allocate, suspend or throw, so those sites always exit. A region starts at a bytecode pc where the operand stack is empty and runs native code with
// EXACTLY the semantics of execute_fast_batch (interpreter_fast.inc): bounded by the caller's op budget, no Java
// calls, no allocation, no monitors, no GC. Opt-in iastore uses a checked helper and the existing array write
// barrier; other heap/statics writes stay interpreted. An unsupported or failing op (null, bounds) exits BEFORE executing it, at that
// op's pc, with locals/stack materialised, so the interpreter re-executes it and owns every exception.
// Values are (i, ref) int32 pairs, identical to Value{i,ref,l=0,is_long=0}. Heap reads go through C++ helpers.
#include "native_jit236.h"
#include <vector>
#include <cstdint>
namespace jitr {
using jit236::Bytes; using jit236::Constant;
// Helper ABI: returns (uint64(ref)<<32)|uint32(i); 0xFFFFFFFF in the high half = bail (op not executed).
using Helper = uint64_t (*)(int32_t a, int32_t b, int32_t c);
struct Helpers { Helper getfield = nullptr, aload = nullptr, alen = nullptr;
    Helper icget = nullptr, icchk = nullptr, cast = nullptr, inst = nullptr, icgfx = nullptr;
    Helper iastore = nullptr; };
// T3 monomorphic IC. A call site whose receiver class was seen once (the interpreter's own QuickSite cache) is
// compiled as a class guard plus the inlined body of a trivial callee. Guard miss / null -> exit at the invoke pc
// (not executed), so the interpreter dispatches or throws. cost = 1 (invoke) + callee bytecodes (incl. return),
// exactly the ops the interpreter would count, and the whole sequence must fit the remaining budget.
enum IcKind : uint8_t { IC_NONE = 0, IC_GET = 1, IC_CONST = 2, IC_VOID = 3, IC_SELF = 4, IC_GFX = 6 };
struct IcSite { uint8_t kind = IC_NONE, argc = 0, cost = 0; int32_t cls = -1, slot = 0, value = 0; };
constexpr uint64_t Bail = 0xFFFFFFFF00000000ull;
inline uint64_t pack(int32_t i, int32_t ref) { return (uint64_t(uint32_t(ref)) << 32) | uint32_t(i); }
struct Spec {
    const uint8_t* code = nullptr; size_t size = 0; int entry = 0; unsigned max_locals = 0, max_stack = 0;
    const std::vector<Constant>* constants = nullptr;   // int CP entries (ldc/ldc_w)
    const std::vector<int>* field_slot = nullptr;       // per pc: resolved instance-field slot for getfield, -1 = unresolved
    unsigned entry_depth = 0;                           // T3: operand stack depth at entry (values copied in by the glue)
    const std::vector<IcSite>* ic = nullptr;            // T3: per pc, invokevirtual/invokespecial IC (kind 0 = exit)
    const std::vector<int>* cast_site = nullptr;        // T3: per pc, checkcast/instanceof cache index (-1 = exit)
    bool int_array_stores = false;                     // requires Helpers::iastore; no reference-array writes
};
enum Kind : uint8_t { K_NONE = 0, K_OP = 1, K_EXIT = 2 };
struct Ins { int pc = 0, next = 0, op = 0, index = 0, target = -1, depth = 0; int32_t value = 0, aux = 0; unsigned cost = 1, argc = 0; Kind kind = K_NONE;
    std::vector<std::pair<int32_t, int>> sw; };   // P6.7: tableswitch/lookupswitch (key -> target pc); target = default
constexpr size_t MaxSwitchCases = 64;
inline int32_t s32(const uint8_t* p) { return int32_t((uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3])); }
constexpr int OP_IC = 0x100;   // synthetic: OP_IC + IcKind
struct Region {
    std::vector<Ins> ins; std::vector<int16_t> depth;   // depth[pc] for every reached pc (-1 = not in region)
    std::vector<uint8_t> stored;                        // locals written by the region
    unsigned locals = 0, stack = 0; int ops = 0;        // ops = compiled (non-exit) instructions
};
constexpr unsigned MaxLocals = 255, MaxStack = 64; constexpr size_t MaxIns = 2048;
inline int s16(const uint8_t* p) { int v = (p[0] << 8) | p[1]; return v < 32768 ? v : v - 65536; }
inline bool load_op(int op) { return op == 21 || op == 25 || (op >= 26 && op <= 29) || (op >= 42 && op <= 45); }
inline bool store_op(int op) { return op == 54 || op == 58 || (op >= 59 && op <= 62) || (op >= 75 && op <= 78); }
inline bool cond1(int op) { return (op >= 153 && op <= 158) || op == 198 || op == 199; }
inline bool cond2(int op) { return op >= 159 && op <= 166; }
// Builds the region reachable from s.entry. false = do not compile (inconsistent depth, too big, malformed).
inline bool build(const Spec& s, Region& r) {
    r = Region{}; if (!s.code || !s.size || s.size > 65535 || size_t(s.entry) >= s.size) return false;
    if (s.max_locals > MaxLocals || s.max_stack > MaxStack || s.entry_depth > s.max_stack) return false;
    r.locals = s.max_locals; r.stack = s.max_stack; r.depth.assign(s.size, -1); r.stored.assign(s.max_locals, 0);
    std::vector<int> work{s.entry}; r.depth[size_t(s.entry)] = int16_t(s.entry_depth); std::vector<int> at(s.size, -1);
    auto send = [&](int pc, int d) { if (pc < 0 || size_t(pc) >= s.size) return false;
        if (r.depth[size_t(pc)] < 0) { r.depth[size_t(pc)] = int16_t(d); work.push_back(pc); return true; } return r.depth[size_t(pc)] == d; };
    while (!work.empty()) {
        const int pc = work.back(); work.pop_back(); if (at[size_t(pc)] >= 0) continue;
        if (r.ins.size() >= MaxIns) return false;
        Ins n; n.pc = pc; n.op = s.code[size_t(pc)]; n.depth = r.depth[size_t(pc)]; const int d = n.depth;
        auto have = [&](size_t k) { return size_t(pc) + k < s.size; };
        int len = 1, need = 0, delta = 0; bool ok = true;
        const int op = n.op;
        if (op >= 2 && op <= 8) { n.value = op - 3; delta = 1; }
        else if (op == 1) { delta = 1; }
        else if (op == 16) { if (!have(1)) return false; n.value = int8_t(s.code[pc + 1]); len = 2; delta = 1; }
        else if (op == 17) { if (!have(2)) return false; n.value = s16(s.code + pc + 1); len = 3; delta = 1; }
        else if (op == 18 || op == 19) { if (!have(op == 18 ? 1 : 2)) return false; unsigned x = op == 18 ? s.code[pc + 1] : unsigned((s.code[pc + 1] << 8) | s.code[pc + 2]);
            len = op == 18 ? 2 : 3; if (s.constants && x < s.constants->size() && (*s.constants)[x].integer) { n.value = (*s.constants)[x].value; delta = 1; } else ok = false; }
        else if (op == 21 || op == 25 || op == 54 || op == 58) { if (!have(1)) return false; n.index = s.code[pc + 1]; len = 2; if (op == 21 || op == 25) delta = 1; else { need = 1; delta = -1; } }
        else if ((op >= 26 && op <= 29) || (op >= 42 && op <= 45)) { n.index = (op - 26) % 4; delta = 1; }
        else if ((op >= 59 && op <= 62) || (op >= 75 && op <= 78)) { n.index = (op - 59) % 4; need = 1; delta = -1; }
        else if (op == 132) { if (!have(2)) return false; n.index = s.code[pc + 1]; n.value = int8_t(s.code[pc + 2]); len = 3; }
        else if (op == 46 || op == 48 || op == 50 || op == 51 || op == 52 || op == 53) { need = 2; delta = -1; }
        else if (op == 79 && s.int_array_stores) { need = 3; delta = -3; }
        else if (op == 190) { need = 1; }
        else if (op == 180) { if (!have(2)) return false; len = 3; need = 1;
            if (!s.field_slot || size_t(pc) >= s.field_slot->size() || (*s.field_slot)[size_t(pc)] < 0) ok = false; else n.index = (*s.field_slot)[size_t(pc)]; }
        else if (op == 87) { need = 1; delta = -1; }
        else if (op == 89) { need = 1; delta = 1; }
        else if (op == 95) { need = 2; }
        else if (op == 96 || op == 100 || op == 104 || op == 120 || op == 122 || op == 124 || op == 126 || op == 128 || op == 130) { need = 2; delta = -1; }
        else if (op == 116 || (op >= 145 && op <= 147)) { need = 1; }
        else if (cond1(op)) { if (!have(2)) return false; len = 3; need = 1; delta = -1; n.target = pc + s16(s.code + pc + 1); }
        else if (cond2(op)) { if (!have(2)) return false; len = 3; need = 2; delta = -2; n.target = pc + s16(s.code + pc + 1); }
        else if (op == 167) { if (!have(2)) return false; len = 3; n.target = pc + s16(s.code + pc + 1); }
        else if (op == 192 || op == 193) { if (!have(2)) return false; len = 3; need = 1;
            if (!s.cast_site || size_t(pc) >= s.cast_site->size() || (*s.cast_site)[size_t(pc)] < 0) ok = false; else n.index = (*s.cast_site)[size_t(pc)]; }
        else if (op == 182 || op == 183) { if (!have(2)) return false; len = 3;
            const IcSite* ic = s.ic && size_t(pc) < s.ic->size() ? &(*s.ic)[size_t(pc)] : nullptr;
            if (!ic || ic->kind == IC_NONE || ic->cost < 1) ok = false;
            else { n.op = OP_IC + ic->kind; n.argc = ic->argc; n.cost = ic->cost; n.aux = ic->cls; n.index = ic->slot; n.value = ic->value;
                need = int(ic->argc) + 1;
                if (ic->kind == IC_VOID || ic->kind == IC_GFX) delta = -need;
                else if (ic->kind == IC_SELF || ic->kind == IC_GET || ic->kind == IC_CONST) delta = -int(ic->argc);
                else ok = false; } }
        else if (op == 170 || op == 171) {   // P6.7: switch = one op, pops the int key, jumps (no fall-through)
            const size_t base = (size_t(pc) + 4) & ~size_t(3); need = 1; delta = -1;
            if (base + 8 > s.size) return false; n.target = pc + s32(s.code + base);
            if (op == 170) { const int32_t lo = s32(s.code + base + 4); if (base + 12 > s.size) return false; const int32_t hi = s32(s.code + base + 8);
                if (hi < lo) return false; const int64_t cnt = int64_t(hi) - int64_t(lo) + 1; if (base + 12 + size_t(4 * cnt) > s.size) return false;
                len = int(base + 12 + size_t(4 * cnt) - size_t(pc));
                if (size_t(cnt) > MaxSwitchCases) ok = false;
                else for (int64_t k = 0; k < cnt; ++k) n.sw.push_back({int32_t(int64_t(lo) + k), pc + s32(s.code + base + 12 + size_t(4 * k))}); }
            else { const int32_t np = s32(s.code + base + 4); if (np < 0 || base + 8 + size_t(8) * size_t(np) > s.size) return false;
                len = int(base + 8 + size_t(8) * size_t(np) - size_t(pc));
                if (size_t(np) > MaxSwitchCases) ok = false;
                else for (int32_t k = 0; k < np; ++k) n.sw.push_back({s32(s.code + base + 8 + size_t(8 * k)), pc + s32(s.code + base + 12 + size_t(8 * k))}); } }
        else ok = false;   // everything else (calls, returns, writes, new, throw, long/float/double, switch, monitors...)
        n.next = pc + len;
        if (ok && (load_op(op) || store_op(op) || op == 132) && unsigned(n.index) >= s.max_locals) ok = false;
        if (ok && (d < need || unsigned(d + delta) > s.max_stack)) return false;   // malformed for verified code
        if (!ok) { n.kind = K_EXIT; at[size_t(pc)] = int(r.ins.size()); r.ins.push_back(n); continue; }
        n.kind = K_OP; r.ops += int(n.cost);
        if (store_op(op) || op == 132) r.stored[size_t(n.index)] = 1;
        at[size_t(pc)] = int(r.ins.size()); r.ins.push_back(n);
        const int nd = d + delta;
        if (n.target >= 0 && !send(n.target, nd)) return false;
        for (const auto& c : r.ins.back().sw) if (!send(c.second, nd)) return false;
        if (op != 167 && op != 170 && op != 171 && !send(n.next, nd)) return false;
    }
    return r.ops > 0;
}
// Slot layout (int32): [0]=last executed pc (out), [1]=install id of the code the caller means to run (0.2.113 guard); local k = [2+2k]{i,ref}; stack d = [2+2*(L+d)].
inline unsigned off_local(const Region& r, unsigned k) { (void)r; return 8 + 8 * k; }
inline unsigned off_stack(const Region& r, unsigned d) { return 8 + 8 * (r.locals + d); }
// Native function: uint64 fn(int32* slots, uint32 budget) -> (ops_executed<<32) | exit_pc. Never executes more than budget.
using Fn = uint64_t (*)(int32_t*, uint32_t);
inline bool binop(int op) { return op == 96 || op == 100 || op == 104 || op == 120 || op == 122 || op == 124 || op == 126 || op == 128 || op == 130; }
// 0.2.114: side-effect classification of a built region. WHITELIST: an op is pure only if it reads the heap or
// writes nothing but the region's own local/stack slot copies (which a bad exit discards). iastore (79) and the
// fillRect IC (OP_IC+IC_GFX) write outside the frame; any op not listed here counts as a write as well.
inline bool pure_op(const Ins& n) {
    const int op = n.op;
    if (op >= OP_IC) return op == OP_IC + IC_GET || op == OP_IC + IC_CONST || op == OP_IC + IC_VOID || op == OP_IC + IC_SELF;
    if ((op >= 1 && op <= 8) || (op >= 16 && op <= 19) || op == 21 || op == 25 || (op >= 26 && op <= 29) || (op >= 42 && op <= 45)) return true;      // consts, loads
    if (op == 54 || op == 58 || (op >= 59 && op <= 62) || (op >= 75 && op <= 78) || op == 132) return true;                  // local stores, iinc
    if (op == 46 || op == 48 || op == 50 || op == 51 || op == 52 || op == 53 || op == 190 || op == 180) return true;          // heap reads
    if (op == 87 || op == 89 || op == 95 || binop(op) || op == 116 || (op >= 145 && op <= 147)) return true;                 // stack / int math
    if (cond1(op) || cond2(op) || op == 167 || op == 170 || op == 171 || op == 192 || op == 193) return true;               // control, casts
    return false;
}
inline bool region_pure(const Region& r) { for (const auto& n : r.ins) if (n.kind == K_OP && !pure_op(n)) return false; return true; }
// A bad exit may be handed back to the interpreter unless the region can write AND some op may have run (done != 0;
// a negative or over-budget count is untrusted and counts as "may have run").
inline bool bad_exit_must_fail(bool pure, int done) { return !pure && done != 0; }

// ---------------- x86-64 SysV ----------------
// 0.2.113 install-id guard (id != 0): the first thing the code does is compare slots[1] with the id compiled into it.
// On mismatch it returns (StaleMark<<32 | own id) without executing any op, calling any helper or writing any slot.
constexpr uint32_t StaleMark = 0xffffffffu;
inline bool stale_result(uint64_t rc) { return uint32_t(rc >> 32) == StaleMark; }
inline std::vector<uint8_t> emit_x64(const Region& r, const Helpers& h, uint32_t id = 0) {
    Bytes o; std::vector<size_t> label(r.depth.size(), SIZE_MAX), stub(r.depth.size(), SIZE_MAX);
    struct Fix { size_t at; int pc; bool to_stub; }; std::vector<Fix> fix;
    auto ld = [&](int reg, unsigned off) { o.b(0x8b); o.b(uint8_t(0x83 | (reg << 3))); o.u32(off); };  // mov r32,[rbx+off]
    auto st = [&](int reg, unsigned off) { o.b(0x89); o.b(uint8_t(0x83 | (reg << 3))); o.u32(off); };
    auto movi = [&](int reg, uint32_t v) { o.b(uint8_t(0xb8 + reg)); o.u32(v); };
    auto jcc_stub = [&](uint8_t cc, int pc) { o.b(0x0f); o.b(cc); fix.push_back({o.v.size(), pc, true}); o.u32(0); };
    auto jmp_stub = [&](int pc) { o.b(0xe9); fix.push_back({o.v.size(), pc, true}); o.u32(0); };
    auto jcc_lab = [&](uint8_t cc, int pc) { o.b(0x0f); o.b(cc); fix.push_back({o.v.size(), pc, false}); o.u32(0); };
    auto jmp_lab = [&](int pc) { o.b(0xe9); fix.push_back({o.v.size(), pc, false}); o.u32(0); };
    // prologue: push rbx,rbp,r12,r13,r14,r15 ; sub rsp,8 ; rbx=rdi ; r13d=esi ; r12d=0 ; r14d=-1 (last pc)
    for (uint8_t b : {0x53, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x08, 0x48, 0x89, 0xfb, 0x41, 0x89, 0xf5, 0x45, 0x31, 0xe4}) o.b(b);
    o.b(0x41); o.b(0xbe); o.u32(0xffffffffu);
    size_t guard_at = SIZE_MAX;
    if (id) { o.b(0x8b); o.b(0x43); o.b(0x04); o.b(0x3d); o.u32(id); o.b(0x0f); o.b(0x85); guard_at = o.v.size(); o.u32(0); }   // mov eax,[rbx+4]; cmp eax,id; jne stale
    jmp_lab(r.ins.empty() ? 0 : -1);   // patched below to the entry label
    const size_t entry_fix = fix.size() - 1;
    auto helper_call = [&](Helper fn) { o.b(0x48); o.b(0xb8); uint64_t a = uint64_t(reinterpret_cast<uintptr_t>(fn)); for (int i = 0; i < 8; ++i) o.b(uint8_t(a >> (8 * i))); o.b(0xff); o.b(0xd0); };
    for (const auto& n : r.ins) {
        label[size_t(n.pc)] = o.v.size();
        if (n.kind == K_EXIT) { jmp_stub(n.pc); continue; }
        // budget: if (count >= budget) exit before this op
        if (n.cost == 1) { o.b(0x45); o.b(0x39); o.b(0xec); jcc_stub(0x83, n.pc);   // cmp r12d,r13d ; jae stub
            o.b(0x41); o.b(0xff); o.b(0xc4); }                                         // inc r12d
        else { o.b(0x41); o.b(0x8d); o.b(0x84); o.b(0x24); o.u32(n.cost);              // lea eax,[r12+cost]
            o.b(0x44); o.b(0x39); o.b(0xe8); jcc_stub(0x87, n.pc);                     // cmp eax,r13d ; ja stub
            o.b(0x41); o.b(0x89); o.b(0xc4); }                                         // mov r12d,eax
        const unsigned d = unsigned(n.depth); const int op = n.op;
        auto S = [&](unsigned k) { return off_stack(r, k); }; auto L = [&](unsigned k) { return off_local(r, k); };
        auto bail_check = [&]() {   // rax = helper result; high==-1 -> undo count, exit at this pc
            o.b(0x48); o.b(0x89); o.b(0xc2); o.b(0x48); o.b(0xc1); o.b(0xea); o.b(0x20);   // mov rdx,rax ; shr rdx,32
            o.b(0x83); o.b(0xfa); o.b(0xff);                                                // cmp edx,-1
            size_t skip_at; o.b(0x75); skip_at = o.v.size(); o.b(0);                        // jne ok
            if (n.cost == 1) { o.b(0x41); o.b(0xff); o.b(0xcc); } else { o.b(0x41); o.b(0x81); o.b(0xec); o.u32(n.cost); }   // undo count
            jmp_stub(n.pc);
            o.v[skip_at] = uint8_t(o.v.size() - skip_at - 1);
        };
        if (load_op(op)) { ld(0, L(unsigned(n.index))); ld(2, L(unsigned(n.index)) + 4); st(0, S(d)); st(2, S(d) + 4); }
        else if (store_op(op)) { ld(0, S(d - 1)); ld(2, S(d - 1) + 4); st(0, L(unsigned(n.index))); st(2, L(unsigned(n.index)) + 4); }
        else if ((op >= 2 && op <= 8) || op == 16 || op == 17 || op == 18 || op == 19) { movi(0, uint32_t(n.value)); st(0, S(d)); movi(2, 0); st(2, S(d) + 4); }
        else if (op == 1) { movi(0, 0); st(0, S(d)); st(0, S(d) + 4); }
        else if (op == 132) { ld(0, L(unsigned(n.index))); o.b(0x05); o.u32(uint32_t(n.value)); st(0, L(unsigned(n.index))); }
        else if (op == 87) { }
        else if (op == 89) { ld(0, S(d - 1)); ld(2, S(d - 1) + 4); st(0, S(d)); st(2, S(d) + 4); }
        else if (op == 95) { ld(0, S(d - 1)); ld(2, S(d - 1) + 4); ld(1, S(d - 2)); ld(6, S(d - 2) + 4); st(0, S(d - 2)); st(2, S(d - 2) + 4); st(1, S(d - 1)); st(6, S(d - 1) + 4); }
        else if (binop(op)) { ld(0, S(d - 2)); ld(1, S(d - 1));
            switch (op) { case 96: o.b(0x01); o.b(0xc8); break; case 100: o.b(0x29); o.b(0xc8); break; case 104: o.b(0x0f); o.b(0xaf); o.b(0xc1); break;
            case 120: o.b(0xd3); o.b(0xe0); break; case 122: o.b(0xd3); o.b(0xf8); break; case 124: o.b(0xd3); o.b(0xe8); break;
            case 126: o.b(0x21); o.b(0xc8); break; case 128: o.b(0x09); o.b(0xc8); break; default: o.b(0x31); o.b(0xc8); }
            st(0, S(d - 2)); movi(2, 0); st(2, S(d - 2) + 4); }
        else if (op == 116 || (op >= 145 && op <= 147)) { ld(0, S(d - 1));
            if (op == 116) { o.b(0xf7); o.b(0xd8); } else if (op == 145) { o.b(0x0f); o.b(0xbe); o.b(0xc0); } else if (op == 146) { o.b(0x0f); o.b(0xb7); o.b(0xc0); } else { o.b(0x0f); o.b(0xbf); o.b(0xc0); }
            st(0, S(d - 1)); movi(2, 0); st(2, S(d - 1) + 4); }
        else if (op == 46 || op == 48 || op == 50 || op == 51 || op == 52 || op == 53) {
            ld(7, S(d - 2) + 4); ld(6, S(d - 1)); movi(2, uint32_t(op)); helper_call(h.aload); bail_check(); st(0, S(d - 2)); st(2, S(d - 2) + 4); }
        else if (op == 79) {
            if(!h.iastore)return {};
            ld(7, S(d - 3) + 4); ld(6, S(d - 2)); ld(2, S(d - 1)); helper_call(h.iastore); bail_check(); }
        else if (op == 190) { ld(7, S(d - 1) + 4); movi(6, 0); movi(2, 0); helper_call(h.alen); bail_check(); st(0, S(d - 1)); st(2, S(d - 1) + 4); }
        else if (op == 180) { ld(7, S(d - 1) + 4); movi(6, uint32_t(n.index)); movi(2, 0); helper_call(h.getfield); bail_check(); st(0, S(d - 1)); st(2, S(d - 1) + 4); }
        else if (op == 192) { ld(7, S(d - 1) + 4); movi(6, uint32_t(n.index)); movi(2, 0); helper_call(h.cast); bail_check(); }
        else if (op == 193) { ld(7, S(d - 1) + 4); movi(6, uint32_t(n.index)); movi(2, 0); helper_call(h.inst); bail_check(); st(0, S(d - 1)); movi(2, 0); st(2, S(d - 1) + 4); }
        else if (op > OP_IC) {
            const unsigned rd = d - 1 - n.argc;
            if (op == OP_IC + IC_GFX) {
                ld(7, S(rd) + 4); movi(6, uint32_t(n.aux)); movi(2, uint32_t(n.value) | (uint32_t(n.argc) << 8) | (uint32_t(d) << 16));
                helper_call(h.icgfx); bail_check();
            } else {
                ld(7, S(rd) + 4); movi(6, uint32_t(n.aux)); movi(2, uint32_t(n.index));
                helper_call(op == OP_IC + IC_GET ? h.icget : h.icchk); bail_check();
                if (op == OP_IC + IC_GET) { st(0, S(rd)); st(2, S(rd) + 4); }
                else if (op == OP_IC + IC_CONST) { movi(0, uint32_t(n.value)); st(0, S(rd)); movi(2, 0); st(2, S(rd) + 4); }
            }
        }
        else if (cond1(op)) { ld(0, S(d - 1) + (op >= 198 ? 4 : 0)); o.b(0x85); o.b(0xc0);
            uint8_t cc = op == 153 || op == 198 ? 0x84 : op == 154 || op == 199 ? 0x85 : op == 155 ? 0x8c : op == 156 ? 0x8d : op == 157 ? 0x8f : 0x8e;
            o.b(0x41); o.b(0xbe); o.u32(uint32_t(n.pc));   // r14d = last pc (mov does not touch flags)
            jcc_lab(cc, n.target); jmp_lab(n.next); continue; }
        else if (cond2(op)) { const unsigned w = op >= 165 ? 4 : 0; ld(0, S(d - 2) + w); ld(1, S(d - 1) + w); o.b(0x39); o.b(0xc8);
            const int k = op >= 165 ? op - 165 : op - 159; const uint8_t cc[] = {0x84, 0x85, 0x8c, 0x8d, 0x8f, 0x8e};
            o.b(0x41); o.b(0xbe); o.u32(uint32_t(n.pc)); jcc_lab(cc[k], n.target); jmp_lab(n.next); continue; }
        else if (op == 167) { o.b(0x41); o.b(0xbe); o.u32(uint32_t(n.pc)); jmp_lab(n.target); continue; }
        else if (op == 170 || op == 171) { ld(0, S(d - 1)); o.b(0x41); o.b(0xbe); o.u32(uint32_t(n.pc));
            for (const auto& c : n.sw) { o.b(0x3d); o.u32(uint32_t(c.first)); jcc_lab(0x84, c.second); }   // cmp eax,key ; je case
            jmp_lab(n.target); continue; }
        o.b(0x41); o.b(0xbe); o.u32(uint32_t(n.pc));        // r14d = last executed pc
        jmp_lab(n.next);
    }
    // exit stubs: eax = pc ; common epilogue
    std::vector<int> stubs; for (const auto& f : fix) if (f.to_stub) stubs.push_back(f.pc);
    std::sort(stubs.begin(), stubs.end()); stubs.erase(std::unique(stubs.begin(), stubs.end()), stubs.end());
    std::vector<size_t> common_fix;
    for (int pc : stubs) { stub[size_t(pc)] = o.v.size(); movi(0, uint32_t(pc)); o.b(0xe9); common_fix.push_back(o.v.size()); o.u32(0); }
    const size_t common = o.v.size();
    // [rbx+0] = r14d ; rdx = r12 ; rax |= rdx<<32 ; epilogue
    o.b(0x44); o.b(0x89); o.b(0x33);
    o.b(0x44); o.b(0x89); o.b(0xe2); o.b(0x48); o.b(0xc1); o.b(0xe2); o.b(0x20); o.b(0x48); o.b(0x09); o.b(0xd0);
    for (uint8_t b : {0x48, 0x83, 0xc4, 0x08, 0x41, 0x5f, 0x41, 0x5e, 0x41, 0x5d, 0x41, 0x5c, 0x5d, 0x5b, 0xc3}) o.b(b);
    if (id) {   // stale: eax = own id ; rdx = StaleMark<<32 ; rax |= rdx ; same epilogue, slots untouched
        o.patch32(guard_at, uint32_t(o.v.size() - (guard_at + 4)));
        movi(0, id); o.b(0x48); o.b(0xba); for (int i = 0; i < 8; ++i) o.b(uint8_t((uint64_t(StaleMark) << 32) >> (8 * i))); o.b(0x48); o.b(0x09); o.b(0xd0);
        for (uint8_t b : {0x48, 0x83, 0xc4, 0x08, 0x41, 0x5f, 0x41, 0x5e, 0x41, 0x5d, 0x41, 0x5c, 0x5d, 0x5b, 0xc3}) o.b(b); }
    for (size_t at : common_fix) o.patch32(at, uint32_t(int32_t(int64_t(common) - int64_t(at + 4))));
    fix[entry_fix].pc = r.ins.empty() ? 0 : r.ins.front().pc;   // build() pushes the entry first
    for (const auto& f : fix) { const size_t dst = f.to_stub ? stub[size_t(f.pc)] : label[size_t(f.pc)];
        if (dst == SIZE_MAX) return {}; o.patch32(f.at, uint32_t(int32_t(int64_t(dst) - int64_t(f.at + 4)))); }
    return std::move(o.v);
}

// ---------------- AArch64 AAPCS64 ----------------
// x19 = slots, w20 = ops executed, w21 = budget, w22 = last pc. Scratch w9..w11, x16 for helper address.
inline std::vector<uint8_t> emit_a64(const Region& r, const Helpers& h, uint32_t id = 0) {
    Bytes o; std::vector<size_t> label(r.depth.size(), SIZE_MAX), stub(r.depth.size(), SIZE_MAX);
    struct Fix { size_t at; int pc; bool to_stub; uint32_t base; bool cond; }; std::vector<Fix> fix;
    auto ins = [&](uint32_t v) { o.u32(v); };
    auto ldr = [&](unsigned t, unsigned off) { ins(0xb9400000u | ((off / 4) << 10) | (19u << 5) | t); };
    auto str = [&](unsigned t, unsigned off) { ins(0xb9000000u | ((off / 4) << 10) | (19u << 5) | t); };
    auto movw = [&](unsigned d, uint32_t v) { ins(0x52800000u | ((v & 0xffffu) << 5) | d); if (v >> 16) ins(0x72a00000u | ((v >> 16) << 5) | d); };
    auto bcond_stub = [&](unsigned cc, int pc) { fix.push_back({o.v.size(), pc, true, 0x54000000u | cc, true}); ins(0); };
    auto b_stub = [&](int pc) { fix.push_back({o.v.size(), pc, true, 0x14000000u, false}); ins(0); };
    auto bcond_lab = [&](unsigned cc, int pc) { fix.push_back({o.v.size(), pc, false, 0x54000000u | cc, true}); ins(0); };
    auto b_lab = [&](int pc) { fix.push_back({o.v.size(), pc, false, 0x14000000u, false}); ins(0); };
    auto stp_pre = [&](unsigned t, unsigned t2, int imm) { ins(0xa9800000u | ((uint32_t(imm / 8) & 0x7fu) << 15) | (t2 << 10) | (31u << 5) | t); };
    auto stp_off = [&](unsigned t, unsigned t2, int imm) { ins(0xa9000000u | ((uint32_t(imm / 8) & 0x7fu) << 15) | (t2 << 10) | (31u << 5) | t); };
    auto ldp_off = [&](unsigned t, unsigned t2, int imm) { ins(0xa9400000u | ((uint32_t(imm / 8) & 0x7fu) << 15) | (t2 << 10) | (31u << 5) | t); };
    auto ldp_post = [&](unsigned t, unsigned t2, int imm) { ins(0xa8c00000u | ((uint32_t(imm / 8) & 0x7fu) << 15) | (t2 << 10) | (31u << 5) | t); };
    stp_pre(29, 30, -48); stp_off(19, 20, 16); stp_off(21, 22, 32);
    ins(0xaa0003f3u);          // mov x19,x0
    ins(0x2a0103f5u);          // mov w21,w1
    ins(0x52800014u);          // mov w20,#0
    ins(0x12800016u);          // mov w22,#-1 (movn w22,#0)
    size_t guard_at = SIZE_MAX;
    if (id) { ins(0xb9400669u); movw(10, id); ins(0x6b0a013fu); guard_at = o.v.size(); ins(0x54000001u); }   // ldr w9,[x19,#4]; cmp w9,w10; b.ne stale
    const size_t entry_fix = fix.size(); b_lab(-1);
    auto setlast = [&](int pc) { movw(22, uint32_t(pc)); };
    auto helper_call = [&](Helper fn) { uint64_t a = uint64_t(reinterpret_cast<uintptr_t>(fn));
        ins(0xd2800010u | (uint32_t(a & 0xffff) << 5)); for (unsigned hw = 1; hw < 4; ++hw) ins(0xf2800010u | (hw << 21) | (uint32_t((a >> (16 * hw)) & 0xffff) << 5));
        ins(0xd63f0200u); };    // blr x16
    for (const auto& n : r.ins) {
        label[size_t(n.pc)] = o.v.size();
        if (n.kind == K_EXIT) { b_stub(n.pc); continue; }
        if (n.cost == 1) { ins(0x6b15029fu); bcond_stub(2, n.pc);   // cmp w20,w21 ; b.hs stub
            ins(0x11000694u); }                                     // add w20,w20,#1
        else { ins(0x11000000u | (n.cost << 10) | (20u << 5) | 9u);  // add w9,w20,#cost
            ins(0x6b15013fu); bcond_stub(8, n.pc);                  // cmp w9,w21 ; b.hi stub
            ins(0x2a0903f4u); }                                     // mov w20,w9
        const unsigned d = unsigned(n.depth); const int op = n.op;
        auto S = [&](unsigned k) { return off_stack(r, k); }; auto L = [&](unsigned k) { return off_local(r, k); };
        auto bail_check = [&]() {
            ins(0xd360fc01u);          // lsr x1,x0,#32
            ins(0x3100043fu);          // cmn w1,#1
            ins(0x54000061u);          // b.ne +3 (skip the next two instructions)
            ins(0x51000000u | (n.cost << 10) | (20u << 5) | 20u);   // sub w20,w20,#cost
            b_stub(n.pc);
        };
        if (load_op(op)) { ldr(9, L(unsigned(n.index))); ldr(10, L(unsigned(n.index)) + 4); str(9, S(d)); str(10, S(d) + 4); }
        else if (store_op(op)) { ldr(9, S(d - 1)); ldr(10, S(d - 1) + 4); str(9, L(unsigned(n.index))); str(10, L(unsigned(n.index)) + 4); }
        else if ((op >= 2 && op <= 8) || op == 16 || op == 17 || op == 18 || op == 19) { movw(9, uint32_t(n.value)); str(9, S(d)); str(31, S(d) + 4); }
        else if (op == 1) { str(31, S(d)); str(31, S(d) + 4); }
        else if (op == 132) { ldr(9, L(unsigned(n.index))); movw(10, uint32_t(n.value)); ins(0x0b0a0129u); str(9, L(unsigned(n.index))); }
        else if (op == 87) { }
        else if (op == 89) { ldr(9, S(d - 1)); ldr(10, S(d - 1) + 4); str(9, S(d)); str(10, S(d) + 4); }
        else if (op == 95) { ldr(9, S(d - 1)); ldr(10, S(d - 1) + 4); ldr(11, S(d - 2)); ldr(12, S(d - 2) + 4); str(9, S(d - 2)); str(10, S(d - 2) + 4); str(11, S(d - 1)); str(12, S(d - 1) + 4); }
        else if (binop(op)) { ldr(9, S(d - 2)); ldr(10, S(d - 1)); uint32_t v = 0;
            switch (op) { case 96: v = 0x0b0a0129u; break; case 100: v = 0x4b0a0129u; break; case 104: v = 0x1b0a7d29u; break;
            case 120: v = 0x1aca2129u; break; case 122: v = 0x1aca2929u; break; case 124: v = 0x1aca2529u; break;
            case 126: v = 0x0a0a0129u; break; case 128: v = 0x2a0a0129u; break; default: v = 0x4a0a0129u; }
            ins(v); str(9, S(d - 2)); str(31, S(d - 2) + 4); }
        else if (op == 116 || (op >= 145 && op <= 147)) { ldr(9, S(d - 1));
            ins(op == 116 ? 0x4b0903e9u : op == 145 ? 0x13001d29u : op == 146 ? 0x53003d29u : 0x13003d29u); str(9, S(d - 1)); str(31, S(d - 1) + 4); }
        else if (op == 46 || op == 48 || op == 50 || op == 51 || op == 52 || op == 53) {
            ldr(0, S(d - 2) + 4); ldr(1, S(d - 1)); movw(2, uint32_t(op)); helper_call(h.aload); bail_check(); str(0, S(d - 2)); str(1, S(d - 2) + 4); }
        else if (op == 79) {
            if(!h.iastore)return {};
            ldr(0, S(d - 3) + 4); ldr(1, S(d - 2)); ldr(2, S(d - 1)); helper_call(h.iastore); bail_check(); }
        else if (op == 190) { ldr(0, S(d - 1) + 4); movw(1, 0); movw(2, 0); helper_call(h.alen); bail_check(); str(0, S(d - 1)); str(1, S(d - 1) + 4); }
        else if (op == 180) { ldr(0, S(d - 1) + 4); movw(1, uint32_t(n.index)); movw(2, 0); helper_call(h.getfield); bail_check(); str(0, S(d - 1)); str(1, S(d - 1) + 4); }
        else if (op == 192) { ldr(0, S(d - 1) + 4); movw(1, uint32_t(n.index)); movw(2, 0); helper_call(h.cast); bail_check(); }
        else if (op == 193) { ldr(0, S(d - 1) + 4); movw(1, uint32_t(n.index)); movw(2, 0); helper_call(h.inst); bail_check(); str(0, S(d - 1)); str(31, S(d - 1) + 4); }
        else if (op > OP_IC) {
            const unsigned rd = d - 1 - n.argc;
            if (op == OP_IC + IC_GFX) {
                ldr(0, S(rd) + 4); movw(1, uint32_t(n.aux)); movw(2, uint32_t(n.value) | (uint32_t(n.argc) << 8) | (uint32_t(d) << 16));
                helper_call(h.icgfx); bail_check();
            } else {
                ldr(0, S(rd) + 4); movw(1, uint32_t(n.aux)); movw(2, uint32_t(n.index));
                helper_call(op == OP_IC + IC_GET ? h.icget : h.icchk); bail_check();
                if (op == OP_IC + IC_GET) { str(0, S(rd)); str(1, S(rd) + 4); }
                else if (op == OP_IC + IC_CONST) { movw(9, uint32_t(n.value)); str(9, S(rd)); str(31, S(rd) + 4); }
            }
        }
        else if (cond1(op)) { ldr(9, S(d - 1) + (op >= 198 ? 4 : 0)); setlast(n.pc); ins(0x7100013fu);   // cmp w9,#0
            unsigned cc = op == 153 || op == 198 ? 0 : op == 154 || op == 199 ? 1 : op == 155 ? 11 : op == 156 ? 10 : op == 157 ? 12 : 13;
            bcond_lab(cc, n.target); b_lab(n.next); continue; }
        else if (cond2(op)) { const unsigned w = op >= 165 ? 4 : 0; ldr(9, S(d - 2) + w); ldr(10, S(d - 1) + w); setlast(n.pc); ins(0x6b0a013fu);
            const int k = op >= 165 ? op - 165 : op - 159; const unsigned cc[] = {0, 1, 11, 10, 12, 13}; bcond_lab(cc[k], n.target); b_lab(n.next); continue; }
        else if (op == 167) { setlast(n.pc); b_lab(n.target); continue; }
        else if (op == 170 || op == 171) { ldr(9, S(d - 1)); setlast(n.pc);
            for (const auto& c : n.sw) { movw(10, uint32_t(c.first)); ins(0x6b0a013fu); bcond_lab(0, c.second); }   // cmp w9,w10 ; b.eq case
            b_lab(n.target); continue; }
        setlast(n.pc); b_lab(n.next);
    }
    std::vector<int> stubs; for (const auto& f : fix) if (f.to_stub) stubs.push_back(f.pc);
    std::sort(stubs.begin(), stubs.end()); stubs.erase(std::unique(stubs.begin(), stubs.end()), stubs.end());
    std::vector<size_t> common_fix;
    for (int pc : stubs) { stub[size_t(pc)] = o.v.size(); movw(0, uint32_t(pc)); common_fix.push_back(o.v.size()); ins(0x14000000u); }
    const size_t common = o.v.size();
    str(22, 0);                // [x19] = last pc
    ins(0xaa148000u);          // orr x0,x0,x20,lsl #32  (w0 writes zeroed the upper half)
    ldp_off(21, 22, 32); ldp_off(19, 20, 16); ldp_post(29, 30, 48); ins(0xd65f03c0u);
    if (id) {   // stale: w0 = own id ; x1 = StaleMark<<32 ; orr ; same epilogue, slots untouched
        const int64_t rel = (int64_t(o.v.size()) - int64_t(guard_at)) / 4; o.patch32(guard_at, 0x54000001u | ((uint32_t(rel) & 0x7ffffu) << 5));
        movw(0, id); ins(0xd2dfffe1u); ins(0xf2ffffe1u); ins(0xaa010000u);
        ldp_off(21, 22, 32); ldp_off(19, 20, 16); ldp_post(29, 30, 48); ins(0xd65f03c0u); }
    for (size_t at : common_fix) o.patch32(at, 0x14000000u | (uint32_t((int64_t(common) - int64_t(at)) / 4) & 0x3ffffffu));
    fix[entry_fix].pc = r.ins.empty() ? 0 : r.ins.front().pc;
    for (const auto& f : fix) { const size_t dst = f.to_stub ? stub[size_t(f.pc)] : label[size_t(f.pc)]; if (dst == SIZE_MAX) return {};
        const int64_t rel = (int64_t(dst) - int64_t(f.at)) / 4;
        if (f.cond) { if (rel < -(1 << 18) || rel >= (1 << 18)) return {}; o.patch32(f.at, f.base | ((uint32_t(rel) & 0x7ffffu) << 5)); }
        else o.patch32(f.at, f.base | (uint32_t(rel) & 0x3ffffffu)); }
    return std::move(o.v);
}
inline std::vector<uint8_t> emit_host(const Region& r, const Helpers& h, uint32_t id = 0) {
#if defined(__aarch64__)
    return emit_a64(r, h, id);
#elif defined(__x86_64__)
    return emit_x64(r, h, id);
#else
    (void)r; (void)h; return {};
#endif
}
} // namespace jitr
