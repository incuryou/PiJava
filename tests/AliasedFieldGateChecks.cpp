#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <cassert>
static void canvas_switch_checks(const char* classes) {
    VMS_CL(g_classpath)={classes,"romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    for(int kind:{0,1,2,3}) {
        stop_thread();VMS_EXEC(g_speed_cap)=kind==0?0:60;speed_sync_clock();
        VMS_EXEC(g_midlet_state)=MidletState::Active;
        const int cls=ensure_class("AliasedFieldGateProbe");
        run(find_method(cls,"start","(I)V",false),{Value::of_i(1)});
        assert(!VMS_EXEC(g_threads).empty());auto& owner=VMS_EXEC(g_threads).back();
        owner.auto_fps.started=true;owner.auto_fps.stable=7;owner.auto_fps.candidate=41000;
        owner.auto_fps.verified_gate_seen=kind==3;VMS_EXEC(g_speed_budget_owner)=&owner;
        if(kind==2){owner.timer.stable=5;g_timer_clock_owner=&owner;}
        const double rate=kind==0?1.0:2.46;VMS_EXEC(g_game_clock).rate(paint_clock_us(),rate);
        run(find_method(cls,"changeCanvas","()V",false),{});
        if(kind==3)assert(!owner.auto_fps.started&&VMS_EXEC(g_game_clock).scale==1.0);
        else assert(owner.auto_fps.started&&owner.auto_fps.stable==7&&VMS_EXEC(g_game_clock).scale==rate);
        if(kind==2)assert(g_timer_clock_owner==&owner&&owner.timer.stable==5);
    }
    stop_thread();std::cout<<"PASS Canvas transition preserves Native, legacy and Timer; releases certified gate\n";
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    const int mode=std::atoi(argv[1]),cap=std::atoi(argv[2]);
    canvas_switch_checks(argv[3]);
    VMS_CL(g_classpath)={argv[3],"romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_slice_ms)=4;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    const int cls=ensure_class("AliasedFieldGateProbe");
    run(find_method(cls,"start","(I)V",false),{Value::of_i(mode)});
    auto field=[&](const char* name){return VMS_CL(g_classes)[cls].statics[find_field(cls,name,"I")->slot].i;};
    auto observe=[&](int ms){
        const auto begin=monotonic_ms();const int before=field("updates");
        while(monotonic_ms()-begin<ms) {
            const int64_t turn=monotonic_ms();
            sched_game_work(false,[]{},[](int64_t wait){std::this_thread::sleep_for(std::chrono::milliseconds(wait));});
            sched_present_turn(true,false,false,3,[&]{
                const int64_t left=16-(monotonic_ms()-turn);
                if(left>0)std::this_thread::sleep_for(std::chrono::milliseconds(left));
                VMS_RS(g_fb_dirty)=false;
            });
        }
        return double(field("updates")-before)*1000.0/(monotonic_ms()-begin);
    };
    observe(2200);const double fps=observe(1100);const double scale=VMS_EXEC(g_game_clock).scale;
    std::cout<<"aliased_field mode="<<mode<<" cap="<<cap<<" update_fps="<<fps<<" scale="<<scale<<" gate_reads="<<VMS_EXEC(g_poll_gate_reads)<<" paints="<<field("paints")<<"\n"<<std::flush;
    if(mode==0||mode==2||mode==4||mode==7) {
        assert(cap?fps>cap*0.88&&fps<cap*1.1:fps>22&&fps<30);
        if(cap)assert(VMS_EXEC(g_poll_gate_reads)>0&&scale>(cap==30?1.05:1.5));
    } else {assert(scale==1.0);assert(fps<31);}
    if(mode==6||mode==8||mode==9) {
        const char* name=mode==6?"volatileLoop":mode==8?"extraInheritedLoop":"unloadedAliasLoop";
        assert(find_method(cls,name,"()V",false)->time_gates.empty());
    }
    if(mode==9) {
        assert(VMS_CL(g_class_index).find("AliasedFieldGateProbe$UnloadedAlias")==VMS_CL(g_class_index).end());
        assert(field("foreignUpdates")==0);
    }
    if(mode==2)assert(field("phase")==2);
    if(mode==0&&cap) {
        run(find_method(cls,"changeCanvas","()V",false),{});
        observe(300);assert(VMS_EXEC(g_game_clock).scale==1.0);
    }
    stop_thread();std::cout<<"PASS aliased field clock gate\n";
}
