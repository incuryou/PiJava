#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iostream>
static int updates() {
    const int cls=ensure_class("ResidualSleepProbe");
    return VMS_CL(g_classes)[cls].statics[find_field(cls,"updates","I")->slot].i;
}
static int short_waits=0,retimed_waits=0,spent_credits=0,last_update=-1;
static void observe_timer_helper() {
    const int cls=ensure_class("ResidualSleepProbe");
    const int64_t requested=as_long(VMS_CL(g_classes)[cls].statics[find_field(cls,"requested","J")->slot]);
    const int count=updates();
    if(count==last_update||requested<1||requested>7)return;
    for(const auto& t:VMS_EXEC(g_threads)) {
        if(t.state!=ThreadState::Sleeping||t.frames.empty()||t.frames.back().method->name!="tick")continue;
        last_update=count;++short_waits;
        if(t.wake_us)++retimed_waits;
        if(speed_period_us()&&!t.frame_due_us)++spent_credits;
    }
}
static void observe_helper(int duration) {
    const auto begin=monotonic_ms();
    while(monotonic_ms()-begin<duration) {
        speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
        observe_timer_helper();
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}
static void observe(int duration) {
    const auto begin=monotonic_ms();
    while(monotonic_ms()-begin<duration) {
        const auto started=monotonic_ms();
        sched_game_work(false,[]{},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
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
    VMS_DIAG(g_log_draw)=false;VMS_RS(g_fb).assign(240*320*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    if(mode==4) {
        auto* configure=find_method(ensure_class("ResidualSleepProbe"),"configureWork","(I)V",false);
        int iterations=10000;int64_t cost=0;
        run(configure,{Value::of_i(iterations)}); // exclude class/quick-site warmup
        do {cost=INT64_MAX;
            for(int sample=0;sample<3;++sample) {
                const auto began=paint_clock_us();run(configure,{Value::of_i(iterations)});
                cost=std::min(cost,paint_clock_us()-began);
            }
            if(cost<4000)iterations*=2;}while(cost<4000&&iterations<1000000);
        iterations=std::max(1,int(int64_t(iterations)*8000/std::max<int64_t>(1,cost)));
        run(configure,{Value::of_i(iterations)});
    }
    run(find_method(ensure_class("ResidualSleepProbe"),"start","(I)V",false),{Value::of_i(mode)});
    if(mode==4) {
        observe_helper(1800);
        // Existing Timer/poll evidence may learn a clock rate in speed mode.
        // This contract is that this short Java sleep keeps its real deadline.
        const bool ok=short_waits>=5&&retimed_waits==0&&spent_credits==0&&(!cap?VMS_EXEC(g_game_clock).scale==1:true);
        std::cout<<"timer_helper cap="<<cap<<" short_waits="<<short_waits<<" retimed_waits="<<retimed_waits
                 <<" spent_credits="<<spent_credits<<" scale="<<VMS_EXEC(g_game_clock).scale<<" "<<(ok?"PASS":"FAIL")<<"\n";
        stop_thread();return ok?0:1;
    }
    observe(900);const int before=updates();const auto start=monotonic_ms();observe(1700);
    const double fps=double(updates()-before)*1000/(monotonic_ms()-start);
    bool ok=VMS_EXEC(g_game_clock).scale==1;
    if(mode==2)ok=ok&&fps>40&&fps<50;
    else if(mode==1)ok=ok&&fps>35&&fps<52;
    else if(cap)ok=ok&&fps>cap*.88&&fps<cap*1.1;
    else {const double expected=1000.0/(mode==3?31:22);ok=ok&&fps>expected*.88&&fps<expected*1.1;}
    std::cout<<"mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" scale="<<VMS_EXEC(g_game_clock).scale<<" "<<(ok?"PASS":"FAIL")<<"\n";
    stop_thread();return ok?0:1;
}
