#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include <sstream>
int main() {
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_EXEC(g_speed_cap)=60;VMS_EXEC(g_slice_ms)=4;VMS_RS(g_fb).assign(240*320*3,0);VMS_EXEC(g_midlet_state)=MidletState::Active;
    std::ostringstream events;auto* oldbuf=std::cerr.rdbuf(events.rdbuf());
    const int cls=ensure_class("GateRecovery234Checks");
    run(find_method(cls,"start","()V",false),{});
    auto tick=[](){speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));};
    const int64_t start=paint_clock_us();
    while(paint_clock_us()-start<2500000)tick();
    JavaThreadContext* owner=nullptr;
    for(auto& t:VMS_EXEC(g_threads))if(t.auto_fps.helper_drawing() && &t==VMS_EXEC(g_speed_budget_owner))owner=&t;
    assert(owner && owner->auto_fps.stable>=5 && VMS_EXEC(g_game_clock).scale>2.9);
    // A real host stall, not a synthetic ready-edge call. The production
    // grouping timeout must remember proof, reset to 1x, then actual Java
    // currentTimeMillis/yield bytecode must certify a NEW edge itself.
    std::this_thread::sleep_for(std::chrono::milliseconds(160));
    assert(!speed_group_helper(owner));assert(VMS_EXEC(g_game_clock).scale==1.0);
    assert(owner->gate_recovery.method && !owner->auto_fps.started);
    const int64_t resumed=paint_clock_us();
    while(paint_clock_us()-resumed<300000 && VMS_EXEC(g_game_clock).scale<2.9)tick();
    const int64_t recovery_us=paint_clock_us()-resumed;
    assert(VMS_EXEC(g_game_clock).scale>2.9);
    assert(events.str().find("GATERECOVER owner=")!=std::string::npos);
    assert(events.str().find("path=certified-recovery")!=std::string::npos);
    std::cerr.rdbuf(oldbuf);std::cerr<<events.str();
    std::cout<<"PASS actual Java 50ms gate, injected 160ms host stall, restored in "<<recovery_us/1000.0<<"ms; not a physical Switch measurement\n";
    stop_thread();
}
