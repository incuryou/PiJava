#include <cassert>
#include <algorithm>
#include <iostream>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/p1_host_turn.h"

static int fixture;
static int acquisitions,schedules,sleeps,presents;
static int input_edges;
static int64_t slept_ms;
static size_t max_queue;
static int value(const char* name){auto* f=find_field(fixture,name,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static int invoke(const char* name,const char* descriptor="()I",std::vector<Value> args={}){
    auto* m=find_method(fixture,name,descriptor,false);assert(m);int result=run(m,args).i;assert(!VMS_EXEC(g_thrown));return result;
}
static void setup(bool boundary,bool handoff,int mode){
    launch_class("P1HostChecks");fixture=ensure_class("P1HostChecks");
    VMS_RS(g_host_boundary242_enabled)=boundary;VMS_EXEC(g_completed_paint241).enabled=handoff;
    VMS_RS(g_m3g_mesh242_enabled)=VMS_RS(g_m3g_sprite241_enabled)=VMS_RS(g_m3g_prep241_enabled)=VMS_EXEC(g_fast_float241_enabled)=true;
    VMS_EXEC(g_speed_cap)=0;speed_sync_clock();
    assert(invoke("setup","(I)I",{Value::of_i(mode)})==1);
    VMS_RS(g_fb_dirty)=VMS_RS(g_repaint)=false;VMS_EXEC(g_last_slice_us)=0;
    acquisitions=schedules=sleeps=presents=input_edges=0;slept_ms=0;max_queue=VMS_EXEC(g_serial_calls).size();
}
static p1::Work work(bool heartbeat=false){
    return p1_host_work_turn(false,heartbeat,[&](bool gpu){
        ++schedules;
        if(gpu)scheduled_frame([&]{++acquisitions;},game_wake_delay,
            [&](int64_t ms){assert(ms>0);},advance_game_frame,0);
        else scheduled_frame([&]{++acquisitions;},game_wake_delay,
            [&](int64_t ms){assert(ms>0);},advance_game_frame,VMS_EXEC(g_frame_wait_budget),paint_clock_us,&VMS_EXEC(g_wake_stats));
    },[&](int64_t ms){assert(ms>0);++sleeps;slept_ms+=ms;});
}
static void tick(){
    host_lifecycle_poll242();
    if(input_edges<2){dispatch_key(53,input_edges==1);++input_edges;}
    max_queue=std::max(max_queue,VMS_EXEC(g_serial_calls).size());work();
    if(p1::should_show(true,false,false,VMS_RS(g_fb_dirty),3,false,speed_group_incomplete())){
        ++presents;p1_host_post_present(true,false);
    }
    max_queue=std::max(max_queue,VMS_EXEC(g_serial_calls).size());
    assert(VMS_RS(g_paint_depth)==0&&VMS_EXEC(g_run_depth)==0&&!VMS_EXEC(g_thrown));
}
static void until_done(bool worker=false){
    for(int i=0;i<96&&(value("calls")<6||(worker&&VMS_EXEC(g_thread_live)));++i)tick();
    assert(value("calls")==6&&value("order")==123456&&VMS_EXEC(g_serial_calls).empty());
    assert(max_queue<=2);
    if(worker)assert(value("workers")==6&&value("afterService")==6&&!VMS_EXEC(g_thread_live));
}
static void decisions(){
    using p1::Work;p1::Turn t;
    t.nap_ms=4;assert(p1::decide(t)==Work::Idle);
    t.pending=true;assert(p1::decide(t)==Work::Advance);
    t.boundary_ready=true;assert(p1::decide(t)==Work::Ready);
    t.boundary_ready=false;t.pending=false;t.incomplete=true;assert(p1::decide(t)==Work::Advance);
    t.incomplete=false;t.nap_ms=0;t.gpu=true;assert(p1::decide(t)==Work::GpuScheduled);
    t.gpu=false;assert(p1::decide(t)==Work::CpuScheduled);
    t.runnable=true;t.last_slice_us=1000;assert(p1::decide(t)==Work::Worker);
    assert(p1::second_worker(false,false,true)&&!p1::second_worker(false,true,true));
    assert(p1::speculate(true,false,true,false,true)&&!p1::speculate(true,true,true,false,true));
    assert(!p1::should_show(true,false,false,true,3,false,true));
    // Old production nap excludes queued host work. If heartbeat crosses
    // DURING the nap, the bottom presents and resets its timestamp without
    // ever advancing Java. Repeat deterministically to demonstrate starvation.
    int64_t now=1000,last=1000;int legacy_advances=0;
    for(int i=0;i<32;++i){
        now=last+49000;t={};t.pending=true;t.nap_ms=4;
        t.heartbeat=now-last>=50000;
        assert(p1::decide(t)==Work::Advance);
        t.pending=false;const auto old_work=p1::decide(t);assert(old_work==Work::Idle);
        if(old_work!=Work::Idle&&old_work!=Work::Ready)++legacy_advances;
        now+=4000;
        assert(p1::should_show(true,false,false,false,3,now-last>=50000,false));
        last=now;
    }
    assert(legacy_advances==0);
    std::cout<<"PASS D_cross_nap_heartbeat legacy_idle_present_cycles=32 legacy_advances=0 fixed_decision=advance\n";
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    decisions();
    for(bool boundary:{false,true})for(bool handoff:{false,true}){
        setup(boundary,handoff,0);assert(!VMS_EXEC(g_thread_live)&&!VMS_RS(g_fb_dirty)&&!VMS_RS(g_repaint));
        const int64_t enqueue_us=paint_clock_us();until_done();
        assert(value("paints")>=1);
        std::cout<<"PASS A B="<<boundary<<" H="<<handoff<<" serials=6 FIFO=123456 paints="<<value("paints")
                 <<" max_queue="<<max_queue<<" enqueue_to_completion_observed_us="<<paint_clock_us()-enqueue_us<<" scope=desktop_monotonic_upper_bound\n";
        setup(boundary,handoff,1);assert(invoke("startWorker")==1);until_done(true);
        std::cout<<"PASS B B="<<boundary<<" H="<<handoff<<" worker=6 serials=6 recursion=0 max_queue="<<max_queue
                 <<" completed_handoffs="<<VMS_EXEC(g_completed_paint241).handoffs<<"\n";
        setup(boundary,handoff,2);assert(invoke("startWorker")==1);until_done(true);
        assert(value("syncPairs")==12&&value("releases")==12);
        std::cout<<"PASS C B="<<boundary<<" H="<<handoff<<" synchronous_resource_pairs=12 released_after_completion=12\n";
        setup(boundary,handoff,3);assert(!game_runnable()&&!VMS_RS(g_fb_dirty));
        // Defer prologue intentionally only for boundary=1, just as production.
        host_lifecycle_poll242();int before=value("calls");work(false);
        assert(value("calls")>before||value("calls")==6);assert(sleeps==0);
        until_done();VMS_RS(g_fb_dirty)=VMS_RS(g_repaint)=false;int old_sleeps=sleeps;work(false);
        assert(sleeps==old_sleeps+1&&slept_ms>0);
        std::cout<<"PASS D B="<<boundary<<" H="<<handoff<<" pending_serial_advance=1 empty_idle_sleep_ms="<<slept_ms<<"\n";
        // A deferred startup repaint is retained work, not work eligible to
        // draw now. It must not suppress idle while its producer is asleep.
        setup(boundary,handoff,3);VMS_EXEC(g_serial_calls).clear();VMS_DIAG(g_p1).serial_times.clear();
        VMS_EXEC(g_farm_startup).start("b3700e3a88c48d35ed925788140714a47ea6a10375d7efd51ff71f623e037b0a",monotonic_ms());
        VMS_RS(g_repaint)=true;VMS_RS(g_fb_dirty)=false;
        assert(!game_runnable()&&!VMS_PLAT(g_audio).pendingEnds()&&farm_startup_paint_waiting243());
        auto deadline=VMS_EXEC(g_farm_startup).deadline;int before_paints=value("paints");
        assert(work(false)==p1::Work::Idle&&sleeps==1&&slept_ms>0);
        assert(VMS_RS(g_repaint)&&VMS_EXEC(g_farm_startup).waiting()&&VMS_EXEC(g_farm_startup).deadline==deadline&&value("paints")==before_paints);
        assert(work(true)==p1::Work::CpuScheduled&&schedules==1);
        assert(VMS_RS(g_repaint)&&VMS_EXEC(g_farm_startup).waiting()&&!VMS_RS(g_fb_dirty)&&value("paints")==before_paints);
        assert(work(false)==p1::Work::Idle&&sleeps==2);
        VMS_EXEC(g_farm_startup).state=FarmStartup::State::Ready;
        assert(!farm_startup_paint_waiting243()&&p1_pending_work());
        assert(work(false)==p1::Work::Advance&&value("paints")==before_paints+1&&!VMS_RS(g_repaint));
        p1_host_post_present(true,false);
        VMS_EXEC(g_farm_startup).state=FarmStartup::State::Waiting;
        VMS_EXEC(g_farm_startup).deadline=monotonic_ms()-1;VMS_RS(g_repaint)=true;
        assert(!farm_startup_paint_waiting243()&&p1_pending_work());
        assert(work(false)==p1::Work::Advance&&VMS_EXEC(g_farm_startup).state==FarmStartup::State::Failed);
        assert(value("paints")==before_paints+2&&!VMS_RS(g_repaint));
        VMS_EXEC(g_farm_startup)=FarmStartup{};p1_host_post_present(true,false);
        std::cout<<"PASS startup_deferred_repaint B="<<boundary<<" H="<<handoff
                 <<" waiting_idle=1 heartbeat_retains=1 following_idle=1 ready_draws=1 deadline_draws=1\n";
        // Independently toggle geometry without assigning boundary.
        bool saved_boundary=VMS_RS(g_host_boundary242_enabled);
        VMS_RS(g_m3g_mesh242_enabled)=false;assert(VMS_RS(g_host_boundary242_enabled)==saved_boundary&&host_boundary242_active()==boundary);
        VMS_RS(g_m3g_mesh242_enabled)=true;assert(VMS_RS(g_host_boundary242_enabled)==saved_boundary&&host_boundary242_active()==boundary);
        setup(boundary,handoff,3);assert(invoke("startAudio")==1);
        int16_t pcm[256];VMS_PLAT(g_audio).render(pcm,128);assert(VMS_PLAT(g_audio).pendingEnds());
        VMS_RS(g_fb_dirty)=true;VMS_EXEC(g_notify_paused)=true;host_lifecycle_poll242();
        assert(VMS_EXEC(g_midlet_state)==MidletState::Paused&&value("calls")==0&&value("ends")==0);
        assert(!VMS_EXEC(g_serial_calls).empty()&&VMS_PLAT(g_audio).pendingEnds()&&VMS_RS(g_fb_dirty));
        work();assert(value("calls")==0&&value("ends")==0);
        p1_host_post_present(true,false);assert(!VMS_RS(g_fb_dirty));
        assert(invoke("resume")==1);host_lifecycle_poll242();until_done();
        assert(VMS_EXEC(g_midlet_state)==MidletState::Active&&value("ends")==1&&!VMS_PLAT(g_audio).pendingEnds());
        assert(invoke("setup","(I)I",{Value::of_i(3)})==1);assert(invoke("startAudio")==1);
        VMS_PLAT(g_audio).render(pcm,128);assert(VMS_PLAT(g_audio).pendingEnds());VMS_RS(g_fb_dirty)=true;
        VMS_EXEC(g_notify_destroyed)=true;host_lifecycle_poll242();
        assert(VMS_EXEC(g_midlet_state)==MidletState::Destroyed&&VMS_EXEC(g_serial_calls).empty()&&!VMS_PLAT(g_audio).pendingEnds());
        int ended=value("ends"),called=value("calls");work();
        assert(value("ends")==ended&&value("calls")==called);
        std::cout<<"PASS E B="<<boundary<<" H="<<handoff<<" pause_retains=1 resume_delivers=1 destroy_cancels=1\n";
    }
    stop_thread();std::cout<<"PASS matrix configurations=4 A_B_C_D_E=20 geometry_independent=4 scope=shared_production_runner_desktop_no_physical_HID_vsync_audio_sink\n";
}
