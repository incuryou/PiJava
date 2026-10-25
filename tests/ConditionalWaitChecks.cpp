#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <iostream>
static int value(const char* name) {
    const int c=ensure_class("ConditionalWaitProbe");const auto* f=find_field(c,name,"I");
    return VMS_CL(g_classes)[c].statics[size_t(f->slot)].i;
}
static void observe(int duration) {
    const auto start=monotonic_ms();
    while(monotonic_ms()-start<duration) {
        speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}
static bool await_waiter() {
    const auto start=monotonic_ms();
    while(monotonic_ms()-start<250) {
        for(const auto& t:VMS_EXEC(g_threads))if(t.state==ThreadState::Waiting&&t.wake>0)return true;
        observe(1);
    }
    return false;
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    // 0: branch/statistics/local period; 1: every-other-tick paint;
    // 2: no paint; 3: business predicate; 4: side-effect period;
    // 5: statistics start after paint; 6/7/8: notify/interrupt/notifyAll;
    // 9: same-owner outer fallback plus inner early-continue candidate.
    const int mode=std::atoi(argv[1]),cap=std::atoi(argv[2]);
    VMS_CL(g_classpath)={argv[3],"romfs/classes"};
    VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_slice_ms)=4;VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    run(find_method(ensure_class("ConditionalWaitProbe"),"start","(I)V",false),{Value::of_i(mode)});
    observe(mode==9?3000:700);bool semantic_ok=true;
    if(mode>=6&&mode<=8) {
        semantic_ok=await_waiter();
        const int exceptions=value("interrupted");
        run(find_method(ensure_class("ConditionalWaitProbe"),"signal","(I)V",false),{Value::of_i(mode==7?1:mode==8?2:0)});
        observe(100);
        semantic_ok=semantic_ok&&value("lockErrors")==0&&value("interrupted")==exceptions+(mode==7?1:0);
    }
    const int before=value("ticks"),before_paints=value("paints");const auto start=monotonic_ms();
    observe(1100);const auto elapsed=monotonic_ms()-start;
    const double fps=1000.0*(value("ticks")-before)/elapsed;
    const double paints=1000.0*(value("paints")-before_paints)/elapsed;
    bool ok=semantic_ok&&(mode==9?(!cap||VMS_EXEC(g_game_clock).scale>1.01):VMS_EXEC(g_game_clock).scale==1);
    if(mode==9) {
        for(const auto& t:VMS_EXEC(g_threads))ok=ok&&!t.auto_fps.verified_gate_seen;
    }
    if(mode==9)ok=ok&&value("stats")>=value("ticks")*2-2&&value("stats")<=value("ticks")*2&&
        value("paints")>=value("ticks")&&value("paints")<=value("ticks")+1;
    if((mode==0||mode>=6)&&cap)ok=ok&&fps>cap*.88&&fps<cap*1.12;
    else if(mode==1&&cap) {
        const double expected=2000.0/(53+1000.0/cap);
        ok=ok&&fps>expected*.88&&fps<expected*1.12&&paints<fps*.58&&paints>fps*.42;
    } else if(mode==4)ok=ok&&fps>=10&&fps<19;
    else {const double native_period=mode==9?67:53;ok=ok&&fps>(1000.0/native_period)*.84&&fps<(1000.0/native_period)*1.12;}
    std::cout<<"mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" paint_fps="<<paints<<" stats="<<value("stats")
             <<" ticks="<<value("ticks")<<" paints="<<value("paints")<<" scale="<<VMS_EXEC(g_game_clock).scale<<" lock_errors="<<value("lockErrors")
             <<" interrupted="<<value("interrupted")<<" "<<(ok?"PASS":"FAIL")<<"\n";
    stop_thread();if(!ok){std::cerr<<"FAIL ConditionalWaitChecks frame remainder wait pace or wait isolation\n";return 1;}return 0;
}
