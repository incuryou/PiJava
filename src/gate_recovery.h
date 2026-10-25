#pragma once
#include "auto_fps.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Remember proof, not an active accelerated clock. During a timeout/loading
// gap Java time returns to 1x. Only a newly certified ready edge at the SAME
// method/PC/constant period and speed setting can reuse proof for at most 2 s.
// Pause, stop, foreign gates and mode changes discard this runtime-only state.
struct GateRecovery {
    const void* method=nullptr;
    int pc=-1,cap_period=0;
    int64_t saved_us=0,period_us=0;
    unsigned stable=0;
    double rate=0,pending_rate=0;
    static constexpr int64_t ttl_us=2000000;
    void reset(){*this={};}
    bool remember(const AutoFpsLearner& a,int cap,double active_rate,int64_t now) {
        reset();
        if(!a.verified_gate_seen || a.stable<5 || !a.gate_method || a.gate_pc<0 ||
           cap<=0 || a.candidate<8000 || a.candidate>120000 ||
           !std::isfinite(active_rate) || active_rate<=1.01 || active_rate>8)return false;
        method=a.gate_method;pc=a.gate_pc;cap_period=cap;saved_us=now;
        period_us=int64_t(std::llround(a.candidate));stable=a.stable;rate=active_rate;return true;
    }
    bool restore(AutoFpsLearner& a,const void* m,int at,int64_t period,int cap,int64_t now) {
        if(!method)return false;
        if(now<saved_us || now-saved_us>ttl_us || m!=method || at!=pc || period!=period_us || cap!=cap_period) {
            reset();return false;
        }
        // Caller must have observed a NEW ready edge via PollGateState and
        // opened this exact group. This does not infer a gate from a load gap.
        if(!a.started || !a.group_open || !a.verified_gate_seen || a.gate_method!=m || a.gate_pc!=at ||
           a.verified_period_us!=period)return false;
        a.candidate=double(period);a.stable=std::max(5u,stable);
        pending_rate=std::min(rate,std::max(1.0,std::min(8.0,double(period)/cap)));
        method=nullptr;return true;
    }
    double consume(double learned,int cap,int64_t now) {
        const double out=pending_rate;pending_rate=0;
        if(out<=1.01 || cap<=0 || cap!=cap_period || now<saved_us || now-saved_us>ttl_us ||
           std::fabs(learned-std::max(1.0,std::min(8.0,double(period_us)/cap)))>0.03)return 0;
        return std::min(learned,out);
    }
};
