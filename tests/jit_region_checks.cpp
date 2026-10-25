// P6.2 T2 + P6.3 T3 differential test for the region JIT (src/jit_region.h). A reference interpreter runs the same op subset
// with a toy heap (int arrays, byte/short/bool arrays, one int field per object). Random programs include loops,
// array loads, arraylength, getfield, and the null/out-of-bounds cases, so compiled code must bail to the exact pc.
#include "../src/jit_region.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <climits>
#include <cstdint>
#include <vector>
using namespace jitr;
// ---- toy heap shared by the reference and the helpers ----
struct Arr { int kind; std::vector<int32_t> v; };     // kind: 'I'=int('[I'),'B','S','Z' sign handled on read
struct Obj { int32_t field = 0; int32_t cls = 0; };
static int32_t g_cast_ok = 1, g_cast_no = 2;            // toy per-site cast cache (site 0)
static std::vector<Arr> g_arrays{{0, {}}};             // ref 0 reserved as null
static std::vector<Obj> g_objs{{0}};
static uint64_t g_store_calls=0;
extern "C" uint64_t h_iastore(int32_t ref,int32_t idx,int32_t value) {
    if(ref<=0 || size_t(ref)>=g_arrays.size() || idx<0)return Bail;
    auto& a=g_arrays[size_t(ref)];if(a.kind!='I' || size_t(idx)>=a.v.size())return Bail;
    a.v[size_t(idx)]=value;++g_store_calls;return 0;
}
static int32_t read_elem(const Arr& a, int idx) {
    int32_t raw = a.v[size_t(idx)];
    if (a.kind == 'B') return int8_t(raw); if (a.kind == 'S') return int16_t(raw); if (a.kind == 'Z') return raw & 1; if (a.kind == 'C') return uint16_t(raw); return raw;
}
// helpers: pack (ref<<32)|i, or Bail
extern "C" uint64_t h_aload(int32_t ref, int32_t idx, int32_t op) {
    if (ref <= 0 || size_t(ref) >= g_arrays.size()) return Bail;
    const Arr& a = g_arrays[size_t(ref)]; if (idx < 0 || size_t(idx) >= a.v.size()) return Bail;
    return pack(read_elem(a, idx), 0);
}
extern "C" uint64_t h_alen(int32_t ref, int32_t, int32_t) {
    if (ref <= 0 || size_t(ref) >= g_arrays.size()) return Bail;
    return pack(int32_t(g_arrays[size_t(ref)].v.size()), 0);
}
extern "C" uint64_t h_getfield(int32_t ref, int32_t slot, int32_t) {
    if (ref <= 0 || size_t(ref) >= g_objs.size()) return Bail;
    return pack(g_objs[size_t(ref)].field, 0);   // one int field, slot ignored in the toy model
}
extern "C" uint64_t h_icchk(int32_t ref, int32_t cls, int32_t) {
    if (ref <= 0 || size_t(ref) >= g_objs.size()) return Bail; if (cls >= 0 && g_objs[size_t(ref)].cls != cls) return Bail; return 0;
}
extern "C" uint64_t h_icget(int32_t ref, int32_t cls, int32_t slot) { if (h_icchk(ref, cls, 0) == Bail) return Bail; return h_getfield(ref, slot, 0); }
extern "C" uint64_t h_cast(int32_t ref, int32_t, int32_t) {
    if (ref == 0) return 0; if (ref < 0 || size_t(ref) >= g_objs.size()) return Bail; return g_objs[size_t(ref)].cls == g_cast_ok ? 0 : Bail;
}
extern "C" uint64_t h_inst(int32_t ref, int32_t, int32_t) {
    if (ref == 0) return pack(0, 0); if (ref < 0 || size_t(ref) >= g_objs.size()) return Bail;
    const int c = g_objs[size_t(ref)].cls; if (c == g_cast_ok) return pack(1, 0); if (c == g_cast_no) return pack(0, 0); return Bail;
}
// ---- reference interpreter over Region ----
struct RVal { int32_t i, ref; };
static uint64_t n_switch_hits = 0, n_switch_programs = 0, n_sw_default = 0, n_sw_case = 0, n_sw_negkey = 0, n_sw_lookup = 0, n_sw_huge = 0;
static int g_last = -1;
static std::vector<RVal> g_ref_stack;
static const uint8_t* g_code = nullptr;   // P6.7: raw bytecode, so the reference decodes switches independently of build()
static int32_t rd32(size_t at) { return int32_t((uint32_t(g_code[at]) << 24) | (uint32_t(g_code[at + 1]) << 16) | (uint32_t(g_code[at + 2]) << 8) | uint32_t(g_code[at + 3])); }
static bool ref_run(const Region& r, std::vector<RVal>& locals, uint32_t budget, int entry, int& exit_pc, uint32_t& ops, std::vector<RVal> st = {}) {
    struct Keep { std::vector<RVal>& s; std::vector<RVal>& out; ~Keep() { out = s; } } keep{st, g_ref_stack};
    int pc = entry; ops = 0; int& last = g_last; last = -1;
    auto at = [&](int p) -> const Ins* { for (auto& n : r.ins) if (n.pc == p) return &n; return nullptr; };
    for (;;) {
        const Ins* np = at(pc); if (!np) { exit_pc = pc; return true; }
        const Ins& n = *np; if (n.kind == K_EXIT) { exit_pc = pc; return true; }
        if (ops + n.cost > budget) { exit_pc = pc; return true; }
        ops += n.cost; const int op = n.op;
        auto push = [&](int32_t i, int32_t ref = 0) { st.push_back({i, ref}); };
        if (load_op(op)) push(locals[size_t(n.index)].i, locals[size_t(n.index)].ref);
        else if (store_op(op)) { locals[size_t(n.index)] = st.back(); st.pop_back(); }
        else if ((op >= 2 && op <= 8) || op == 16 || op == 17 || op == 18 || op == 19) push(n.value);
        else if (op == 1) push(0, 0);
        else if (op == 132) locals[size_t(n.index)].i = int32_t(uint32_t(locals[size_t(n.index)].i) + uint32_t(n.value));
        else if (op == 87) st.pop_back();
        else if (op == 89) st.push_back(st.back());
        else if (op == 95) std::swap(st[st.size() - 1], st[st.size() - 2]);
        else if (binop(op)) { int32_t b = st.back().i; st.pop_back(); int32_t a = st.back().i; uint32_t ua = a, ub = b, rr = 0;
            switch (op) { case 96: rr = ua + ub; break; case 100: rr = ua - ub; break; case 104: rr = ua * ub; break;
            case 120: rr = ua << (ub & 31); break; case 122: rr = uint32_t(a >> (ub & 31)); break; case 124: rr = ua >> (ub & 31); break;
            case 126: rr = ua & ub; break; case 128: rr = ua | ub; break; default: rr = ua ^ ub; }
            st.back() = {int32_t(rr), 0}; }
        else if (op == 116) st.back().i = int32_t(0u - uint32_t(st.back().i));
        else if (op == 145) st.back().i = int8_t(st.back().i);
        else if (op == 146) st.back().i = uint16_t(st.back().i);
        else if (op == 147) st.back().i = int16_t(st.back().i);
        else if (op == 46 || op == 48 || op == 50 || op == 51 || op == 52 || op == 53) {
            int idx = st.back().i; int ref = st[st.size() - 2].ref; uint64_t h = h_aload(ref, idx, op);
            if (h == Bail) { --ops; exit_pc = pc; return true; } st.pop_back(); st.back() = {int32_t(uint32_t(h)), 0}; }
        else if(op==79) {
            const size_t d=st.size();const int ref=st[d-3].ref,idx=st[d-2].i;
            if(ref<=0 || size_t(ref)>=g_arrays.size() || idx<0 || g_arrays[size_t(ref)].kind!='I' || size_t(idx)>=g_arrays[size_t(ref)].v.size()) {
                --ops;exit_pc=pc;return true;
            }
            g_arrays[size_t(ref)].v[size_t(idx)]=st[d-1].i;st.resize(d-3);
        }
        else if (op == 190) { int ref = st.back().ref; uint64_t h = h_alen(ref, 0, 0); if (h == Bail) { --ops; exit_pc = pc; return true; } st.back() = {int32_t(uint32_t(h)), 0}; }
        else if (op == 180) { int ref = st.back().ref; uint64_t h = h_getfield(ref, n.index, 0); if (h == Bail) { --ops; exit_pc = pc; return true; } st.back() = {int32_t(uint32_t(h)), 0}; }
        else if (op == 192) { if (h_cast(st.back().ref, n.index, 0) == Bail) { ops -= n.cost; exit_pc = pc; return true; } }
        else if (op == 193) { uint64_t h = h_inst(st.back().ref, n.index, 0); if (h == Bail) { ops -= n.cost; exit_pc = pc; return true; } st.back() = {int32_t(uint32_t(h)), 0}; }
        else if (op > OP_IC) { const size_t rd = st.size() - 1 - n.argc; const int recv = st[rd].ref;
            uint64_t h = op == OP_IC + IC_GET ? h_icget(recv, n.aux, n.index) : h_icchk(recv, n.aux, 0);
            if (h == Bail) { ops -= n.cost; exit_pc = pc; return true; }
            if (op == OP_IC + IC_GET) { st.resize(rd + 1); st[rd] = {int32_t(uint32_t(h)), int32_t(h >> 32)}; }
            else if (op == OP_IC + IC_CONST) { st.resize(rd + 1); st[rd] = {n.value, 0}; }
            else if (op == OP_IC + IC_SELF) { st.resize(rd + 1); }
            else st.resize(rd); }
        else if (cond1(op)) { int32_t v = op >= 198 ? st.back().ref : st.back().i; st.pop_back();
            bool t = op == 153 || op == 198 ? v == 0 : op == 154 || op == 199 ? v != 0 : op == 155 ? v < 0 : op == 156 ? v >= 0 : op == 157 ? v > 0 : v <= 0;
            last = pc; pc = t ? n.target : n.next; continue; }
        else if (cond2(op)) { int32_t b = op >= 165 ? st.back().ref : st.back().i; st.pop_back(); int32_t a = op >= 165 ? st.back().ref : st.back().i; st.pop_back();
            bool t = op == 159 || op == 165 ? a == b : op == 160 || op == 166 ? a != b : op == 161 ? a < b : op == 162 ? a >= b : op == 163 ? a > b : a <= b;
            last = pc; pc = t ? n.target : n.next; continue; }
        else if (op == 167) { last = pc; pc = n.target; continue; }
        else if (op == 170 || op == 171) { const int32_t key = st.back().i; st.pop_back(); const size_t b = (size_t(pc) + 4) & ~size_t(3);
            int dst = pc + rd32(b);
            if (op == 170) { const int32_t lo = rd32(b + 4), hi = rd32(b + 8); if (key >= lo && key <= hi) dst = pc + rd32(b + 12 + 4 * size_t(int64_t(key) - lo)); }
            else { const int32_t np = rd32(b + 4); for (int32_t k = 0; k < np; ++k) if (rd32(b + 8 + 8 * size_t(k)) == key) { dst = pc + rd32(b + 12 + 8 * size_t(k)); break; } }
            if (dst == pc + rd32(b)) ++n_sw_default; else ++n_sw_case; if (key < 0) ++n_sw_negkey; if (op == 171) ++n_sw_lookup;
            ++n_switch_hits; last = pc; pc = dst; continue; }
        else { exit_pc = pc; return true; }
        last = pc; pc = n.next;
    }
}
int main(int argc, char** argv) {
    std::mt19937 rng(argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 0x6202);
    // seed arrays and objects
    for (int i = 0; i < 10; ++i) { Arr a; a.kind = "IBSZC"[i % 5]; int len = rng() % 14; for (int j = 0; j < len; ++j) a.v.push_back(int32_t(rng())); g_arrays.push_back(a); }
    for (int i = 0; i < 6; ++i) g_objs.push_back({int32_t(rng()), int32_t(i % 4)});   // classes 0..3
    uint64_t programs = 0, cases = 0, n_bail = 0, n_budget = 0, n_exitop = 0, n_zero = 0;
    Helpers H{(Helper)h_getfield, (Helper)h_aload, (Helper)h_alen, (Helper)h_icget, (Helper)h_icchk, (Helper)h_cast, (Helper)h_inst, nullptr,h_iastore};
    uint64_t n_ic = 0, n_cast = 0, n_stack_entry = 0;
    std::vector<Constant> cpool(4); cpool[1] = {true, int32_t(0x7ffffff3)}; const std::vector<Constant>* consts = &cpool;
    for (int round = 0; round < 2200; ++round) {
        const unsigned L = 3 + rng() % 5;
        std::vector<uint8_t> code; std::vector<int> fslot; std::vector<IcSite> ics; std::vector<int> casts;
        const bool stack_entry = rng() % 4 == 0; if (stack_entry) { code.push_back(0x36); code.push_back(3); }   // entry depth 1: istore 3
        auto emit = [&](std::initializer_list<int> bs) { for (int b : bs) code.push_back(uint8_t(b)); };
        // Build a counted loop: local0 = arg; iconst_0; istore_2; LOOP: ... ; iinc 2,1; iload_2; bipush N; if_icmplt LOOP; iload_1; ireturn-ish exit
        emit({3, 0x36, 2});                       // iconst_0 ; istore_2
        size_t loop = code.size();
        // loop body: a few random ops that read arg (local1 holds an array ref? we pass refs in locals)
        int body = 1 + rng() % 4;
        for (int b = 0; b < body; ++b) {
            int pick = rng() % 27;
            if(pick==26){emit({25,1,21,2,21,3,79,25,1,21,2,46,54,3});continue;}
            if (pick >= 23) {   // P6.7 switch cases: key = local2 + bias (loop counter, so default/boundaries/negatives are all hit)
                emit({0x15, 2}); emit({0x10, int(uint8_t(int8_t(int(rng() % 9) - 6)))}); emit({0x60});
                const size_t at = code.size(); const bool table = pick != 25; emit({table ? 0xaa : 0xab}); while (code.size() % 4) code.push_back(0);
                std::vector<int32_t> keys; const bool huge = rng() % 12 == 0;
                if (table) { const int32_t lo = huge ? -3 : int32_t(rng() % 7) - 4; const int cnt = huge ? 70 : 1 + int(rng() % 7); for (int k = 0; k < cnt; ++k) keys.push_back(lo + k); }
                else { const int32_t pool[] = {INT32_MIN, -1000, -3, -1, 0, 2, 3, 5, 9, 77, 0x7fffffff}; for (int32_t v : pool) if (rng() % 2) keys.push_back(v); if (huge) for (int k = 0; k < 70; ++k) keys.push_back(100000 + 3 * k); }
                const int K = int(keys.size()); const size_t hdr = code.size() + (table ? 12 : 8) + size_t(table ? 4 : 8) * size_t(K);
                // landing chain: K+1 iinc 3,c blocks (3 bytes each); case j -> block j, default -> block K; all fall through to the join
                auto be = [&](int32_t v) { code.push_back(uint8_t(uint32_t(v) >> 24)); code.push_back(uint8_t(uint32_t(v) >> 16)); code.push_back(uint8_t(uint32_t(v) >> 8)); code.push_back(uint8_t(v)); };
                std::vector<int> land(size_t(K) + 1); for (int j = 0; j <= K; ++j) land[size_t(j)] = int(hdr) + 3 * int(rng() % size_t(K + 1));
                be(land[size_t(K)] - int(at));
                if (table) { be(keys.front()); be(keys.back()); for (int j = 0; j < K; ++j) be(land[size_t(j)] - int(at)); }
                else { be(K); for (int j = 0; j < K; ++j) { be(keys[size_t(j)]); be(land[size_t(j)] - int(at)); } }
                for (int j = 0; j <= K; ++j) emit({0x84, 3, 1 + j});
                ++n_switch_programs; if (huge) ++n_sw_huge; continue; }
            auto ic_at = [&](IcSite ic) { ics.resize(code.size() + 1); ics[code.size()] = ic; emit({0xb6, 0, 9}); };
            if (pick >= 17) {
                if (pick == 17) { emit({0x2a}); IcSite ic; ic.kind = IC_GET; ic.cls = int32_t(rng() % 4); ic.cost = 4; ic_at(ic); emit({0x3b}); }               // aload_0; invokevirtual getter; istore_0
                else if (pick == 18) { emit({0x2a}); emit({0x15, 3}); IcSite ic; ic.kind = IC_CONST; ic.argc = 1; ic.cls = int32_t(rng() % 4); ic.value = int32_t(rng() % 100) - 50; ic.cost = 3; ic_at(ic); emit({0x36, 3}); }
                else if (pick == 19) { emit({0x2a}); IcSite ic; ic.kind = IC_VOID; ic.cls = int32_t(rng() % 4) - (rng() % 3 == 0 ? 6 : 0); ic.cost = 2; ic_at(ic); }   // incl. cls<0 (invokespecial)
                else if (pick == 20) { emit({0x2a}); IcSite ic; ic.kind = IC_SELF; ic.cls = int32_t(rng() % 4); ic.cost = 3; ic_at(ic); emit({0x57}); }
                else if (pick == 21) { emit({0x2a}); casts.resize(code.size() + 1, -1); casts[code.size()] = 0; emit({0xc0, 0, 7}); emit({0x57}); }               // checkcast; pop
                else { emit({0x2a}); casts.resize(code.size() + 1, -1); casts[code.size()] = 0; emit({0xc1, 0, 7}); emit({0x36, 3}); }                           // instanceof; istore_3
                continue; }
            if (pick == 0) { emit({0x15, 2}); emit({0x10, int(rng() % 20)}); emit({0x60}); emit({0x36, 3}); }         // iload_2; bipush; iadd; istore_3
            else if (pick == 1) { emit({0x19, 1}); emit({0xbe}); emit({0x3b}); }                                       // aload_1; arraylength; istore_0
            else if (pick == 2) { emit({0x19, 1}); emit({0x15, 2}); emit({0x2e}); emit({0x3b}); }                      // aload_1; iload_2; iaload; istore_0
            else if (pick == 3) { emit({0x19, 1}); emit({0x15, 2}); emit({0x33}); emit({0x3b}); }                      // aload_1; iload_2; baload; istore_0
            else if (pick == 4) { emit({0x2a}); fslot.resize(code.size(), -1); fslot.back() = 0; emit({0xb4, 0, 19}); emit({0x3b}); } // aload_0; getfield; istore_0  (aload_0 is arg0=obj ref)
            else if (pick == 5) { emit({0x15, 2}); emit({0x15, 3}); emit({0x7e}); emit({0x3b}); }                      // iload_2; iload_3; iand; istore_0
            else if (pick == 6) { emit({0x15, 3}); emit({0x10, int(rng() % 40)}); emit({int(std::vector<int>{0x78,0x7a,0x7c,0x64,0x68,0x80,0x82}[rng() % 7])}); emit({0x36, 3}); } // shifts/sub/mul/or/xor
            else if (pick == 7) { emit({0x15, 3}); emit({int(std::vector<int>{0x74,0x91,0x92,0x93}[rng() % 4])}); emit({0x36, 3}); }   // ineg/i2b/i2c/i2s
            else if (pick == 8) { emit({0x15, 3}); emit({0x59}); emit({0x60}); emit({0x15, 2}); emit({0x5f}); emit({0x64}); emit({0x36, 3}); } // dup add, swap sub
            else if (pick == 9) { emit({0x11, int(rng() & 0xff), int(rng() & 0xff)}); emit({0x57}); }                // sipush; pop
            else if (pick == 10) { emit({0x12, 1}); emit({0x36, 3}); }                                                  // ldc #1 (int constant)
            else if (pick == 11) { emit({0x19, 1}); emit({0x10, int(int8_t(rng() % 20 - 4))}); emit({int(std::vector<int>{0x2e,0x33,0x34,0x35}[rng() % 4])}); emit({0x36, 3}); } // array load incl. negative/oob index
            else if (pick == 12) { emit({0x15, 3}); emit({0x99 + int(rng() % 6), 0, 6}); emit({0x84, 3, 3}); }       // if<cond> +6 skipping iinc 3,3
            else if (pick == 13) { emit({0x19, 1}); emit({0xc6, 0, 6}); emit({0x84, 3, 1}); }                         // aload_1; ifnull +6
            else if (pick == 14) { emit({0x19, 1}); emit({0x19, 0}); emit({0xa5 + int(rng() % 2), 0, 6}); emit({0x84, 3, 7}); } // if_acmpeq/ne
            else if (pick == 15) { emit({0x01}); emit({0xc7, 0, 6}); emit({0x84, 3, 5}); }                            // aconst_null; ifnonnull
            else { emit({0x15, 2}); emit({0x15, 3}); emit({0x9f + int(rng() % 6), 0, 6}); emit({0x84, 3, 2}); }       // if_icmp<cond> forward
        }
        emit({0x84, 2, 1});                       // iinc 2,1
        emit({0x15, 2});                          // iload_2
        int N = 2 + rng() % 8; emit({0x10, N});   // bipush N
        int off = int(loop) - int(code.size());   // if_icmplt back to loop
        emit({0xa1, (off >> 8) & 0xff, off & 0xff});
        emit({0x15, 3}); emit({0xac});            // iload_3; ireturn (exit op; not compiled -> region exits here)
        fslot.resize(code.size(), -1); ics.resize(code.size()); casts.resize(code.size(), -1);
        Spec s; s.code = code.data(); s.size = code.size(); s.entry = 0; s.max_locals = L; s.max_stack = 8; s.constants = consts; s.field_slot = &fslot; s.ic = &ics; s.cast_site = &casts; s.entry_depth = stack_entry ? 1 : 0;
        s.int_array_stores=true;
        Region r; if (!build(s, r)) continue; ++programs; g_code = code.data();
        auto bytes = emit_host(r, H); if (bytes.empty()) { std::printf("EMIT FAIL round=%d\n", round); return 1; }
        jit236::Executable exe; if (!exe.install(bytes)) { std::printf("INSTALL FAIL\n"); return 1; }
        Fn fn = (Fn)exe.fn;
        for (int t = 0; t < 60; ++t) {
            std::vector<RVal> rl(L); for (unsigned k = 0; k < L; ++k) rl[k] = {int32_t(rng()), 0};
            // locals 0 = object ref, 1 = array ref (sometimes null to force bail)
            rl[0].ref = int32_t(rng() % (g_objs.size() + 1)); rl[1].ref = (rng() % 5 == 0) ? 0 : int32_t(1 + rng() % (g_arrays.size() - 1));
            uint32_t budget = 1 + rng() % 400;
            const int32_t sv = int32_t(rng());
            const auto initial_arrays=g_arrays;
            std::vector<RVal> rlc = rl; int rpc = 0; uint32_t rops = 0; ref_run(r, rlc, budget, 0, rpc, rops, stack_entry ? std::vector<RVal>{{sv, 0}} : std::vector<RVal>{});
            const auto expected_arrays=g_arrays;g_arrays=initial_arrays;
            // JIT: slots layout
            std::vector<int32_t> slots(2 + 2 * (L + s.max_stack), 0);
            for (unsigned k = 0; k < L; ++k) { slots[2 + 2 * k] = rl[k].i; slots[3 + 2 * k] = rl[k].ref; }
            if (stack_entry) { slots[2 + 2 * L] = sv; slots[3 + 2 * L] = 0; }
            uint64_t rc = fn(slots.data(), budget);
            uint32_t jpc = uint32_t(rc); uint32_t jops = uint32_t(rc >> 32); int32_t jlast = slots[0];
            bool mism = (jpc != uint32_t(rpc)) || (jops != rops) || (jlast != g_last);
            for(size_t a=0;a<g_arrays.size();++a)if(g_arrays[a].v!=expected_arrays[a].v)mism=true;
            for (unsigned k = 0; k < L && !mism; ++k) if (slots[2 + 2 * k] != rlc[k].i || slots[3 + 2 * k] != rlc[k].ref) mism = true;
            if (!mism) { const int dep = r.depth[size_t(rpc)]; if (dep < 0 || size_t(dep) != g_ref_stack.size()) mism = true;
                for (int q = 0; q < dep && !mism; ++q) if (slots[2 + 2 * (L + unsigned(q))] != g_ref_stack[size_t(q)].i || slots[3 + 2 * (L + unsigned(q))] != g_ref_stack[size_t(q)].ref) mism = true; }
            if (mism) { std::printf("MISMATCH round=%d trial=%d budget=%u ref_pc=%d jit_pc=%u ref_ops=%u jit_ops=%u ref_last=%d jit_last=%d\n", round, t, budget, rpc, jpc, rops, jops, g_last, jlast);
                for (unsigned k = 0; k < L; ++k) std::printf("  local%u ref(i=%d,r=%d) jit(i=%d,r=%d)\n", k, rlc[k].i, rlc[k].ref, slots[2 + 2 * k], slots[3 + 2 * k]); return 2; }
            ++cases; if (rops == 0) ++n_zero; if (stack_entry) ++n_stack_entry;
            { const Ins* e = nullptr; for (auto& q : r.ins) if (q.pc == rpc) e = &q; if (e && e->op > OP_IC) ++n_ic; if (e && (e->op == 192 || e->op == 193)) ++n_cast; }
            { const Ins* e = nullptr; for (auto& q : r.ins) if (q.pc == rpc) e = &q; if (e && e->kind == K_EXIT) ++n_exitop; else if (rops >= budget) ++n_budget; else ++n_bail; }
        }
    }
    std::printf("PASS jit_region arch=%s programs=%llu differential_cases=%llu exit_op=%llu budget_exit=%llu bail_null_or_oob_or_icmiss=%llu zero_progress=%llu exits_at_ic=%llu exits_at_cast=%llu stack_entry_cases=%llu switch_sites=%llu switch_taken=%llu sw_default=%llu sw_case=%llu sw_negkey=%llu sw_lookup=%llu sw_oversize_exit_sites=%llu\n", jit236::architecture(), (unsigned long long)programs, (unsigned long long)cases, (unsigned long long)n_exitop, (unsigned long long)n_budget, (unsigned long long)n_bail, (unsigned long long)n_zero, (unsigned long long)n_ic, (unsigned long long)n_cast, (unsigned long long)n_stack_entry, (unsigned long long)n_switch_programs, (unsigned long long)n_switch_hits, (unsigned long long)n_sw_default, (unsigned long long)n_sw_case, (unsigned long long)n_sw_negkey, (unsigned long long)n_sw_lookup, (unsigned long long)n_sw_huge);
    assert(g_store_calls>0);std::printf("PASS iastore differential heap writes=%llu\n",(unsigned long long)g_store_calls);
    return 0;
}
