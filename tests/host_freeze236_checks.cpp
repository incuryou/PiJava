#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
int main(){
 VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
 launch_class("LifecycleChecks");
 int c=ensure_class("LifecycleChecks");auto f=[&](const char* name){return VMS_CL(g_classes)[c].statics[find_field(c,name,"I")->slot].i;};
 auto call=[&](const char* name){return run(find_method(c,name,"()I",false),{}).i;};
 assert(call("workers")==1);thread_resume();
 int starts=f("starts"),pauses=f("pauses"),shows=f("showA"),hides=f("hideA"),ticks=f("workerTicks");
 dispatch_key(49,false);VMS_PLAT(g_key_states)=31;
 auto& t=VMS_EXEC(g_threads).front();t.state=ThreadState::Sleeping;t.wake=monotonic_ms()+1000;t.wake_us=0;
 const auto due=t.wake;const int audio=VMS_PLAT(g_audio).playTone(60,10000,70);
 host_freeze236(true);host_freeze236(true);
 const int64_t clock=game_current_ms();const auto pixels=VMS_RS(g_fb);const auto frames=VMS_RS(g_game_frames);const auto media=VMS_PLAT(g_audio).time(audio);
 for(int i=0;i<60;++i){advance_game_frame();dispatch_key(53,false);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
 assert(VMS_EXEC(g_midlet_state)==MidletState::Active&&VMS_PLAT(g_host_paused)&&VMS_PLAT(g_host_frozen236));
 assert(clock==game_current_ms()&&pixels==VMS_RS(g_fb)&&frames==VMS_RS(g_game_frames)&&media==VMS_PLAT(g_audio).time(audio));
 assert(starts==f("starts")&&pauses==f("pauses")&&shows==f("showA")&&hides==f("hideA")&&ticks==f("workerTicks"));
 assert(!VMS_PLAT(g_key_states)&&!f("held"));
 host_freeze236(false);host_events();
 assert(!VMS_PLAT(g_host_paused)&&!VMS_PLAT(g_host_frozen236)&&!VMS_PLAT(g_audio).paused()&&VMS_EXEC(g_midlet_state)==MidletState::Active);
 assert(t.wake>=due+100&&game_current_ms()-clock<100);
 assert(starts==f("starts")&&pauses==f("pauses")&&shows==f("showA")&&hides==f("hideA"));
 t.wake=monotonic_ms()-1;thread_resume();assert(f("workerTicks")>ticks);
 host_freeze236(true);lifecycle_destroy(false);assert(!VMS_PLAT(g_host_frozen236)&&VMS_EXEC(g_midlet_state)==MidletState::Destroyed);
 std::cout<<"PASS host freeze: no application restart/visibility callbacks; pixels/timers/worker/audio stable; deadline shift; input release; destroy reset\n";
}
