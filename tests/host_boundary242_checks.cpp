#include <cassert>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static int fixture;
static int value(const char* n){auto* f=find_field(fixture,n,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static void setup(bool boundary,int mode=0){
    stop_thread();VMS_RS(g_host_boundary242_enabled)=boundary;VMS_EXEC(g_completed_paint241).enabled=true;VMS_EXEC(g_speed_cap)=0;VMS_EXEC(g_midlet_state)=MidletState::Active;
    VMS_PLAT(g_host_paused)=false;VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);speed_sync_clock();
    assert(run(find_method(fixture,"setup","(I)I",false),{Value::of_i(mode)}).i==1);VMS_RS(g_fb_dirty)=false;
}
// This sequence is shared with the real Switch loop: lifecycle poll BEFORE
// advance_game_frame, plus the dirty-frame and post-present work guards.
static void host_loop(){host_lifecycle_poll242();if(!host_frame_ready242())advance_game_frame();}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();fixture=ensure_class("PaintHandoff241Checks");
    setup(false);host_loop();int old=value("paints");assert(old>=2);
    setup(true);host_loop();assert(value("paints")==1&&VMS_RS(g_fb_dirty));assert(!host_speculation_allowed242());
    int kept=value("paints");host_loop();assert(value("paints")==kept&&host_frame_ready242());
    std::cout<<"PASS real_host_sequence old_paints_before_present="<<old<<" new_paints_before_present="<<kept<<" dirty_frame_preserved=1 heavy_speculation_suppressed=1\n";
    for(int i=0;i<16&&value("calls")<4;++i){VMS_RS(g_fb_dirty)=false;host_loop();}assert(value("calls")==4&&VMS_EXEC(g_serial_calls).empty());
    std::cout<<"PASS callbacks_retained=4\n";
    setup(true);assert(run(find_method(fixture,"startWorker","()I",false),{}).i==1);
    for(int i=0;i<24&&(value("afterService")<6||value("calls")<4||VMS_EXEC(g_thread_live));++i){VMS_RS(g_fb_dirty)=false;host_loop();assert(VMS_RS(g_paint_depth)==0&&VMS_EXEC(g_run_depth)==0&&!VMS_EXEC(g_thrown));}
    assert(value("workers")==6&&value("afterService")==6&&value("calls")==4&&!VMS_EXEC(g_thread_live));
    std::cout<<"PASS host_sequence_worker_fairness=6\n";
    setup(true,1);for(int i=0;i<12&&value("calls")<4;++i){VMS_RS(g_fb_dirty)=false;host_loop();}assert(value("syncPairs")==4&&value("inside")==0);
    std::cout<<"PASS explicit_synchronous_pairs=4 not_paint_preemption=1\n";
    for(int cap:{30,60}){VMS_EXEC(g_speed_cap)=cap;assert(!host_boundary242_active()&&!host_frame_ready242()&&host_speculation_allowed242());}
    VMS_EXEC(g_speed_cap)=0;VMS_EXEC(g_notify_paused)=true;host_lifecycle_poll242();assert(VMS_EXEC(g_midlet_state)==MidletState::Paused&&!VMS_EXEC(g_notify_paused));
    VMS_EXEC(g_notify_destroyed)=true;host_lifecycle_poll242();assert(VMS_EXEC(g_midlet_state)==MidletState::Destroyed);
    std::cout<<"PASS cap_exclusion=2 pause_and_destroy_priority=2\n";
}
