#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <iostream>
static int value(const char* name) {
    const int c=ensure_class("WaitFrameProbe");const auto* f=find_field(c,name,"I");
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
        for(const auto& t:VMS_EXEC(g_threads))if(t.state==ThreadState::Waiting&&t.wake_us>0)return true;
        observe(1);
    }
    return false;
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const int mode=std::atoi(argv[1]),cap=std::atoi(argv[2]);
    VMS_CL(g_classpath)={argv[3],"romfs/classes"};
    VMS_EXEC(g_speed_cap)=mode==13?0:cap;VMS_EXEC(g_slice_ms)=4;VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    if(mode==2||mode==3) {
        auto* configure=find_method(ensure_class("WaitFrameProbe"),"configureWork","(I)V",false);
        int iterations=10000;int64_t cost=0;
        do {const auto began=paint_clock_us();run(configure,{Value::of_i(iterations)});cost=paint_clock_us()-began;
            if(cost<4000)iterations*=2;}while(cost<4000&&iterations<1000000);
        const int desired=mode==2?5000:10000;
        iterations=std::max(1,int(int64_t(iterations)*desired/std::max<int64_t>(1,cost)));
        run(configure,{Value::of_i(iterations)});
    }
    run(find_method(ensure_class("WaitFrameProbe"),"start","(I)V",false),{Value::of_i(mode)});
    observe(700);bool semantic_ok=true;
    if(mode==8||mode==9||mode==10) {
        semantic_ok=await_waiter();
        const int exceptions=value("interrupted");
        run(find_method(ensure_class("WaitFrameProbe"),"signal","(I)V",false),{Value::of_i(mode==9?1:mode==10?2:0)});
        observe(100);
        semantic_ok=semantic_ok&&value("lockErrors")==0&&value("interrupted")==exceptions+(mode==9?1:0);
    }
    if(mode==12||mode==13) {VMS_EXEC(g_speed_cap)=mode==12?0:cap;speed_sync_clock();observe(200);}
    const int before=value("updates"),before_returned=value("returned");const auto start=monotonic_ms();
    observe(1100);const auto elapsed=monotonic_ms()-start;
    const double fps=1000.0*(value("updates")-before)/elapsed;
    bool ok=semantic_ok&&VMS_EXEC(g_game_clock).scale==1;
    if((mode<=3||(mode>=8&&mode<=10)||mode==13)&&cap)ok=ok&&fps>cap*.88&&fps<cap*1.12;
    else if(mode==5)ok=ok&&fps==0&&value("returned")-before_returned>=20&&value("returned")-before_returned<=25;
    else if(mode==7)ok=ok&&fps==0&&value("returned")==0;
    else {const double native=1000.0/(mode==1?63:mode==14?94:47);ok=ok&&fps>native*.84&&fps<native*1.12;}
    std::cout<<"mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" returned="<<value("returned")
             <<" scale="<<VMS_EXEC(g_game_clock).scale<<" lock_errors="<<value("lockErrors")
             <<" interrupted="<<value("interrupted")<<" "<<(ok?"PASS":"FAIL")<<"\n";
    stop_thread();if(!ok){std::cerr<<"FAIL WaitFrameChecks frame remainder wait pace or wait isolation\n";return 1;}return 0;
}
