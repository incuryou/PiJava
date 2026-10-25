#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Recognize a time subtraction followed by a constant comparison whose waiting
// path contains only yield/nop/goto. Never skip Java instructions or side effects.
struct TimeGatePattern {
    int local = -1;
    int64_t millis = 0;
    int static_field = -1, anchor_clock_pc = -1, site_pc = -1;
};

// This is a value provenance record, not a gate proof. A short Java sleep can
// discard polling evidence while the same update still owns its clock anchor.
struct FieldGateAnchor {
    const void* method=nullptr;
    int clock_pc=-1, field=-1;
    int64_t value=0, real_us=0;
    uint64_t draws=0;
    void reset() {*this={};}
    void record(const void* m,int pc,int cp,int64_t v,int64_t now,uint64_t drawn) {
        method=m;clock_pc=pc;field=cp;value=v;real_us=now;draws=drawn;
    }
    bool owns(const void* m,const TimeGatePattern& gate,int64_t current,int64_t now,uint64_t drawn) const {
        return method==m && clock_pc==gate.anchor_clock_pc && field==gate.static_field &&
            current==value && drawn>draws && now>=real_us && now-real_us<=120000;
    }
};
inline int field_clock_store(const std::vector<uint8_t>& code,int next_pc) {
    int p=next_pc;
    if(p>=0 && p<int(code.size()) && code[p]==0x5c)++p; // dup2: one additional timestamp alias
    return p>=0 && p+2<int(code.size()) && code[p]==0xb3 ? (int(code[p+1])<<8)|code[p+2] : -1;
}
template<class LongConstant, class IsYield>
TimeGatePattern recognize_time_gate(const std::vector<uint8_t>& code, int clock_pc,
                                    int next_pc, LongConstant constant, IsYield is_yield) {
    const int size = int(code.size());
    int p = next_pc;
    TimeGatePattern result;
    if (p < 0 || p >= size) return {};
    const int load = code[p++];
    if (load == 0x16) { if (p >= size) return {}; result.local = code[p++]; }
    else if (load >= 0x1e && load <= 0x21) result.local = load - 0x1e;
    else return {};
    if (p + 7 > size || code[p++] != 0x65 || code[p++] != 0x14) return {};
    const int index = (int(code[p]) << 8) | code[p+1]; p += 2;
    result.millis = constant(index);
    if (result.millis < 2 || result.millis > 120 || code[p++] != 0x94) return {};
    const int branch_pc = p, branch = code[p++];
    if (p + 2 > size) return {};
    const int offset = int16_t((int(code[p]) << 8) | code[p+1]); p += 2;
    if (branch == 0x9b || branch == 0x9e) p = branch_pc + offset; // iflt/ifle: wait
    else if (branch != 0x9c && branch != 0x9d) return {}; // ifge/ifgt: ready
    if (branch == 0x9e || branch == 0x9d) ++result.millis;
    for (int n = 0; n < 8 && p >= 0 && p < size; ++n) {
        const int op_pc = p, op = code[p++];
        if (op == 0) continue;
        if (op == 0xb8) {
            if (p + 2 > size) return {};
            const int cp = (int(code[p]) << 8) | code[p+1]; p += 2;
            if (!is_yield(cp)) return {};
            continue;
        }
        if (op == 0xa7) {
            if (p + 2 > size) return {};
            const int target = op_pc + int16_t((int(code[p]) << 8) | code[p+1]);
            return target >= 0 && target <= clock_pc ? result : TimeGatePattern{};
        }
        return {};
    }
    return {};
}

// Two deliberately narrow extensions: a static-long anchor followed by the
// existing pure wait path, or a cached static-long difference guarded by
// `difference > 0 && difference < constant`. No arbitrary field expression,
// call, write or extra loop is accepted on the waiting path.
template<class LongConstant,class IsYield,class IsClock,class IsLongStatic,class OpLength>
TimeGatePattern recognize_field_time_gate(const std::vector<uint8_t>& code,int clock_pc,int next_pc,
        LongConstant constant,IsYield is_yield,IsClock is_clock,IsLongStatic is_long_static,OpLength op_length) {
    const int size=int(code.size());
    auto cp=[&](int p){return p>=0 && p+2<size?(int(code[p+1])<<8)|code[p+2]:-1;};
    auto jump=[&](int p){return p>=0 && p+2<size?p+int16_t((int(code[p+1])<<8)|code[p+2]):-1;};
    auto op=[&](int p,int value){return p>=0 && p<size && code[p]==value;};
    if(!op(next_pc,0xb2) || !is_long_static(cp(next_pc)) || !op(next_pc+3,0x65))return {};
    const int anchor=cp(next_pc);
    int p=next_pc+4,site=clock_pc;int64_t period=0;
    if(op(p,0x14)) {
        period=constant(cp(p));p+=3;
        if(period<8 || period>120)return {};
        if(!op(p,0x94))return {};
        const int branch=++p;
        const int ready=(op(branch,0x9b)||op(branch,0x9e))?branch+3:jump(branch);
        if(ready<0 || ready>=size)return {};
        if(op(branch,0x9b)||op(branch,0x9e))p=jump(branch);
        else if(op(branch,0x9c)||op(branch,0x9d))p=branch+3;
        else return {};
        if(op(branch,0x9e)||op(branch,0x9d))++period;
        bool closed=false;
        for(int n=0;n<8 && p>=0 && p<size;++n) {
            if(p==clock_pc){closed=true;break;}
            if(op(p,0)){++p;continue;}
            if(op(p,0xb8)&&is_yield(cp(p))){p+=3;continue;}
            if(op(p,0xa7)&&jump(p)==clock_pc){closed=true;break;}
            return {};
        }
        if(!closed)return {};
    } else if(op(p,0xb3)&&is_long_static(cp(p))) {
        const int difference=cp(p);p+=3;
        if(difference==anchor)return {};
        if(op(p,0xa7))p=jump(p);
        const int guard=p;
        if(!op(p,0xb2)||cp(p)!=difference||!op(p+3,0x09)||!op(p+4,0x94)||!op(p+5,0x9e))return {};
        const int exit=jump(p+5);p+=8;
        if(exit<0 || exit>=size)return {};
        if(!op(p,0xb2)||cp(p)!=difference||!op(p+3,0x14)||!op(p+6,0x94)||!op(p+7,0x9c)||jump(p+7)!=exit)return {};
        period=constant(cp(p+3));p+=10;
        if(!op(p,0xb8)||!is_yield(cp(p)))return {};
        p+=3;site=p;
        if(!op(p,0xb8)||!is_clock(cp(p))||!op(p+3,0xb2)||cp(p+3)!=anchor||
           !op(p+6,0x65)||!op(p+7,0xb3)||cp(p+7)!=difference||!op(p+10,0xa7)||jump(p+10)!=guard)return {};
        if(exit>=guard && exit<p+13)return {};
        // A separate initial read and the repeated read share one logical site.
        if(clock_pc!=site && next_pc+7!=guard)return {};
    } else return {};
    if(period<8 || period>120)return {};
    int anchor_clock=-1,writes=0;
    for(int at=0;at<size;) {
        const int length=op_length(at);
        if(length<=0 || at+length>size)return {};
        if(op(at,0xb3)&&cp(at)==anchor)++writes;
        if(at<clock_pc&&op(at,0xb8)&&is_clock(cp(at))&&field_clock_store(code,at+3)==anchor)anchor_clock=at;
        at+=length;
    }
    if(anchor_clock<0 || writes!=1)return {};
    TimeGatePattern gate;gate.millis=period;gate.static_field=anchor;
    gate.anchor_clock_pc=anchor_clock;gate.site_pc=site;return gate;
}

struct PollGateState {
    const void* method = nullptr;
    int pc = -1;
    int64_t previous_ms = 0, deadline_ms = 0;
    unsigned reads = 0;
    bool waiting = false, certified = false, fresh_ready = false;
    void reset() { *this = {}; }
    bool observe(const void* owner, int site, int64_t previous, int64_t period,
                 int64_t now_ms, bool has_drawn) {
        fresh_ready=false;
        if (!has_drawn || previous < 0 || period > INT64_MAX - previous) { reset(); return false; }
        if (method != owner || pc != site) { reset(); method = owner; pc = site; }
        if (previous_ms != previous) { previous_ms = previous; reads = 0; waiting = false; }
        deadline_ms = previous + period;
        if (now_ms < deadline_ms) {
            waiting = true; reads = std::min(1000000u, reads+1); return false;
        }
        fresh_ready=waiting && reads>=4;
        certified = certified || fresh_ready;
        const bool verified = certified;
        waiting = false; reads = 0;
        return verified;
    }
    int64_t wait_us(int64_t now_ms, double scale, bool paced_verified = false) const {
        if (!waiting || reads < 4 || !std::isfinite(scale) || scale < 1.0 || now_ms >= deadline_ms) return 0;
        const double remaining = double(deadline_ms-now_ms)*1000.0/scale;
        // Only a certified gate owned by the paced drawing thread earns the
        // smaller busy margin. Native and unverified loops retain one ms.
        // Each bounded nap still returns to Java and services audio normally.
        const double margin = paced_verified && certified ? 250.0 : 1000.0;
        return remaining > margin ? int64_t(std::min(500.0, remaining-margin)) : 0;
    }
};
