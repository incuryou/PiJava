#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include "../src/host_menu_settings.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) {return "build/auto-fps.state";}
#include "../src/instant_state.inc"

static void fixture(int mode,int cap) {
    stop_thread();VMS_CL(g_classes).clear();VMS_CL(g_class_index).clear();VMS_HEAP(g_heap).assign(1,Obj{});VMS_HEAP(g_free).clear();
    VMS_RS(g_gfx_color).clear();VMS_RS(g_gfx_target).clear();VMS_RS(g_gfx_tx).clear();VMS_RS(g_gfx_ty).clear();VMS_RS(g_clip).clear();VMS_RS(g_offscreen).clear();
    VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_slice_ms)=4;VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;VMS_RS(g_current)=0;
    int cls=ensure_class("AutoFpsChecks");
    run(find_method(cls,"start","(I)V",false),{Value::of_i(mode)});
}
static double observe(int duration) {
    const int64_t start=monotonic_ms();
    const int frames=VMS_RS(g_game_frames);
    while(monotonic_ms()-start<duration) {
        speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    return double(VMS_RS(g_game_frames)-frames)*1000/(monotonic_ms()-start);
}
static int64_t java_time() {
    const int cls=ensure_class("java/lang/System");
    return run(find_method(cls,"currentTimeMillis","()J",false),{}).l;
}
int main(int argc,char** argv) {
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};
    if(!(argc==2 && std::string(argv[1])=="--clock-only"))
    for(int cap:{0,30,60})for(int mode:{0,1,2,3,4,5,6,7}) {
        if(argc==3 && (cap!=std::atoi(argv[1]) || mode!=std::atoi(argv[2])))continue;
        // Clock changes are bounded to 0.2 per verified cycle. Allow the
        // conservative ramp to settle before checking sustained throughput.
        fixture(mode,cap);observe(cap?3000:1800);const double fps=observe(1800);
        std::cout<<"mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" scale="<<VMS_EXEC(g_game_clock).scale<<"\n";
        if(!cap) {assert(VMS_EXEC(g_game_clock).scale==1);assert(fps<45);}
        else if(mode==3 || mode==7) {assert(VMS_EXEC(g_game_clock).scale==1);assert(fps<23);}
        else {assert(fps>cap*0.88 && fps<cap*1.1);}
        if(cap==60 && (mode==4 || mode==5))assert(VMS_EXEC(g_game_clock).scale>1.4 && VMS_EXEC(g_game_clock).scale<1.7);
    }
    fixture(1,60);observe(1800);
    const int64_t before=game_current_ms();VMS_EXEC(g_game_clock).rate(paint_clock_us(),2);
    assert(game_current_ms()>=before);
    speed_pause_clock(true);const int64_t paused=game_current_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));assert(game_current_ms()==paused);
    speed_pause_clock(false);assert(game_current_ms()>=paused);
    VMS_EXEC(g_speed_cap)=30;game_current_ms();assert(VMS_EXEC(g_game_clock).scale==1);
    VMS_EXEC(g_midlet)=VMS_RS(g_current);
    SavState state;assert(capture_state(state,"fixture.jar"));
    std::string bytes;assert(encode_state(state,bytes));assert(bytes.substr(0,4)=="J2S5");
    SavState decoded;assert(decode_state(bytes,decoded));assert(decoded.game_millis==state.game_millis);
    VMS_EXEC(g_game_clock).start(paint_clock_us(),1);
    const int64_t restore_started=paint_clock_us();
    VMS_EXEC(g_save_clock).valid=false;VMS_EXEC(g_save_clock).ms=monotonic_ms();VMS_EXEC(g_save_clock).us=restore_started;
    assert(apply_state(decoded));assert(game_current_ms()>=decoded.game_millis);
    assert(game_current_ms()-decoded.game_millis<=(paint_clock_us()-restore_started)/1000+5);
    assert(VMS_EXEC(g_game_clock).scale==1);
    for(const auto& t:VMS_EXEC(g_threads))assert(!t.auto_fps.started);
    // Exercise the actual Java native dispatch: checking GameClock internals
    // alone would miss a Native branch accidentally returning wall time.
    VMS_EXEC(g_game_clock).rate(paint_clock_us(),3);
    assert(capture_state(state,"fixture.jar"));assert(state.clock_scale==3);
    const int64_t capped_java=java_time();
    assert(capped_java>now_ms()+100);
    VMS_EXEC(g_speed_cap)=0;
    const int64_t native_java=java_time();
    assert(native_java>=capped_java && native_java-capped_java<=5);
    assert(VMS_EXEC(g_game_clock).scale==1);
    SavState native_saved;assert(capture_state(native_saved,"fixture.jar"));
    assert(native_saved.game_millis>=native_java && native_saved.game_millis-native_java<=5);
    assert(native_saved.clock_scale==1);
    assert(encode_state(state,bytes));assert(decode_state(bytes,decoded));
    const int64_t native_restore_started=paint_clock_us();
    VMS_EXEC(g_save_clock).valid=false;VMS_EXEC(g_save_clock).ms=monotonic_ms();VMS_EXEC(g_save_clock).us=native_restore_started;
    assert(apply_state(decoded));
    const int64_t restored_java=java_time();
    assert(restored_java>=decoded.game_millis && restored_java-decoded.game_millis<=(paint_clock_us()-native_restore_started)/1000+5);
    assert(VMS_EXEC(g_game_clock).scale==1);
    const int64_t real_start=monotonic_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int64_t java_elapsed=java_time()-restored_java;
    assert(std::llabs(java_elapsed-(monotonic_ms()-real_start))<=5);
    assert(capture_state(native_saved,"fixture.jar"));
    assert(native_saved.game_millis>=restored_java && native_saved.game_millis-restored_java<100);
    const int64_t inherited_native_java=java_time();
    VMS_EXEC(g_speed_cap)=60;
    const int64_t recapped_java=java_time();
    assert(recapped_java>=inherited_native_java && recapped_java-inherited_native_java<=5);
    assert(VMS_EXEC(g_game_clock).scale==1);
    fixture(1,0);
    const int64_t wall_before=now_ms(),fresh_java=java_time(),wall_after=now_ms();
    assert(fresh_java>=wall_before && fresh_java<=wall_after);
    assert(!VMS_EXEC(g_game_clock_inherited));
    VMS_EXEC(g_speed_cap)=30;
    const int64_t fresh_capped_java=java_time();
    assert(fresh_capped_java>=fresh_java && fresh_capped_java-fresh_java<=5);
    assert(encode_state(state,bytes));bytes.replace(0,4,"J2S3");assert(!decode_state(bytes,decoded));
    std::cout<<"PASS auto FPS integration\n";
}
