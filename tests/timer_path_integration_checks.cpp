#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <iostream>
static int value(const char* cls,const char* name) {
    const int c=ensure_class(cls); const Field* f=find_field(c,name,"I");
    return VMS_CL(g_classes)[c].statics[size_t(f->slot)].i;
}
static void observe(int duration) {
    const int64_t start=monotonic_ms();
    while(monotonic_ms()-start<duration) {
        speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    const char* type=argv[1];const int mode=std::atoi(argv[2]),cap=std::atoi(argv[3]);
    VMS_CL(g_classpath)={argv[4],"romfs/classes"};
    VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_slice_ms)=4;VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    const int cls=ensure_class(type);
    run(find_method(cls,"start","(I)V",false),{Value::of_i(mode)});
    observe(3000);
    const auto start=monotonic_ms();const int updates=value(type,"updates"),paints=value(type,"paints");
    const auto budget=VMS_EXEC(g_speed_budget_frames);
    observe(1800);const auto elapsed=monotonic_ms()-start;
    std::cout<<"class="<<type<<" mode="<<mode<<" cap="<<cap
             <<" updates_fps="<<1000.0*(value(type,"updates")-updates)/elapsed
             <<" paints_fps="<<1000.0*(value(type,"paints")-paints)/elapsed
             <<" budget_fps="<<1000.0*(VMS_EXEC(g_speed_budget_frames)-budget)/elapsed
             <<" scale="<<VMS_EXEC(g_game_clock).scale;
#ifdef HAS_TIMER_PACING
    std::cout<<" timer_paced="<<g_frame_sleep_stats.timer_paced;
#endif
    std::cout<<"\n"; const double fps=1000.0*(value(type,"updates")-updates)/elapsed; const bool is_timer=std::string(type)=="TimerPathProbe";
    const bool negative=is_timer&&(mode==8||mode==9);
    bool ok=fps>cap*0.88 && fps<cap*1.10;
    if(cap==0||negative){
        const bool slow=(is_timer&&(mode==4||mode==9))||!is_timer;
        ok=VMS_EXEC(g_game_clock).scale==1 && fps>(slow?10:21) && fps<(slow?18:28);
    }
    if(is_timer&&mode==6&&cap)ok=ok && 1000.0*(value(type,"paints")-paints)/elapsed>fps*1.8;
    if(is_timer&&mode==8)ok=ok && value(type,"paints")==paints;
    stop_thread(); if(!ok){std::cerr<<"FAIL actual updates did not reach selected pace\n";return 1;} return 0;
}
