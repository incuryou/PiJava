#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iostream>
static int64_t field(const char* name,const char* desc="I") {
    const int cls=ensure_class("RecheckedDeadlineProbe");
    const auto v=VMS_CL(g_classes)[cls].statics[find_field(cls,name,desc)->slot];
    return *desc=='J'?as_long(v):v.i;
}
static int active_seen=0,interrupt_active=0,interrupt_waits=0;
static void observe(int duration) {
    const auto begin=monotonic_ms();
    while(monotonic_ms()-begin<duration) {
        speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
#ifndef J2ME_TEST_BASELINE
        for(const auto& t:VMS_EXEC(g_threads)) {
            if(t.rechecked_deadline.active)++active_seen;
            if(field("interrupted")&&t.state==ThreadState::Sleeping&&!t.frames.empty()&&
               t.frames.back().method->name=="run"&&t.frames.back().method->class_id==ensure_class("RecheckedDeadlineProbe")&&
               t.frames.back().op_pc!=t.rechecked_deadline.pattern.sleep_pc) {
                ++interrupt_waits;if(t.rechecked_deadline.active)++interrupt_active;
            }
        }
#endif
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const int mode=std::atoi(argv[1]),cap=std::atoi(argv[2]);
    VMS_CL(g_classpath)={argv[3],"romfs/classes"};VMS_EXEC(g_speed_cap)=cap;
    VMS_DIAG(g_log_draw)=false;VMS_RS(g_fb).assign(240*320*3,0);VMS_EXEC(g_midlet_state)=MidletState::Active;
    const int fixture=ensure_class("RecheckedDeadlineProbe");auto* loop=find_method(fixture,"run","()V",false);
    JitRegionMethod region;jit_region_scan(loop,region);
    run(find_method(fixture,"start","(I)V",false),{Value::of_i(mode)});
    observe(mode==5?2100:650);
    if(mode==6){VMS_EXEC(g_speed_cap)=0;speed_sync_clock();observe(200);}
    const int before=field("updates");const auto begin=monotonic_ms(),clock=game_current_ms();observe(1400);
    bool gate_banned=true;
#ifndef J2ME_TEST_BASELINE
    for(const auto& gate:loop->rechecked_deadlines)
        gate_banned=gate_banned&&region.banned[gate.clock_pc]&&region.banned[gate.branch_pc]&&region.banned[gate.sleep_pc];
#endif
    const double fps=double(field("updates")-before)*1000/(monotonic_ms()-begin);
    const auto clock_delta=game_current_ms()-clock,wall_delta=monotonic_ms()-begin;
    bool ok=gate_banned&&VMS_EXEC(g_game_clock).scale==1&&std::llabs(clock_delta-wall_delta)<=20;
    ok=ok&&field("backgroundDone")&&field("backgroundElapsed","J")>=(mode==8?70:160)&&field("backgroundElapsed","J")<(mode==8?200:400);
    if(mode==0||mode==5||mode==8||mode==9||mode==10)ok=ok&&fps>(cap?cap*.85:22)&&fps<(cap?cap*1.12:28);
    else if(mode==6)ok=ok&&fps>22&&fps<28;
    else ok=ok&&fps>0&&fps<28&&active_seen==0;
    if(mode==5)ok=ok&&field("longDone")&&field("longElapsed","J")>=990&&field("longElapsed","J")<1100;
    if(mode==10)ok=ok&&field("interrupted")>0&&field("interruptedAt")==12;
    if(mode==9)ok=ok&&field("interrupted")>0&&interrupt_waits>0&&interrupt_active==0;
    std::cout<<"mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" scale="<<VMS_EXEC(g_game_clock).scale
      <<" clock_delta="<<clock_delta<<" wall_delta="<<wall_delta<<" background_ms="<<field("backgroundElapsed","J")
      <<" interrupted_at="<<field("interruptedAt")<<" interrupt_waits="<<interrupt_waits<<" interrupt_active="<<interrupt_active
      <<" active_seen="<<active_seen<<" load_ms="<<field("longElapsed","J")<<" "<<(ok?"PASS":"FAIL")<<"\n";
    stop_thread();return ok?0:1;
}
