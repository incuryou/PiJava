#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
int main(){
 VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_EXEC(g_slice_ms)=2;
 const int64_t before=monotonic_ms();
 launch_class("AsyncStart236Checks","75aaf194cbd01715d4eaa99720e6876ff2355e494d3ed5f09c33d85cae81b100");
 assert(monotonic_ms()-before<2000&&VMS_RS(g_current)&&VMS_EXEC(g_thread_live));
 int c=ensure_class("AsyncStart236Checks");auto f=[&](const char* n)->Value&{return VMS_CL(g_classes)[c].statics[find_field(c,n,n==std::string("stop")?"Z":"I")->slot];};
 for(int i=0;i<30;++i){advance_game_frame();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
 dispatch_key(53,false);dispatch_key(53,true);assert(f("ticks").i>1&&f("keys").i==1);
 const int ticks=f("ticks").i;host_freeze236(true);
 for(int i=0;i<30;++i){advance_game_frame();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
 assert(f("ticks").i==ticks&&f("starts").i==1&&f("pauses").i==0);
 host_freeze236(false);f("stop")=Value::of_i(1);
 for(int i=0;i<10;++i){advance_game_frame();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
 assert(!VMS_EXEC(g_notify_destroyed)&&!VMS_EXEC(g_notify_paused)&&VMS_EXEC(g_midlet_state)==MidletState::Active);
 lifecycle_destroy(false);assert(VMS_EXEC(g_threads).empty());
 std::cout<<"PASS async startApp Java continuation: launcher returns while loop live; keys/paint/freeze/completion/destroy\n";
}
