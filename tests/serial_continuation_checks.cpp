// P2.2-S1: SerialRun continuation acceptance (PX-2 doc 04 row S1):
//  - serial FIFO order (including callSerially from inside a serial task) unchanged,
//  - sync repaint (serviceRepaints inside a serial task / worker) results unchanged (A1 part),
//  - every dispatched Runnable became exactly one SerialRun continuation, none Faulted, none left Running,
//  - two consecutive passes give identical Java-visible results (the old reference path was removed in P2.2-S2S4-fix;
//    its former pass-1 output is kept in docs/P2.2-S2S4-FIX-SELFCHECK-CN.md for comparison).
// Uses the P1HostChecks fixture (tests/java/P1HostChecks.java), shared production dispatch.
#include <cassert>
#include <iostream>
#include <string>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static int fixture;
static int value(const char* n){auto* f=find_field(fixture,n,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static int invoke(const char* n,const char* d="()I",std::vector<Value> a={}){auto* m=find_method(fixture,n,d,false);assert(m);int r=run(m,a).i;assert(!VMS_EXEC(g_thrown));return r;}
static std::string scenario(int mode,bool worker){
    launch_class("P1HostChecks");fixture=ensure_class("P1HostChecks");
    VMS_EXEC(g_speed_cap)=0;speed_sync_clock();
    const uint64_t epoch=g_cont.epoch,created=g_cont.created,completed=g_cont.completed,faulted=g_cont.faulted,viol=g_cont.order_violations,untr=g_cont.untracked;
    assert(invoke("setup","(I)I",{Value::of_i(mode)})==1);
    if(worker)assert(invoke("startWorker")==1);
    for(int i=0;i<200&&(value("calls")<6||(worker&&VMS_EXEC(g_thread_live)));++i){
        host_lifecycle_poll242();advance_game_frame();
        if(VMS_RS(g_fb_dirty))p1_host_post_present(true,false);
        assert(VMS_RS(g_paint_depth)==0&&VMS_EXEC(g_run_depth)==0&&!VMS_EXEC(g_thrown));
        assert(g_cont.running.empty());
    }
    assert(value("calls")==6&&value("order")==123456&&VMS_EXEC(g_serial_calls).empty());
    assert(g_cont.created-created==6&&g_cont.completed-completed==6&&g_cont.faulted==faulted);
    assert(g_cont.order_violations==viol&&g_cont.untracked==untr);
    (void)epoch;
    std::string r="mode="+std::to_string(mode)+" worker="+std::to_string(worker)+" calls="+std::to_string(value("calls"))+" order="+std::to_string(value("order"))+
        " paints="+std::to_string(value("paints"))+" workers="+std::to_string(value("workers"))+" afterService="+std::to_string(value("afterService"))+
        " syncPairs="+std::to_string(value("syncPairs"))+" releases="+std::to_string(value("releases"));
    return r;
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    VMS_EXEC(g_completed_paint241).enabled=true;
    std::string all[2];
    for(int ref=0;ref<2;++ref){
        for(auto mw:{std::make_pair(0,false),std::make_pair(2,true)}){
            const uint64_t e0=g_cont.epoch;
            const std::string s=scenario(mw.first,mw.second);
            assert(g_cont.epoch>e0); // launch/reset opened a new suite epoch
            std::cout<<"CASE pass="<<ref<<" "<<s<<"\n";all[ref]+=s+"\n";
        }
    }
    // Paint count may legitimately depend on wall-clock; FIFO/sync/resource results may not.
    auto strip=[](std::string s){std::string o;size_t p=0;while(p<s.size()){size_t q=s.find("paints=",p);if(q==std::string::npos){o+=s.substr(p);break;}
        o+=s.substr(p,q-p);size_t e=s.find(' ',q);p=e==std::string::npos?s.size():e;}return o;};
    assert(strip(all[0])==strip(all[1]));
    cont_report();
    std::cout<<"SERIAL_CONTINUATION_RESULT PASS created="<<g_cont.created<<" faulted="<<g_cont.faulted<<" order_violations="<<g_cont.order_violations
             <<" untracked="<<g_cont.untracked<<" epochs="<<g_cont.epoch<<" scope=desktop_shared_dispatch\n";
}
