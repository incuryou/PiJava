// Production VM included unchanged. --wrap=stat injection is test-only.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include <unistd.h>
#include <iostream>

static bool interrupt_rms=false;
extern "C" int __real_stat(const char*,struct stat*);
extern "C" int __wrap_stat(const char* path,struct stat* out){
    // Only the uniquely named synthetic store, under the runner's temporary
    // J2ME_RMS_DIR. Exit before the real native stat, after production begin.
    if(interrupt_rms&&std::strstr(path,"r636974792d646961676e6f737469632d66697874757265.bin"))_exit(73);
    return __real_stat(path,out);
}
static int field(const char* name){
    auto* f=find_field(ensure_class("P1CityDiagnosticChecks"),name,"I");assert(f);
    return VMS_CL(g_classes)[f->owner].statics[f->slot].i;
}
static void launch(const char* identity=CityDiagnosticSuite){
    launch_class("P1CityDiagnosticChecks",identity);VMS_EXEC(g_speed_cap)=0;speed_sync_clock();
    assert(!VMS_EXEC(g_thrown)&&VMS_RS(g_current)&&VMS_EXEC(g_midlet_state)==MidletState::Active);
}
static void prime(){run(find_method(ensure_class("P1CityDiagnosticChecks"),"primeCaught","()V",false),{});assert(!VMS_EXEC(g_thrown)&&VMS_DIAG(g_p1).caught_count==1);}
static void saturated_logger(){
    p1_flush("test_setup");VMS_DIAG(g_p1).events=0;
    for(int i=0;i<512;++i){p1_event("fixture saturation index=%d",i);if(i%64==63)p1_flush("host_turn");}
    assert(VMS_DIAG(g_p1).events==512);VMS_PLAT(g_log).flush();
}
static bool budgets_empty(){
    const auto& d=VMS_DIAG(g_city_diag);
    return !d.key_active&&!d.trace&&!d.key_spans&&!d.key_dropped&&!d.rms_spans&&!d.rms_dropped&&
        !d.wait_spans&&!d.wait_dropped&&!d.host_spans&&!d.host_dropped&&!d.methods&&!d.method_dropped&&
        !d.sync_steps&&!d.sync_dropped&&!d.wait_steps&&!d.wait_step_dropped&&!d.phase_spans&&!d.phase_dropped&&
        !d.snapshots&&!d.snapshot_records&&!d.host_this_trace&&!d.yield_trace&&!d.key_started&&!d.recent_until&&
        !d.last_sync&&!d.last_wait&&!d.base_ops;
}
static void caps(){
    launch();Method* m=find_method(VMS_HEAP(g_heap)[VMS_RS(g_current)].cls,"keyPressed","(I)V",true);
    Frame frame;frame.method=m;frame.op_pc=7;frame.pc=8;
    for(int k=0;k<65;++k){
        CityKeySpan key(1,1,false,VMS_RS(g_current),m,0);
        for(int h=0;h<9;++h){CityHostSpan host("synthetic_cap");}
        key.end();
    }
    assert(VMS_DIAG(g_city_diag).key_spans==64&&VMS_DIAG(g_city_diag).key_dropped==1);
    assert(VMS_DIAG(g_city_diag).host_spans==512&&VMS_DIAG(g_city_diag).host_dropped==73);
    // Controlled diagnostic clock fields test eligibility/caps; actual VM
    // elapsed and checkpoint coverage are tested separately by key=3.
    VMS_DIAG(g_city_diag).key_active=true;VMS_DIAG(g_city_diag).key_started=paint_clock_us()-2000000;
    for(int i=0;i<18;++i){VMS_DIAG(g_city_diag).last_sync=0;city_sync_step(frame,1,1000);}
    assert(VMS_DIAG(g_city_diag).sync_steps==16&&VMS_DIAG(g_city_diag).sync_dropped==2);
    const auto sync=VMS_DIAG(g_city_diag).sync_steps,dropped=VMS_DIAG(g_city_diag).sync_dropped;
    city_sync_step(frame,1,1000);assert(sync==VMS_DIAG(g_city_diag).sync_steps&&dropped==VMS_DIAG(g_city_diag).sync_dropped);
    VMS_EXEC(g_can_suspend)=false;
    for(int w=0;w<65;++w){CityWaitSpan wait;wait.end("synthetic_return");}
    assert(VMS_DIAG(g_city_diag).wait_spans==64&&VMS_DIAG(g_city_diag).wait_dropped==1);
    // The production WAIT budget is independent from the progress budget.
    VMS_DIAG(g_city_diag).wait_spans=0;
    {CityWaitSpan wait;wait.began=paint_clock_us()-2000000;
        for(int i=0;i<18;++i){VMS_DIAG(g_city_diag).last_wait=0;wait.progress();}
        assert(VMS_DIAG(g_city_diag).wait_steps==16&&VMS_DIAG(g_city_diag).wait_step_dropped==2);
        auto before=VMS_DIAG(g_city_diag).wait_step_dropped;wait.progress();assert(before==VMS_DIAG(g_city_diag).wait_step_dropped);
        wait.end("synthetic_return");}
    for(int op=0;op<129;++op){CityRmsSpan rms("synthetic_budget",-1);
        if(!op)for(int p=0;p<65;++p){CityRmsPhase phase(rms,"synthetic_phase",8);phase.end(0,0,8);}}
    assert(VMS_DIAG(g_city_diag).rms_spans==128&&VMS_DIAG(g_city_diag).rms_dropped==1);
    assert(VMS_DIAG(g_city_diag).phase_spans==64&&VMS_DIAG(g_city_diag).phase_dropped==1);
    // Exact method allowlist/cap, synthetic metadata with no class loading.
    Class cls;cls.name="j";VMS_CL(g_classes).push_back(std::move(cls));
    Method marker;marker.class_id=int(VMS_CL(g_classes).size())-1;marker.name="g";marker.desc="()V";
    for(int i=0;i<129;++i)city_method_enter(&marker);
    assert(VMS_DIAG(g_city_diag).methods==128&&VMS_DIAG(g_city_diag).method_dropped==1);
    VMS_DIAG(g_city_diag).key_active=false;city_diag_report();
    const auto session=VMS_DIAG(g_p1).session_id,id=VMS_DIAG(g_p1).launch_id;
    launch();assert(VMS_DIAG(g_city_diag).enabled&&budgets_empty()&&VMS_DIAG(g_p1).session_id==session&&VMS_DIAG(g_p1).launch_id==id+1);
    dispatch_key(1,false);assert(VMS_DIAG(g_city_diag).trace==1&&VMS_DIAG(g_city_diag).key_spans==1);
    launch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    assert(!VMS_DIAG(g_city_diag).enabled&&budgets_empty());dispatch_key(1,false);assert(budgets_empty());
    unsetenv("J2ME_CITY_DIAG");city_diag_configure();launch();
    assert(!VMS_DIAG(g_city_diag).requested&&!VMS_DIAG(g_city_diag).enabled&&budgets_empty());dispatch_key(1,false);assert(budgets_empty());
    setenv("J2ME_CITY_DIAG","true",1);city_diag_configure();launch();assert(!VMS_DIAG(g_city_diag).enabled&&budgets_empty());
    setenv("J2ME_CITY_DIAG","1",1);city_diag_configure();launch();assert(VMS_DIAG(g_city_diag).enabled&&budgets_empty());
    assert(city_method(nullptr)=="unknown");assert(city_int("missing",-1,"a","I")=="unknown");
    assert(city_state(-1).find("selected_title_nonempty=unknown")!=std::string::npos);
    std::cout<<"PASS caps paired_key64 host512 wait64 rms128 phase64 method128 sync16 progress16 reset_same_non_target_marker_off unknown\n";
}
int main(int argc,char** argv){
    if(argc!=5)return 2;
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={argv[3],argv[4]};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    VMS_PLAT(g_log).open(argv[2],std::ios::trunc);assert(VMS_PLAT(g_log).is_open());
    setenv("J2ME_CITY_DIAG","1",1);city_diag_configure();launch();
    const std::string mode=argv[1];
    if(mode=="tail"){
        p1_flush("fixture_durable");log_line("FIXTURE durable_baseline");VMS_PLAT(g_log).flush();
        p1_event("fixture buffered_tail_marker");_exit(71);
    }
    if(mode=="keycrash"){
        prime();saturated_logger();dispatch_key(99,false);assert(false);
    }
    if(mode=="rmscrash"){
        interrupt_rms=true;dispatch_key(4,false);assert(false);
    }
    if(mode!="checks")return 2;
    prime();const auto dumps=VMS_DIAG(g_p1).caught_dumps;saturated_logger();
    dispatch_key(1,false);dispatch_key(2,false);dispatch_key(1,true);
    assert(field("returned")==2&&field("released")==1&&field("caught")==2);
    assert(VMS_DIAG(g_p1).caught_count==2&&VMS_DIAG(g_p1).caught_dumps==dumps&&VMS_DIAG(g_city_diag).snapshots==1&&VMS_DIAG(g_city_diag).snapshot_records==1);
    const auto begun=paint_clock_us();dispatch_key(3,false);const auto elapsed=paint_clock_us()-begun;
    assert(elapsed>=1000000&&elapsed<10000000&&VMS_DIAG(g_city_diag).sync_steps>=1&&VMS_DIAG(g_city_diag).sync_steps<=2);
    assert(VMS_EXEC(g_cuts)>0&&!VMS_DIAG(g_city_diag).key_active&&!VMS_EXEC(g_thrown)&&!VMS_EXEC(g_run_depth)&&!VMS_EXEC(g_pump_depth)&&!VMS_RS(g_paint_depth));
    std::cout<<"PASS actual_sync_handler elapsed_us="<<elapsed<<" sync="<<VMS_DIAG(g_city_diag).sync_steps<<" cuts="<<VMS_EXEC(g_cuts)<<" original_caught_dumps="<<dumps<<" snapshot=1 generic_events=512\n";
    caps();stop_thread();VMS_PLAT(g_log).flush();std::cout<<"PASS desktop_synthetic_only commercial_reproduction=0 device_passes=0\n";
}
