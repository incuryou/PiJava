#include <cassert>
#define main emulator_main
#ifndef CITY_MONITOR_VM_SOURCE
#define CITY_MONITOR_VM_SOURCE "../src/j2mevm.cpp"
#endif
#include CITY_MONITOR_VM_SOURCE
#undef main
#ifndef VMS_EXEC
#define VMS_EXEC(n) n  // r01 negative control: pre-P2 VM source
#endif
#ifndef VMS_PLAT
#define VMS_PLAT(n) n  // r01 negative control: pre-P2 VM source
#endif
#ifndef VMS_RS
#define VMS_RS(n) n  // r01 negative control: pre-P2 VM source
#endif
#ifndef VMS_CL
#define VMS_CL(n) n  // r01 negative control: pre-P2 VM source
#endif
#ifndef VMS_DIAG
#define VMS_DIAG(n) n  // the r01 negative control compiles this test against the pre-P2 VM source
#endif
static int fixture;
static int value(const char* name){
    auto* f=find_field(fixture,name,"I");assert(f&&f->is_static);
    return VMS_CL(g_classes)[f->owner].statics[f->slot].i;
}
static void invoke(const char* name,int argument){
    auto* method=find_method(fixture,name,"(I)I",false);assert(method);
    assert(run(method,{Value::of_i(argument)}).i==1);assert(!VMS_EXEC(g_thrown));
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};
    VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();fixture=ensure_class("CityMonitorContinuationChecks");
    for(bool boundary:{false,true})for(bool handoff:{false,true})for(int mode:{0,1,2}){
        stop_thread();VMS_RS(g_host_boundary242_enabled)=boundary;VMS_EXEC(g_completed_paint241).enabled=handoff;
        VMS_EXEC(g_speed_cap)=0;VMS_EXEC(g_midlet_state)=MidletState::Active;VMS_PLAT(g_host_paused)=false;
        VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);speed_sync_clock();invoke("setup",mode);
        host_events();VMS_RS(g_repaint)=false;VMS_RS(g_fb_dirty)=false;
        const int canvas=VMS_RS(g_current);assert(canvas>0);
        for(int token:{157,293}){
            invoke("arm",token);const int64_t began=monotonic_ms();
            dispatch_key(53,false);const int64_t key_ms=monotonic_ms()-began;
            assert(value("keyDone")==1&&value("lockHeld")==0&&value("beforeService")==1);
            assert(key_ms>=20); // A real Java sleep, not a forced timeout return.
            if(mode!=2)assert(value("paints")==0&&value("afterService")==0);
            else assert(value("paints")==1&&value("afterService")==1&&value("plainWhileLocked")==1);
            for(int turn=0;turn<32&&VMS_EXEC(g_thread_live);++turn){
                p1_host_work_turn(false,false,[](bool){advance_game_frame();},
                    [](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
                if(VMS_RS(g_fb_dirty))p1_host_post_present(true,false);
            }
            assert(!VMS_EXEC(g_thread_live)&&value("workerDone")==1&&value("beforeService")==1&&value("afterService")==1);
            assert(value("paints")==1&&value("seenToken")==token&&value("keyDone")==1);
            assert(value("order")== (mode==2?231:123));
            assert(VMS_EXEC(g_run_depth)==0&&VMS_EXEC(g_pump_depth)==0&&VMS_RS(g_paint_depth)==0&&!VMS_EXEC(g_thrown));
            auto monitor=VMS_EXEC(g_monitors).find(canvas);assert(monitor==VMS_EXEC(g_monitors).end()||!monitor->second.owner);
            dispatch_key(53,true);
            std::cout<<"PASS city_monitor B="<<boundary<<" H="<<handoff<<" mode="<<mode
                     <<" token="<<token<<" key_ms="<<key_ms<<" order="<<value("order")
                     <<" paints=1 worker_completed=1 operands_preserved=1 locks_released=1\n";
        }
    }
    stop_thread();std::cout<<"PASS city_monitor cases=24 matrix=4 paint_modes=3 rounds=2\n";
}
