// P2.6: scheduling fairness and thread-switch order are the same
//   - on the desktop frontend path and on the Switch host path (both drive the ONE scheduler core; the Switch
//     path is reproduced with its own hook shapes: frame acquisition before the wake wait, CPU and GPU variants), and
//   - between the old (legacy) loop and the new core (run this binary with J2ME_SCHED_LEGACY=1 and compare).
// Output: PATH lines with the exact switch-order signature (Java-recorded thread id log, compressed) and fairness.
#include <cassert>
#include <iostream>
#include <string>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "host_turn_harness.h"
static int fx;
static int sint(const char* n){auto* f=find_field(fx,n,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static int sref(const char* n,const char* d){auto* f=find_field(fx,n,d);assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].ref;}
struct Result { std::string order; int counts[4]{}; int maxrun=0, len=0, paints=0, sleeper=0, turns=0, acquires=0; };
template<class Turn> static Result drive(Turn turn) {
    stop_thread(); launch_class("SchedParity"); fx=ensure_class("SchedParity"); VMS_PLAT(g_last_present_us)=0;
    VMS_EXEC(g_speed_cap)=0; speed_sync_clock();
    Result r; int64_t t0=monotonic_ms();
    while(monotonic_ms()-t0<6000){ host_lifecycle_poll242(); turn(r); ++r.turns; if(sint("done")==3&&sint("sleeperSteps")>=20)break; }
    r.len=sint("len"); const int log=sref("log","[I"); int run=0,prev=0;
    for(int i=0;i<r.len;++i){int id=VMS_HEAP(g_heap)[log].elems.get(i).i; r.counts[id]++; run=id==prev?run+1:1; prev=id; r.maxrun=std::max(r.maxrun,run);
        if(i<60)r.order+=char('0'+id);}
    // rotation signature over the whole log: every window of 3 must contain each live worker once while all 3 live
    r.paints=sint("paints"); r.sleeper=sint("sleeperSteps"); return r;
}
static std::string fmt(const char* name,const Result& r){
    return std::string("PATH ")+name+" mode="+(sched_legacy()?"legacy":"unified")+" order60="+r.order+" counts="+std::to_string(r.counts[1])+"/"+std::to_string(r.counts[2])+"/"+std::to_string(r.counts[3])+
        " max_consecutive_same_thread="+std::to_string(r.maxrun)+" len="+std::to_string(r.len)+" sleeper_steps="+std::to_string(r.sleeper)+" paints_gt0="+std::to_string(r.paints>0);
}
static bool rotation_ok(int log,int len){ // strict round robin while all three are live
    for(int i=3;i<len;++i){int a=VMS_HEAP(g_heap)[log].elems.get(i).i,b=VMS_HEAP(g_heap)[log].elems.get(i-3).i;if(a!=b)return false;}
    return true;
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    VMS_EXEC(g_completed_paint241).enabled=true;
    int fails=0; std::string sig;
    auto check=[&](const char* name,const Result& r){
        const std::string line=fmt(name,r); std::cout<<line<<"\n";
        const bool rot=rotation_ok(sref("log","[I"),r.len);
        const bool fair=r.counts[1]==400&&r.counts[2]==400&&r.counts[3]==400&&r.maxrun==1&&r.sleeper==20&&r.paints>0;
        std::cout<<"CHECK "<<(rot&&fair?"ok":"FAIL")<<" "<<name<<" strict_round_robin="<<rot<<" fair="<<fair<<"\n"; if(!(rot&&fair))++fails;
        const std::string s=r.order+"|"+std::to_string(r.maxrun)+"|"+std::to_string(r.counts[1])+"/"+std::to_string(r.counts[2])+"/"+std::to_string(r.counts[3]);
        if(sig.empty())sig=s; else if(s!=sig){++fails;std::cout<<"CHECK FAIL "<<name<<" signature differs from first path\n";}
    };
    if(sched_legacy()){
        check("legacy_harness_loop",drive([](Result&){static int64_t lp=0;harness_host_turn(lp,true);}));
    } else {
        // desktop frontend shape (desktop_host.inc): no acquire, present = sleep to the 16 ms frame
        check("desktop_core",drive([](Result&){const int64_t b=monotonic_ms();sched_game_work(false,[]{},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
            sched_present_turn(true,false,false,3,[&]{int64_t l=16-(monotonic_ms()-b);if(l>0)std::this_thread::sleep_for(std::chrono::milliseconds(l));});}));
        // Switch host shape (switch_host.inc): acquire_cpu_frame before the wake wait, CPU presenter, cpu_cleared>=3
        check("switch_core_cpu",drive([](Result& r){sched_game_work(false,[&]{++r.acquires;},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
            sched_present_turn(true,false,false,3,[]{std::this_thread::sleep_for(std::chrono::milliseconds(16));});}));
        // Switch GPU variant (deko3d presenter: scheduled_frame without wake budget)
        check("switch_core_gpu",drive([](Result& r){sched_game_work(true,[&]{++r.acquires;},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
            sched_present_turn(true,false,true,3,[]{std::this_thread::sleep_for(std::chrono::milliseconds(16));});}));
        // the desktop harness helper itself (what city/savestate gates run)
        check("harness_core",drive([](Result&){static int64_t lp=0;harness_host_turn(lp,true);}));
    }
    sched_core_report();
    std::cout<<"SIGNATURE "<<sig<<"\n";
    std::cout<<"SCHED_PARITY_RESULT "<<(fails?"FAIL":"PASS")<<" fails="<<fails<<" mode="<<(sched_legacy()?"legacy":"unified")<<"\n";
    return fails?1:0;
}
