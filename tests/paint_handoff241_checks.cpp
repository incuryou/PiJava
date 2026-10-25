#include <cassert>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static int fixture;
static int value(const char* name){auto* f=find_field(fixture,name,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static void setup(bool enabled,int mode=0,int cap=0){
    stop_thread();VMS_EXEC(g_completed_paint241).enabled=enabled;VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_midlet_state)=MidletState::Active;
    VMS_PLAT(g_host_paused)=false;VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);speed_sync_clock();
    assert(run(find_method(fixture,"setup","(I)I",false),{Value::of_i(mode)}).i==1);
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    fixture=ensure_class("PaintHandoff241Checks");
    setup(false);advance_game_frame();const int old_paints=value("paints"),old_calls=value("calls");assert(old_paints>1);
    setup(true);advance_game_frame();const int new_paints=value("paints");assert(new_paints==1&&value("calls")==0&&VMS_RS(g_repaint));
    assert(VMS_EXEC(g_completed_paint241).serial_owed&&!VMS_EXEC(g_serial_calls).empty());
    for(int i=0;i<8&&value("calls")<4;++i)advance_game_frame();assert(value("calls")==4&&VMS_EXEC(g_serial_calls).empty());
    std::cout<<"PASS event_handoff legacy_first_turn_paints="<<old_paints<<" legacy_calls="<<old_calls<<" new_first_turn_paints="<<new_paints<<" callbacks_retained=4\n";
    setup(true);assert(run(find_method(fixture,"startWorker","()I",false),{}).i==1);
    for(int i=0;i<24&&(value("afterService")<6||value("calls")<4||VMS_EXEC(g_thread_live));++i){
        advance_game_frame();assert(VMS_RS(g_paint_depth)==0&&VMS_EXEC(g_run_depth)==0&&!VMS_EXEC(g_thrown));
    }
    assert(value("workers")==6&&value("afterService")==6&&value("calls")==4&&!VMS_EXEC(g_thread_live));
    assert(VMS_EXEC(g_completed_paint241).worker_priorities>0);
    std::cout<<"PASS worker_fairness completed_services="<<value("afterService")<<" callbacks="<<value("calls")<<" priority_turns="<<VMS_EXEC(g_completed_paint241).worker_priorities<<"\n";
    setup(true,1);for(int i=0;i<12&&value("calls")<4;++i)advance_game_frame();assert(value("syncPairs")==4&&value("inside")==0);
    std::cout<<"PASS synchronous_service callback_pairs=4 both_paints_complete=1\n";
    for(int cap:{30,60}){setup(true,0,cap);advance_game_frame();assert(!VMS_EXEC(g_completed_paint241).active&&VMS_EXEC(g_completed_paint241).turns==0);}
    stop_thread();assert(!VMS_EXEC(g_completed_paint241).active&&!VMS_EXEC(g_completed_paint241).serial_owed&&!VMS_EXEC(g_completed_paint241).worker_owed);
    std::cout<<"PASS cap_exclusion=2 lifecycle_reset=1 scopes_balanced=1\n";
}
