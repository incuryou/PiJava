// P2.2 stall watchdog (test-side, log only). It never intervenes: no timeout, no signal, no exception handling,
// no write to VM state. A background thread samples a progress counter; when nothing progressed for more than
// 5 s of steady time it dumps a read-only best-effort snapshot (the main thread is presumed stuck, so reads are
// unsynchronized by design). It also reports its own oversleeps with BOTH clocks, which separates
// "VM stuck while the process runs" (steady and realtime advance together) from "whole process not scheduled /
// suspended" (the watchdog itself oversleeps; realtime and steady can diverge).
// Include after the emulator source. Call stall_watchdog_progress() from the harness loop; it also counts VM ops.
#pragma once
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#ifdef VMS_EXEC
#define SWD_E(n) VMS_EXEC(n)
#define SWD_R(n) VMS_RS(n)
#define SWD_C(n) VMS_CL(n)
#define SWD_H(n) VMS_HEAP(n)
#else
#define SWD_E(n) n
#define SWD_R(n) n
#define SWD_C(n) n
#define SWD_H(n) n
#endif
namespace swd {
inline std::atomic<uint64_t> progress{0};
inline std::atomic<bool> stop{false};
inline std::atomic<int64_t> stalls{0}, self_gaps{0}, max_gap_ms{0};
inline std::thread worker;
inline int64_t steady_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline int64_t real_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
inline int64_t t0s=0,t0r=0;
inline void line(const char* s){std::fprintf(stderr,"%s\n",s);std::fflush(stderr);}
inline const char* cname(int cid){ if(cid<0||size_t(cid)>=SWD_C(g_classes).size())return "?"; return SWD_C(g_classes)[cid].name.c_str(); }
inline void dump(int64_t stalled_ms){
    char b[512];
    std::snprintf(b,sizeof b,"WATCHDOG STALL no_progress_ms=%lld steady_ms=%lld real_ms=%lld run_depth=%d pump_depth=%d paint_depth=%d repaint=%d serial_queue=%zu",
        (long long)stalled_ms,(long long)(steady_ms()-t0s),(long long)(real_ms()-t0r),int(SWD_E(g_run_depth)),int(SWD_E(g_pump_depth)),int(SWD_R(g_paint_depth)),
        int(SWD_R(g_repaint)),SWD_E(g_serial_calls).size());line(b);
    { size_t i=0; std::string q="WATCHDOG serial_head="; for(int r:SWD_E(g_serial_calls)){ if(i++>=8)break; q+=std::to_string(r)+","; } line(q.c_str()); }
#ifdef SWD_S4 // P2.2-S2S4+ observation fields (trace switch must be on for them to move)
    std::snprintf(b,sizeof b,"WATCHDOG serviceRepaints s4_calls=%llu s4_target=%llu paint_id=%llu trace_on=%d",(unsigned long long)s0.s4_calls,(unsigned long long)s0.s4_target,(unsigned long long)s0.paint_id,int(s0.on));line(b);
#else
    line("WATCHDOG serviceRepaints state=not_instrumented_in_this_build (see repaint/paint_depth above)");
#endif
    auto th=[&](const char* tag,const JavaThreadContext& t){
        const size_t n=t.frames.size(); const char* m="-"; const char* c="-"; int pc=-1;
        if(n){ const Frame& f=t.frames[n-1]; if(f.method){ m=f.method->name.c_str(); c=cname(f.method->class_id);} pc=f.pc; }
        std::snprintf(b,sizeof b,"WATCHDOG thread %s ref=%d state=%d executing=%d frames=%zu top=%s.%s pc=%d monitor=%d",tag,t.ref,int(t.state),int(t.executing),n,c,m,pc,t.monitor);line(b);
    };
    th("main",SWD_E(g_main_thread)); for(auto& t:SWD_E(g_threads))th("java",t);
    if(auto* st=SWD_E(g_active_stack)){ const size_t n=st->size(); if(n){ const Frame& f=(*st)[n-1];
        std::snprintf(b,sizeof b,"WATCHDOG active_stack frames=%zu top=%s.%s pc=%d",n,f.method?cname(f.method->class_id):"?",f.method?f.method->name.c_str():"?",f.pc);line(b);} }
    for(auto& kv:SWD_E(g_monitors)) if(kv.second.owner){ std::snprintf(b,sizeof b,"WATCHDOG monitor ref=%d owner=%d depth=%d waiters=%zu",kv.first,kv.second.owner->ref,kv.second.depth,kv.second.waiters.size());line(b);}
}
inline void start(){
    t0s=steady_ms();t0r=real_ms();
    worker=std::thread([]{
        uint64_t last=progress.load(); int64_t last_change=steady_ms(); bool dumped=false;
        int64_t ps=steady_ms(),pr=real_ms();
        while(!stop.load()){
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            const int64_t s=steady_ms(),r=real_ms(); const int64_t ds=s-ps,dr=r-pr; ps=s;pr=r;
            if(ds>1000||dr>1000||dr-ds>500||ds-dr>500){ ++self_gaps; int64_t g=std::max(ds,dr); if(g>max_gap_ms)max_gap_ms=g;
                char b[200];std::snprintf(b,sizeof b,"WATCHDOG SELF_GAP watchdog_sleep_250ms_took steady_ms=%lld real_ms=%lld (process not scheduled or clocks stepped)",(long long)ds,(long long)dr);line(b);}
            const uint64_t p=progress.load()+uint64_t(SWD_E(g_fast_ops))+uint64_t(SWD_E(g_slow_ops));
            if(p!=last){last=p;last_change=s;if(dumped){char b[160];std::snprintf(b,sizeof b,"WATCHDOG STALL_END steady_ms=%lld",(long long)(s-t0s));line(b);}dumped=false;continue;}
            if(!dumped&&s-last_change>5000){++stalls;dump(s-last_change);dumped=true;}
        }
    });
}
inline void finish(){ stop=true; if(worker.joinable())worker.join();
    char b[200];std::snprintf(b,sizeof b,"WATCHDOG_SUMMARY stalls=%lld self_gaps=%lld max_gap_ms=%lld steady_total_ms=%lld real_total_ms=%lld",
        (long long)stalls.load(),(long long)self_gaps.load(),(long long)max_gap_ms.load(),(long long)(steady_ms()-t0s),(long long)(real_ms()-t0r));
    line(b); std::printf("%s\n",b); }
}
