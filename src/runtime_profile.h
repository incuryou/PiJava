#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include "cpu_clock.h"

// Host wall time, including interpreter work, synchronous paint and blocking.
// The remaining wall estimate is not CPU time. All scopes are reset-generation
// bound so a lifecycle transition cannot charge the next reporting window.
struct RuntimeProfileWindow {
    int64_t worker_gross_us=0, worker_syncpaint_us=0;
    int64_t worker_capwait_us=0, worker_pollwait_us=0, worker_nativewait_us=0;
    int64_t worker_paint_blocked_us=0, gc_us=0, worker_gc_us=0, worker_paint_gc_us=0;
    int64_t worker_cpu_ns=0, paint_cpu_ns=0, worker_paint_cpu_ns=0;
    int64_t gc_cpu_ns=0, worker_gc_cpu_ns=0, worker_paint_gc_cpu_ns=0;
    uint64_t cpu_samples=0, cpu_unavailable=0;
    uint64_t gc_count=0, worker_gc_count=0, worker_turns=0, worker_yields=0, worker_suspends=0, early_repaint_handoffs=0;
    int64_t remaining_wall_us() const {
        return std::max<int64_t>(0,worker_gross_us-worker_syncpaint_us-
            worker_capwait_us-worker_pollwait_us-worker_nativewait_us+worker_paint_blocked_us-
            worker_gc_us+worker_paint_gc_us);
    }
};
struct RuntimeProfile {
    enum class Kind { Worker, Paint, CapWait, PollWait, NativeWait, GC };
    struct Token { uint64_t generation=0; int64_t started=0; bool active=false, worker=false, paint=false; int64_t started_cpu_ns=-1; };
    uint64_t generation=1;
    unsigned worker_depth=0, paint_depth=0;
    RuntimeProfileWindow pending;
    using CpuClock = int64_t (*)();
    CpuClock cpu_clock = j2me_cpu_clock::now_ns;
    void reset() {++generation;worker_depth=paint_depth=0;pending={};}
    Token begin(Kind kind,int64_t now) {
        Token t{generation,now,true,worker_depth>0,paint_depth>0};
        if(kind==Kind::Worker) {t.active=worker_depth++==0;t.worker=true;}
        if(kind==Kind::Paint) {t.active=paint_depth++==0;}
        if(t.active && (kind==Kind::Worker || kind==Kind::Paint || kind==Kind::GC))t.started_cpu_ns=cpu_clock();
        return t;
    }
    Token begin_worker(int64_t now) {return begin(Kind::Worker,now);}
    Token begin_paint(int64_t now) {return begin(Kind::Paint,now);}
    bool end(Kind kind,Token t,int64_t now) {
        if(t.generation!=generation)return false;
        if(kind==Kind::Worker && worker_depth)--worker_depth;
        if(kind==Kind::Paint && paint_depth)--paint_depth;
        if(!t.active)return true;
        if(kind==Kind::Worker || kind==Kind::Paint || kind==Kind::GC) {
            const int64_t cpu=j2me_cpu_clock::difference(t.started_cpu_ns,cpu_clock());
            if(cpu<0)++pending.cpu_unavailable;
            else {
                ++pending.cpu_samples;
                if(kind==Kind::Worker)pending.worker_cpu_ns+=cpu;
                else if(kind==Kind::Paint) {pending.paint_cpu_ns+=cpu;if(t.worker)pending.worker_paint_cpu_ns+=cpu;}
                else {pending.gc_cpu_ns+=cpu;if(t.worker){pending.worker_gc_cpu_ns+=cpu;if(t.paint)pending.worker_paint_gc_cpu_ns+=cpu;}}
            }
        }
        const int64_t elapsed=std::max<int64_t>(0,now-t.started);
        if(kind==Kind::Worker)pending.worker_gross_us+=elapsed;
        else if(kind==Kind::Paint) {if(t.worker)pending.worker_syncpaint_us+=elapsed;}
        else if(kind==Kind::GC) {
            pending.gc_us+=elapsed;++pending.gc_count;
            if(t.worker) {pending.worker_gc_us+=elapsed;++pending.worker_gc_count;if(t.paint)pending.worker_paint_gc_us+=elapsed;}
        } else if(t.worker) {
            if(kind==Kind::CapWait)pending.worker_capwait_us+=elapsed;
            else if(kind==Kind::PollWait)pending.worker_pollwait_us+=elapsed;
            else pending.worker_nativewait_us+=elapsed;
            if(t.paint)pending.worker_paint_blocked_us+=elapsed;
        }
        return true;
    }
    bool end_worker(Token t,int64_t now) {return end(Kind::Worker,t,now);}
    bool end_paint(Token t,int64_t now) {return end(Kind::Paint,t,now);}
    // Only the host reporting point takes a completed window, outside scopes.
    RuntimeProfileWindow take() {auto w=pending;pending={};return w;}
    static int64_t clock_us() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};
class RuntimeProfileScope {
    RuntimeProfile& profile;
    RuntimeProfile::Kind kind;
    RuntimeProfile::Token token;
public:
    RuntimeProfileScope(RuntimeProfile& p,RuntimeProfile::Kind k):profile(p),kind(k),token(p.begin(k,RuntimeProfile::clock_us())) {}
    ~RuntimeProfileScope() {profile.end(kind,token,RuntimeProfile::clock_us());}
    RuntimeProfileScope(const RuntimeProfileScope&)=delete;
    RuntimeProfileScope& operator=(const RuntimeProfileScope&)=delete;
};
