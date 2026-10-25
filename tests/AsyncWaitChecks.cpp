#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iostream>
static int value(const char* name) {
    const int c=ensure_class("AsyncWaitProbe");const auto* f=find_field(c,name,"I");
    return VMS_CL(g_classes)[c].statics[size_t(f->slot)].i;
}
static void call(const char* method) {run(find_method(ensure_class("AsyncWaitProbe"),method,"()V",false),{});}
static void observe(int duration) {
    const auto start=monotonic_ms();
    while(monotonic_ms()-start<duration) {
        speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}
static bool staged(int mode) {
    auto& t=VMS_EXEC(g_threads).front();
    // Complete an existing timeout, then let only Java execute the next
    // request/wait. No host paint is serviced until the assertions below.
    t.wake=monotonic_ms()-1;t.wake_us=paint_clock_us()-1;refresh_thread(t);
    VMS_EXEC(g_speed_frame_open)=false;VMS_RS(g_repaint)=false;
    const int paints=value("paints");const uint64_t draws=t.owned_draws;
    for(int n=0;n<5&&t.state==ThreadState::Runnable;++n)pump_one_thread();
    bool ok=t.state==ThreadState::Waiting&&t.wait_frame.awaiting_paint&&t.wake_us==0&&
        value("paints")==paints&&t.owned_draws==draws&&t.reacquire;
    const int64_t original=t.wake;
    if(mode==19) {
        const uint64_t request=t.wait_frame.request;
        VMS_EXEC(g_speed_cap)=0;speed_sync_clock();
        VMS_EXEC(g_speed_cap)=60;speed_sync_clock();
        ok=ok&&!t.wait_frame.active&&!t.wait_frame.awaiting_paint&&t.wait_frame.request==request;
        std::cout<<"stage=19 request="<<request<<" reset_request="<<t.wait_frame.request<<" "<<(ok?"PASS":"FAIL")<<"\n";
        return ok;
    }
    if(mode==10)call("request"); // new request belongs to the main thread
    if(mode==11){t.wake=monotonic_ms()-1;refresh_thread(t);ok=ok&&!t.wait_frame.active&&VMS_EXEC(g_exec_ctx).frame_wait_request_owner==0;}
    if(mode==12||mode==13)run(find_method(ensure_class("AsyncWaitProbe"),"signal","(I)V",false),{Value::of_i(mode==13?1:0)});
    if(mode==14){VMS_EXEC(g_speed_cap)=30;speed_sync_clock();}
    if(mode==15)call("replace");
    if(mode==17)call("changePeriod");
    if(VMS_RS(g_repaint)){VMS_RS(g_repaint)=false;repaint_current();}
    if(mode==9)ok=ok&&t.wait_frame.paced&&t.wake_us>0&&t.wake<original&&
        t.owned_draws==draws+1&&t.state==ThreadState::Waiting&&t.monitor==VMS_RS(g_current);
    else ok=ok&&!t.wait_frame.paced&&t.wake_us==0;
    std::cout<<"stage="<<mode<<" before_paints="<<paints<<" after_paints="<<value("paints")
        <<" paced="<<t.wait_frame.paced<<" wake_us="<<t.wake_us<<" "<<(ok?"PASS":"FAIL")<<"\n";
    return ok;
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const int mode=std::atoi(argv[1]),cap=std::atoi(argv[2]);
    VMS_CL(g_classpath)={argv[3],"romfs/classes"};VMS_EXEC(g_speed_cap)=cap;
    VMS_EXEC(g_slice_ms)=4;VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    run(find_method(ensure_class("AsyncWaitProbe"),"start","(I)V",false),{Value::of_i(mode)});
    observe(700);bool semantic=true;
    if((mode>=9&&mode<=17)||mode==19) {const bool ok=staged(mode);stop_thread();return ok?0:1;}
    if(mode>=1&&mode<=3) {
        const int exceptions=value("interrupted");
        run(find_method(ensure_class("AsyncWaitProbe"),"signal","(I)V",false),{Value::of_i(mode==2?1:mode==3?2:0)});
        observe(100);semantic=value("lockErrors")==0&&value("interrupted")==exceptions+(mode==2?1:0);
    }
    if(mode==6){VMS_EXEC(g_speed_cap)=0;speed_sync_clock();observe(100);}
    if(mode==7){VMS_EXEC(g_speed_cap)=0;speed_sync_clock();observe(100);VMS_EXEC(g_speed_cap)=cap;speed_sync_clock();observe(200);}
    if(mode==8){call("replace");observe(100);}
    const int before=value("updates"),timer_before=value("ticks");const auto began=monotonic_ms();observe(1100);
    const double fps=1000.0*(value("updates")-before)/(monotonic_ms()-began);
    const double timerfps=1000.0*(value("ticks")-timer_before)/(monotonic_ms()-began);
    const double target=(!cap||mode==4||mode==6||mode==8||mode==18)?20:cap;
    const bool ok=semantic&&fps>target*.84&&fps<target*1.14&&VMS_EXEC(g_game_clock).scale==1&&(mode!=5||(timerfps>16&&timerfps<24));
    std::cout<<"mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" timerfps="<<timerfps<<" paints="<<value("paints")<<" lockErrors="<<value("lockErrors")<<" interrupted="<<value("interrupted")<<" scale="<<VMS_EXEC(g_game_clock).scale<<" "<<(ok?"PASS":"FAIL")<<"\n";
    stop_thread();return ok?0:1;
}
