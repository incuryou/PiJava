#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iostream>
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&){return "/tmp/compensated-sleep-unused.j2s";}
#include "../src/instant_state.inc"
static int64_t field(const char* name,const char* desc="I") {
    const int cls=ensure_class("CompensatedSleepProbe");
    const auto v=VMS_CL(g_classes)[cls].statics[find_field(cls,name,desc)->slot];
    return *desc=='J'?as_long(v):v.i;
}
static int long_requests=0,negative_errors=0;
static bool jit_banned=true,reset_ok=true,snapshot_ok=true,jit_admission=true;
static int64_t max_request=0,min_error=0;
static void inspect() {
    const int cls=ensure_class("CompensatedSleepProbe");
    for(const auto& t:VMS_EXEC(g_threads)) {
        if(t.state!=ThreadState::Sleeping||t.frames.empty())continue;
        const auto& fr=t.frames.back();
        if(fr.method->name!="run"||fr.locals.empty()||!fr.locals[0].ref||!type_compatible(fr.locals[0].ref,"CompensatedSleepProbe")||fr.locals.size()<11)continue;
        const auto request=as_long(fr.locals[9]),error=as_long(fr.locals[5]);
#ifndef J2ME_TEST_BASELINE
        JitRegionMethod region;jit_region_scan(fr.method,region);
        for(const auto& p:fr.method->compensated_sleep_patterns)
            jit_banned=jit_banned&&region.banned[p.sleep]&&region.banned[p.subtract]&&region.banned[p.after_clock];
        if(field("mode")==4||field("mode")==6)jit_banned=jit_banned&&fr.method->compensated_sleep_patterns.empty();
#endif
        max_request=std::max(max_request,request);min_error=std::min(min_error,error);
        if(request>120)++long_requests;
        if(error<0)++negative_errors;
    }
}
static void check_jit_admission() {
#ifndef J2ME_TEST_BASELINE
    auto* loop=find_method(ensure_class("CompensatedSleepProbe"),"run","()V",false);
    if(loop->compensated_sleep_patterns.empty())return;
    auto& engine=VMS_NATIVE(g_native_jit);
    jit_admission=engine.configure(true)&&engine.available;
    if(!jit_admission)return;
    jit_region_reset();
    for(const auto& p:loop->compensated_sleep_patterns) {
        for(int pc:{p.before_clock,p.sleep,p.after_clock,p.subtract,p.anchor_clock}) {
            Frame probe;probe.method=loop;probe.pc=pc;probe.locals.resize(loop->max_locals);
            const auto skipped=g_jit_region_stats.skip_marked,compiled=g_jit_region_stats.compiled;
            jit_admission=jit_admission&&jit_region_try(probe,1024)==0&&
                g_jit_region_stats.skip_marked==skipped+1&&g_jit_region_stats.compiled==compiled;
        }
    }
#endif
}
static void observe(int duration) {
    const auto begin=monotonic_ms();
    while(monotonic_ms()-begin<duration) {
        const auto started=monotonic_ms();
        sched_game_work(false,[]{},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
        inspect();
        sched_present_turn(true,false,false,3,[&]{
            const auto left=16-(monotonic_ms()-started);
            if(left>0)std::this_thread::sleep_for(std::chrono::milliseconds(left));
            VMS_RS(g_fb_dirty)=false;
        });
    }
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const int mode=std::atoi(argv[1]),cap=std::atoi(argv[2]);
    VMS_CL(g_classpath)={argv[3],"romfs/classes"};VMS_EXEC(g_speed_cap)=cap;
    VMS_DIAG(g_log_draw)=false;VMS_RS(g_fb).assign(240*320*3,0);VMS_EXEC(g_midlet_state)=MidletState::Active;
    if(mode==10)launch_class("CompensatedSleepProbe$App","fixture");
    else run(find_method(ensure_class("CompensatedSleepProbe"),"start","(I)V",false),{Value::of_i(mode)});
    observe(750);
    if(mode==8) {
        speed_reset_learning();
#ifndef J2ME_TEST_BASELINE
        for(const auto& t:VMS_EXEC(g_threads))reset_ok=reset_ok&&!t.compensated_sleep.pending&&!t.compensated_sleep.anchor_seen&&!t.compensated_sleep.method;
#endif
        observe(250);
    }
    if(mode==9){VMS_EXEC(g_speed_cap)=0;speed_sync_clock();observe(250);}
    if(mode==10) {
        SavState state,decoded;std::string bytes;
        snapshot_ok=capture_state(state,"fixture.jar")&&encode_state(state,bytes)&&decode_state(bytes,decoded)&&apply_state(decoded);
#ifndef J2ME_TEST_BASELINE
        for(const auto& t:VMS_EXEC(g_threads))snapshot_ok=snapshot_ok&&!t.compensated_sleep.method&&!t.compensated_sleep.pending&&!t.compensated_sleep.anchor_seen;
#endif
        observe(250);
    }
    const int before=field("updates");
    const auto begin=monotonic_ms(),clock=game_current_ms();observe(1700);
    const auto wall=monotonic_ms()-begin,delta=game_current_ms()-clock;
    const double fps=double(field("updates")-before)*1000/wall;
    const bool paced=cap&&mode!=2&&mode!=3&&mode!=4&&mode!=6&&mode!=9;
    const double expected=paced?cap:1000.0/(mode==5?53:37);
    check_jit_admission();
    const bool ok=jit_admission&&snapshot_ok&&jit_banned&&reset_ok&&VMS_EXEC(g_game_clock).scale==1&&std::llabs(delta-wall)<=20&&
        field("backgroundDone")&&field("backgroundElapsed","J")>=165&&field("backgroundElapsed","J")<350&&
        fps>expected*.88&&fps<expected*1.1&&(!paced||max_request<=120);
    std::cout<<"mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" scale="<<VMS_EXEC(g_game_clock).scale
        <<" wall="<<wall<<" game="<<delta<<" background="<<field("backgroundElapsed","J")
        <<" max_request="<<max_request<<" min_error="<<min_error<<" requests_over120="<<long_requests
        <<" negative_errors="<<negative_errors<<" jit_banned="<<jit_banned<<" reset="<<reset_ok<<" snapshot="<<snapshot_ok<<" jit_admission="<<jit_admission<<" "<<(ok?"PASS":"FAIL")<<"\n";
    stop_thread();return ok?0:1;
}
