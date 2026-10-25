#include <cassert>
#include <iostream>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

// The VM, host runner and scheduler are included from production. Only host
// acquire/wait/sleep and heartbeat's external clock are injected here.
static int sleeps=0,schedules=0,acquires=0,waits=0;
static int64_t control_us=0,slept_us=0;
static int fixture=0;
static int field(const char* name){auto* f=find_field(fixture,name,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static int invoke(const char* name,const char* desc="()I",std::vector<Value> args={}){
    auto* m=find_method(fixture,name,desc,true);assert(m);const int n=run(m,args).i;assert(!VMS_EXEC(g_thrown));return n;
}
static void flags(bool b,bool h){
    VMS_RS(g_host_boundary242_enabled)=b;VMS_EXEC(g_completed_paint241).enabled=h;
    VMS_RS(g_m3g_mesh242_enabled)=VMS_RS(g_m3g_sprite241_enabled)=VMS_RS(g_m3g_prep241_enabled)=VMS_EXEC(g_fast_float241_enabled)=true;
    VMS_EXEC(g_speed_cap)=0;speed_sync_clock();VMS_EXEC(g_slice_ms)=14;
}
static p1::Work turn(bool heartbeat=false,bool poll=true){
    if(poll)host_lifecycle_poll242();
    const auto selected=p1_host_work_turn(false,heartbeat,[&](bool gpu){
        ++schedules;assert(!gpu);
        scheduled_frame([&]{++acquires;},game_wake_delay,[&](int64_t ms){
            assert(ms>0);++waits;control_us+=ms*1000;
        },advance_game_frame,VMS_EXEC(g_frame_wait_budget),[&]{return control_us;},&VMS_EXEC(g_wake_stats));
    },[&](int64_t ms){assert(ms>0);++sleeps;control_us+=ms*1000;slept_us+=ms*1000;});
    assert(VMS_EXEC(g_run_depth)==0&&VMS_EXEC(g_pump_depth)==0&&VMS_RS(g_paint_depth)==0&&!VMS_EXEC(g_thrown));
    if(p1::should_show(true,false,false,VMS_RS(g_fb_dirty),3,heartbeat,speed_group_incomplete()))p1_host_post_present(true,false);
    return selected;
}
static void drain(){
    const int64_t end=paint_clock_us()+1000000;
    do{turn();}while((VMS_EXEC(g_thread_live)||p1_pending_work())&&paint_clock_us()<end);
    assert(!VMS_EXEC(g_thread_live)&&!p1_pending_work());
}
static void launch(const char* main,const char* suite,bool b,bool h){
    flags(b,h);launch_class(main,suite);fixture=ensure_class(main);flags(b,h);
    VMS_RS(g_fb_dirty)=VMS_RS(g_repaint)=false;sleeps=schedules=acquires=waits=0;slept_us=0;
}
static void reason(int expected){
    for(int i=0;i<4;i++)assert(VMS_DIAG(g_p1).reason_counts[i]==uint64_t(i==expected));
    assert(VMS_DIAG(g_p1).zero_transitions==1&&!VMS_EXEC(g_thread_live)&&VMS_EXEC(g_threads).size()<=1);
}
static void endings(bool b,bool h){
    for(int mode:{0,1,2,3,4,9}){
        const bool reset=mode==4,destroy=mode==9;
        VMS_CL(g_props)["p1.test.mode"]=std::to_string(destroy?4:mode);
        launch("P1ThreadEndingChecks","p1-closeout-ending",b,h);
        if(reset||destroy){
            assert(VMS_EXEC(g_thread_live)&&field("entered")==1&&field("completed")==0);
            turn();assert(VMS_EXEC(g_thread_live)); // production idle path retains sleeping worker
            if(reset)stop_thread();else lifecycle_destroy();
            assert(VMS_EXEC(g_threads).empty()&&VMS_EXEC(g_serial_calls).empty()&&!VMS_PLAT(g_audio).pendingEnds());
            assert(VMS_EXEC(g_midlet_state)==(reset?MidletState::Empty:MidletState::Destroyed));
        }else{
            drain();assert(field("entered")==int(mode!=3));
            assert(field("completed")==int(mode!=2&&mode!=3));
            assert(field("caught")==int(mode==1));
            assert((VMS_DIAG(g_p1).caught_count>0)==(mode==1));
        }
        const int expected=reset?2:destroy?3:mode==2?1:0;reason(expected);
        std::cout<<"PASS F B="<<b<<" H="<<h<<" ending="
            <<(reset?"reset":destroy?"destroy":mode==0?"normal":mode==1?"caught_return":mode==2?"uncaught":"no_target")
            <<" reason="<<expected<<" live_to_zero=1 session="<<VMS_DIAG(g_p1).session_id<<" launch="<<VMS_DIAG(g_p1).launch_id<<'\n';
        if(!reset&&!destroy)lifecycle_destroy();
    }
}
static bool equal_epoch(const P1M3GEpoch& a,const P1M3GEpoch& b){
    return a.identity==b.identity&&a.skin==b.skin&&a.quantize==b.quantize&&a.light==b.light&&a.gathered==b.gathered&&a.emitted==b.emitted&&a.texture==b.texture&&a.span==b.span&&a.row==b.row;
}
static void epoch_report(const char* stage,bool b,bool h){
    std::cout<<"EPOCH stage="<<stage<<" B="<<b<<" H="<<h<<" session="<<VMS_DIAG(g_p1).session_id<<" launch="<<VMS_DIAG(g_p1).launch_id<<'\n';
    p1_report_m3g_epoch();
}
static void transitions(bool b,bool h){
    launch("P1CloseoutChecks","p1-closeout-first",b,h);
    const auto session=VMS_DIAG(g_p1).session_id;
    assert(invoke("queueM3G","(I)I",{Value::of_i(3)})==1);assert(invoke("startWorker")==1);drain();
    assert(field("identities")==3&&field("workers")==1&&field("calls")==1);
    assert(p1_m3g_values().identity-VMS_DIAG(g_p1_m3g_base).identity==3);
    epoch_report("previous_nonzero",b,h);
    for(bool different:{false,true}){
        // Pending work from the old launch must be canceled by actual teardown.
        assert(invoke("queueShort")==1);assert(!VMS_EXEC(g_serial_calls).empty());
        const auto prior_id=VMS_DIAG(g_p1).launch_id;const auto prior_values=p1_m3g_values();
        lifecycle_destroy();
        assert(VMS_EXEC(g_midlet_state)==MidletState::Destroyed&&!VMS_EXEC(g_midlet)&&!VMS_RS(g_current)&&!VMS_RS(g_display));
        assert(VMS_EXEC(g_threads).empty()&&VMS_EXEC(g_serial_calls).empty()&&!VMS_PLAT(g_audio).pendingEnds()&&!VMS_RS(g_repaint));
        assert(field("destroys")==1);
        launch(different?"P1CloseoutOtherChecks":"P1CloseoutChecks",different?"p1-closeout-second":"p1-closeout-first",b,h);
        assert(VMS_DIAG(g_p1).session_id==session&&VMS_DIAG(g_p1).launch_id==prior_id+1);
        assert(VMS_EXEC(g_midlet_state)==MidletState::Active&&VMS_EXEC(g_midlet)&&VMS_RS(g_current));
        assert(equal_epoch(prior_values,p1_m3g_values())&&equal_epoch(VMS_DIAG(g_p1_m3g_base),p1_m3g_values()));
        assert(!VMS_DIAG(g_p1).caught_count&&!VMS_DIAG(g_p1).zero_transitions&&!VMS_DIAG(g_p1).serial_enqueues&&!VMS_DIAG(g_p1).serial_dispatches);
        for(auto count:VMS_DIAG(g_p1).reason_counts)assert(count==0);
        assert(field("starts")==1&&field("calls")==0&&field("identities")==0&&field("workers")==0);
        epoch_report(different?"different_zero":"same_zero",b,h);
        const auto baseline=p1_m3g_values();
        assert(invoke("queueM3G","(I)I",{Value::of_i(2)})==1);assert(invoke("startWorker")==1);drain();
        assert(field("identities")==2&&field("workers")==1&&field("calls")==1);
        const auto after=p1_m3g_values();
        assert(after.identity-baseline.identity==2&&after.identity-VMS_DIAG(g_p1_m3g_base).identity==2);
        assert(after.skin==baseline.skin&&after.quantize==baseline.quantize&&after.light==baseline.light&&
            after.gathered==baseline.gathered&&after.emitted==baseline.emitted&&after.texture==baseline.texture&&
            after.span==baseline.span&&after.row==baseline.row);
        epoch_report(different?"different_local":"same_local",b,h);
        std::cout<<"PASS G B="<<b<<" H="<<h<<" transition="<<(different?"different":"same")
            <<" previous_identity_delta="<<(different?2:3)<<" new_launch_delta=0 local_identity_delta=2 session="<<session
            <<" launch="<<VMS_DIAG(g_p1).launch_id<<" pending_canceled=1 worker_normal=1 production_M3G=Transform.setIdentity\n";
    }
    lifecycle_destroy();
}
static void starvation_bound(bool b,bool h){
    constexpr int64_t heartbeat_us=50000,budget_us=14000,tolerance_us=20000;
    launch("P1CloseoutChecks","p1-closeout-latency",b,h);
    assert(!VMS_EXEC(g_thread_live)&&!game_runnable()&&!VMS_RS(g_fb_dirty)&&!VMS_RS(g_repaint));
    control_us=1000000;const int64_t last_present=control_us;
    host_lifecycle_poll242();
    const int64_t started=paint_clock_us();assert(invoke("queueShort")==1);
    assert(p1_pending_work());
    // Enqueue after lifecycle polling, as an input callback can in Switch.
    // No copied loop: one real production work turn with pending serial and
    // heartbeat still false must execute immediately, with no sleep/acquire.
    assert(turn(control_us-last_present>=heartbeat_us,false)==p1::Work::Advance);
    const int64_t immediate_us=paint_clock_us()-started;
    assert(field("calls")==1&&field("checksum")==120&&sleeps==0&&schedules==0);
    assert(immediate_us<=budget_us+tolerance_us);
    host_lifecycle_poll242();assert(invoke("queueShort")==1);
    const int64_t measured_start=paint_clock_us(),control_start=control_us;
    // Controlled host I/O makes the next host turn available after the whole
    // heartbeat plus one allowed 14ms scheduler gap. This is an assumption
    // about host availability, not a claim of paint preemption or device time.
    control_us+=heartbeat_us+budget_us;
    const int old_sleeps=sleeps;
    assert(turn(control_us-last_present>=heartbeat_us,false)==p1::Work::Advance);
    const int64_t vm_io_us=paint_clock_us()-measured_start;
    const int64_t dispatch_us=control_us-control_start+vm_io_us;
    assert(field("calls")==2&&sleeps==old_sleeps&&vm_io_us<=tolerance_us);
    assert(dispatch_us<=heartbeat_us+budget_us+tolerance_us);
    VMS_RS(g_fb_dirty)=VMS_RS(g_repaint)=false;const int64_t idle_start=control_us;
    assert(turn(false)==p1::Work::Idle&&sleeps==old_sleeps+1&&control_us>idle_start);
    const int old_schedules=schedules;
    assert(turn(true)==p1::Work::CpuScheduled&&schedules==old_schedules+1&&acquires==1);
    assert(turn(false)==p1::Work::Idle&&sleeps==old_sleeps+2);
    std::cout<<"PASS D_bound B="<<b<<" H="<<h<<" heartbeat_us="<<heartbeat_us<<" scheduling_budget_us="<<budget_us
        <<" clock_host_io_tolerance_us="<<tolerance_us<<" immediate_dispatch_us="<<immediate_us<<" measured_vm_io_us="<<vm_io_us
        <<" controlled_plus_measured_dispatch_us="<<dispatch_us<<" limit_us="<<heartbeat_us+budget_us+tolerance_us
        <<" empty_sleep_us="<<slept_us<<" heartbeat_scheduled=1 following_empty_sleeps=1 scope=controlled_host_availability_plus_desktop_monotonic\n";
    lifecycle_destroy();
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    for(bool b:{false,true})for(bool h:{false,true}){endings(b,h);transitions(b,h);starvation_bound(b,h);}
    stop_thread();std::cout<<"PASS closeout configurations=4 F=24 G=8 D_bound=4 production_changes=0 device_passes=0\n";
}
