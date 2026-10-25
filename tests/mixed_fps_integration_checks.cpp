#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iomanip>
#include <cassert>
#include "../src/host_menu_settings.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) {return "build/mixed-fps.state";}
#include "../src/instant_state.inc"
static void fixture(int mode,int cap) {
    stop_thread();VMS_CL(g_classes).clear();VMS_CL(g_class_index).clear();VMS_HEAP(g_heap).assign(1,Obj{});VMS_HEAP(g_free).clear();
    VMS_RS(g_gfx_color).clear();VMS_RS(g_gfx_target).clear();VMS_RS(g_gfx_tx).clear();VMS_RS(g_gfx_ty).clear();VMS_RS(g_clip).clear();VMS_RS(g_offscreen).clear();
    VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_slice_ms)=4;VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_midlet_state)=MidletState::Active;VMS_RS(g_current)=0;
    int cls=ensure_class("MixedFpsChecks");run(find_method(cls,"start","(I)V",false),{Value::of_i(mode)});
}
struct Window {double fps,low,high;unsigned reset_edges;};
static Window observe(int duration) {
    const int64_t start=monotonic_ms();const int frames=VMS_RS(g_game_frames);
    Window w{0,VMS_EXEC(g_game_clock).scale,VMS_EXEC(g_game_clock).scale,0};double previous=w.low;
    while(monotonic_ms()-start<duration) {
        speed_wait_paced_sleep();advance_game_frame();VMS_RS(g_fb_dirty)=false;
        double s=VMS_EXEC(g_game_clock).scale;w.low=std::min(w.low,s);w.high=std::max(w.high,s);
        if(previous>1.01 && s<=1.01)++w.reset_edges;previous=s;
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    w.fps=double(VMS_RS(g_game_frames)-frames)*1000/(monotonic_ms()-start);return w;
}

#ifndef J2ME_TEST_BASELINE
static void unit_checks() {
    stop_thread();VMS_EXEC(g_speed_cap)=60;speed_sync_clock();
    Method gate;gate.name="auditGate";
    const int cls=ensure_class("java/lang/Thread");auto* native=find_method(cls,"sleep","(J)V",false);
    auto proof=[&]() -> JavaThreadContext& {
        stop_thread();VMS_EXEC(g_speed_cap)=60;speed_sync_clock();
        VMS_EXEC(g_threads).emplace_back();auto& t=VMS_EXEC(g_threads).back();
        VMS_EXEC(g_running_thread)=&t;VMS_EXEC(g_speed_budget_owner)=&t;
        t.frame_group_started_us=paint_clock_us();t.frame_due_us=paint_clock_us()+16667;
        t.auto_fps.started=true;t.auto_fps.verified_gate_seen=true;t.auto_fps.gate_method=&gate;
        t.auto_fps.gate_pc=12;t.auto_fps.stable=8;t.auto_fps.candidate=70000;
        t.auto_fps.verified_period_us=70000;t.auto_fps.group_open=true;
        VMS_EXEC(g_game_clock).rate(paint_clock_us(),3);return t;
    };
    auto sleep=[&](int ms) {Frame f;f.method=native;f.locals={Value::of_l(ms)};
        const bool can=VMS_EXEC(g_can_suspend);VMS_EXEC(g_can_suspend)=true;
        thread_native(native,f);VMS_EXEC(g_can_suspend)=can;};
    for(int ms:{1,8,120}) {
        auto& t=proof();const auto before=game_current_ms();sleep(ms);
        assert(t.frame_sleep_bridge && t.auto_fps.verified_period_us==70000 && VMS_EXEC(g_game_clock).scale==3);
        speed_note_paint(paint_clock_us(),&t);
        assert(t.auto_fps.stable>=8 && !t.frame_sleep_bridge && t.mixed_wait && VMS_EXEC(g_game_clock).scale>1);
        assert(game_current_ms()>=before);
    }
    {auto& t=proof();sleep(121);assert(!t.frame_sleep_bridge && !t.auto_fps.started && VMS_EXEC(g_game_clock).scale==1);}
    {auto& t=proof();t.frame_group_started_us=paint_clock_us()-200000;sleep(8);
        assert(!t.frame_sleep_bridge && !t.auto_fps.started && VMS_EXEC(g_game_clock).scale==1);}
    {auto& t=proof();t.frame_due_us=0;sleep(8);assert(!t.frame_sleep_bridge && t.wake_us==0 && !t.auto_fps.started);}
    {auto& t=proof();VMS_EXEC(g_speed_budget_owner)=nullptr;sleep(8);
        assert(!t.frame_sleep_bridge && !t.auto_fps.started && t.frame_due_us==0 && t.wake_us>0);}
    {auto& t=proof();t.auto_fps.verified_period_us=0;t.auto_fps.polls=t.auto_fps.yields=0;
        t.sleep_paced=true;t.frame_sleep_bridge=true;speed_note_paint(paint_clock_us(),&t);
        assert(!t.auto_fps.started && !t.mixed_wait && VMS_EXEC(g_game_clock).scale==1);}
    {auto& t=proof();t.frame_sleep_bridge=t.mixed_wait=true;speed_pause_clock(true);
        assert(!t.frame_sleep_bridge && !t.mixed_wait && !t.auto_fps.started);speed_pause_clock(false);}
    {auto& t=proof();t.frame_sleep_bridge=t.mixed_wait=true;VMS_EXEC(g_speed_cap)=30;speed_sync_clock();
        assert(!t.frame_sleep_bridge && !t.mixed_wait && !t.auto_fps.started);}
    {auto& t=proof();sleep(8);assert(t.frame_sleep_bridge);
        auto* interrupt=find_method(cls,"interrupt","()V",false);Frame f;f.method=interrupt;f.locals={Value::of_ref(t.ref)};
        thread_native(interrupt,f);assert(!t.frame_sleep_bridge && !t.auto_fps.started && VMS_EXEC(g_game_clock).scale==1);}
    fixture(0,60);
    VMS_EXEC(g_main_thread).frame_sleep_bridge=VMS_EXEC(g_main_thread).mixed_wait=true;
    for(auto& t:VMS_EXEC(g_threads))t.frame_sleep_bridge=t.mixed_wait=true;
    const int canvas_class=ensure_class("MixedFpsChecks");
    run(find_method(canvas_class,"changeCanvas","()V",false),{});
    assert(!VMS_EXEC(g_main_thread).frame_sleep_bridge && !VMS_EXEC(g_main_thread).mixed_wait);
    for(const auto& t:VMS_EXEC(g_threads))assert(!t.frame_sleep_bridge && !t.mixed_wait);
    stop_thread();std::cout<<"PASS mixed frame ownership, expiry, pure-sleep transition, pause, Canvas and mode reset\n";
}
#endif
static int snapshot(bool writing,const char* path) {
    if(writing) {
        fixture(6,60);observe(3000);VMS_EXEC(g_midlet)=VMS_RS(g_current);
        SavState state;std::string bytes;assert(capture_state(state,"mixed-fixture.jar") && encode_state(state,bytes));
        assert(bytes.substr(0,4)=="J2S5");std::ofstream f(path,std::ios::binary);f.write(bytes.data(),bytes.size());
        assert(f.good());std::cout<<"PASS baseline J2S5 written bytes="<<bytes.size()<<"\n";
    } else {
        VMS_EXEC(g_speed_cap)=60;std::ifstream f(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(f)),{});
        SavState state;assert(decode_state(bytes,state));
        VMS_EXEC(g_save_clock).valid=false;VMS_EXEC(g_save_clock).ms=monotonic_ms();VMS_EXEC(g_save_clock).us=paint_clock_us();
        assert(apply_state(state));
        assert(VMS_RS(g_fb)==state.fb && VMS_EXEC(g_game_clock).scale==1);
        for(const auto& t:VMS_EXEC(g_threads))assert(!t.auto_fps.started);
        observe(3000);const auto w=observe(1800);assert(w.fps>52.8 && w.fps<66);
        std::cout<<"PASS prior-build J2S5 restored, first framebuffer equal, mixed fps="<<w.fps<<"\n";
    }
    stop_thread();return 0;
}
int main(int argc,char**argv) {
    if(argc!=5)return 2;
    VMS_CL(g_classpath)={argv[1],argv[4]};
    if(std::string(argv[2])=="snapshot-write")return snapshot(true,argv[3]);
    if(std::string(argv[2])=="snapshot-read")return snapshot(false,argv[3]);
#ifndef J2ME_TEST_BASELINE
    if(std::string(argv[2])=="unit"){unit_checks();return 0;}
#endif
    int mode=std::atoi(argv[2]),cap=std::atoi(argv[3]);fixture(mode,cap);observe(mode>=10 && cap?5000:3000);auto w=observe(2400);
    const int cls=ensure_class("MixedFpsChecks");const int updates=VMS_CL(g_classes)[cls].statics[find_field(cls,"updates","I")->slot].i;
    std::cout<<std::setprecision(8)<<"{\"mode\":"<<mode<<",\"cap\":"<<cap<<",\"fps\":"<<w.fps<<",\"scale_min\":"<<w.low<<",\"scale_max\":"<<w.high<<",\"reset_edges\":"<<w.reset_edges<<"}\n"<<std::flush;
#ifndef J2ME_TEST_BASELINE
    if(cap && mode!=8 && mode!=9)assert(w.fps>cap*0.88 && w.fps<cap*1.1);
    if(cap && (mode==8 || mode==9))assert(w.fps>cap*(mode==9?2:1)*0.88 && w.fps<cap*(mode==9?2:1)*1.1);
    if(!cap)assert(w.low==1 && w.high==1 && w.fps<45);
    if(cap && (mode==10 || mode==11 || mode==13))assert(updates>=100 && w.low==1 && w.high==1);
    if(cap && (mode==3 || mode==4 || mode==6 || mode==7 || mode==12))assert(w.reset_edges==0);
#endif
    stop_thread();return 0;
}
