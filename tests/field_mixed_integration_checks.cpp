#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iomanip>
#include <sys/resource.h>
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
// Test-only snapshots; no Java calls or production state mutations.
struct TimingOwner {
    int ref=-1,state=-1;uint64_t draws=0;int64_t due=0,wake_us=0,wake_ms=0,prep_woke=0,lead=0;
    unsigned prep_count=0;bool awaiting_wake=false,awaiting_draw=false,budget=false;
};
static TimingOwner timing_owner() {
    TimingOwner out;
    for(const auto& t:VMS_EXEC(g_threads))if(t.owned_draws>=out.draws && t.owned_draws) {
        out={t.ref,int(t.state),t.owned_draws,t.frame_due_us,t.wake_us,t.wake,t.frame_prep.woke_us,
             t.frame_prep.lead_us(speed_period_us()),t.frame_prep.count,t.frame_prep.awaiting_wake,
             t.frame_prep.awaiting_draw,&t==VMS_EXEC(g_speed_budget_owner)};
    }
    return out;
}
struct TimingRow {
    int64_t begin=0,wait_end=0,advance_end=0,next_before=0,next_after=0,cap_slept=0,cap_added=0;
    int draws=0;uint64_t owned=0,unowned=0,requested=0,effective=0;double scale=1;
    TimingOwner before,after_wait,after;
};
static std::ofstream g_field_timing;
static void timing_dump(const char* phase,const std::vector<TimingRow>& rows) {
    auto state=[](std::ostream& f,const TimingOwner& t) {
        f<<"{\"ref\":"<<t.ref<<",\"state\":"<<t.state<<",\"draws\":"<<t.draws<<",\"frame_due_us\":"<<t.due<<",\"wake_us\":"<<t.wake_us<<",\"wake_ms\":"<<t.wake_ms<<",\"prep_woke_us\":"<<t.prep_woke<<",\"prep_lead_us\":"<<t.lead<<",\"prep_count\":"<<t.prep_count<<",\"awaiting_wake\":"<<t.awaiting_wake<<",\"awaiting_draw\":"<<t.awaiting_draw<<",\"budget\":"<<t.budget<<"}";
    };
    for(const auto& r:rows) {
        g_field_timing<<"{\"phase\":\""<<phase<<"\",\"begin_us\":"<<r.begin<<",\"wait_end_us\":"<<r.wait_end<<",\"advance_end_us\":"<<r.advance_end<<",\"wait_call_us\":"<<r.wait_end-r.begin<<",\"advance_call_us\":"<<r.advance_end-r.wait_end<<",\"draw_delta\":"<<r.draws<<",\"owned_sleep_delta\":"<<r.owned<<",\"unowned_sleep_delta\":"<<r.unowned<<",\"requested_ms_delta\":"<<r.requested<<",\"effective_ms_delta\":"<<r.effective<<",\"cap_slept_us_delta\":"<<r.cap_slept<<",\"cap_added_us_delta\":"<<r.cap_added<<",\"speed_next_before\":"<<r.next_before<<",\"speed_next_after\":"<<r.next_after<<",\"scale\":"<<r.scale<<",\"before\":";
        state(g_field_timing,r.before);g_field_timing<<",\"after_wait\":";state(g_field_timing,r.after_wait);g_field_timing<<",\"after\":";state(g_field_timing,r.after);g_field_timing<<"}\n";
    }
    g_field_timing.flush();
}
struct Window {double fps,low,high;unsigned reset_edges;};
static Window observe(int duration,const char* phase="attack") {
    const int64_t start=monotonic_ms();const int frames=VMS_RS(g_game_frames);
    Window w{0,VMS_EXEC(g_game_clock).scale,VMS_EXEC(g_game_clock).scale,0};double previous=w.low;
    const bool trace=std::getenv("FIELD_MIXED_TRACE")!=nullptr;
    auto cpu_us=[](){struct rusage u{};getrusage(RUSAGE_SELF,&u);return int64_t(u.ru_utime.tv_sec+u.ru_stime.tv_sec)*1000000+u.ru_utime.tv_usec+u.ru_stime.tv_usec;};
    int64_t traced_at=start,traced_cpu=cpu_us();int traced_draws=frames;
    std::vector<TimingRow> timing;if(g_field_timing.is_open())timing.reserve(2048);
    while(monotonic_ms()-start<duration) {
        TimingRow row;const auto before_stats=g_frame_sleep_stats;
        const int before_draws=VMS_RS(g_game_frames);
        const int64_t slept_before=VMS_EXEC(g_speed_slept_us),added_before=VMS_EXEC(g_speed_add_us);
        if(g_field_timing.is_open()){row.before=timing_owner();row.next_before=VMS_EXEC(g_speed_next_us);row.begin=paint_clock_us();}
        speed_wait_paced_sleep();
        if(g_field_timing.is_open()){row.wait_end=paint_clock_us();row.after_wait=timing_owner();}
        advance_game_frame();
        if(g_field_timing.is_open()) {
            row.advance_end=paint_clock_us();row.after=timing_owner();row.next_after=VMS_EXEC(g_speed_next_us);row.scale=VMS_EXEC(g_game_clock).scale;
            row.draws=VMS_RS(g_game_frames)-before_draws;row.owned=g_frame_sleep_stats.owned-before_stats.owned;row.unowned=g_frame_sleep_stats.unowned-before_stats.unowned;
            row.requested=g_frame_sleep_stats.requested_ms-before_stats.requested_ms;row.effective=g_frame_sleep_stats.effective_ms-before_stats.effective_ms;
            row.cap_slept=VMS_EXEC(g_speed_slept_us)-slept_before;row.cap_added=VMS_EXEC(g_speed_add_us)-added_before;
            if(row.draws||row.owned||row.unowned||row.wait_end-row.begin>=2000||row.advance_end-row.wait_end>=2000)timing.push_back(row);
        }
        VMS_RS(g_fb_dirty)=false;
        double s=VMS_EXEC(g_game_clock).scale;w.low=std::min(w.low,s);w.high=std::max(w.high,s);
        if(previous>1.01 && s<=1.01)++w.reset_edges;previous=s;
        const int64_t traced_now=monotonic_ms();
        if(trace && traced_now-traced_at>=500) {
            const auto cpu=cpu_us();
            std::cerr<<"FIELDTRACE wall_ms="<<traced_now-traced_at<<" cpu_us="<<cpu-traced_cpu<<" draws="<<VMS_RS(g_game_frames)-traced_draws<<" scale="<<s;
            for(const auto& t:VMS_EXEC(g_threads))if(t.owned_draws) {
                const auto& a=t.auto_fps;const auto& f=t.field_gate_anchor;
                std::cerr<<" owner="<<t.ref<<" state="<<int(t.state)<<" owned="<<t.owned_draws<<" started="<<a.started<<" stable="<<a.stable<<" verified="<<a.verified_gate_seen<<" period="<<a.verified_period_us<<" group="<<a.group_open<<","<<a.group_drawn<<","<<a.group_complete<<" polls="<<a.polls<<","<<a.yields<<" gatepc="<<a.gate_pc<<" waiting="<<t.poll_gate.waiting<<","<<t.poll_gate.reads<<","<<t.poll_gate.certified<<","<<t.poll_gate.fresh_ready<<" anchorpc="<<f.clock_pc<<" anchor_us="<<paint_clock_us()-f.real_us<<" anchor_draw="<<f.draws<<" budget="<<(&t==VMS_EXEC(g_speed_budget_owner));
            }
            std::cerr<<std::endl;traced_at=traced_now;traced_cpu=cpu;traced_draws=VMS_RS(g_game_frames);
        }
        if(!game_runnable())std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    w.fps=double(VMS_RS(g_game_frames)-frames)*1000/(monotonic_ms()-start);
    if(g_field_timing.is_open())timing_dump(phase,timing);
    return w;
}
static int& fixture_int(const char* name) {
    const int cls=ensure_class("MixedFpsChecks");
    return VMS_CL(g_classes)[cls].statics[find_field(cls,name,"I")->slot].i;
}
static void enabled(bool value) {
    const int cls=ensure_class("MixedFpsChecks");
    VMS_CL(g_classes)[cls].statics[find_field(cls,"fieldGateEnabled","Z")->slot]=Value::of_i(value);
}
static bool field_proof() {
    for(auto& t:VMS_EXEC(g_threads))if(t.auto_fps.verified_gate_seen&&t.auto_fps.stable>=5)return true;
    return false;
}
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    if(const char* path=std::getenv("FIELD_MIXED_TIMING"))g_field_timing.open(path);
    VMS_CL(g_classpath)={argv[1],argv[4]};
    const int mode=std::atoi(argv[2]),cap=std::atoi(argv[3]);
    fixture(mode,cap);observe(2200,"initial_warm");auto steady=observe(1200,"initial_steady");
    std::cout<<"field mode="<<mode<<" cap="<<cap<<" fps="<<steady.fps<<" scale="<<steady.low<<".."<<steady.high<<" certified="<<field_proof()<<std::endl;
    if(cap) {
        assert(field_proof());assert(steady.fps>cap*.88&&steady.fps<cap*1.1);assert(steady.reset_edges==0);
        enabled(false);observe(700,"sleep_warm");auto plain=observe(500,"sleep_steady");
        assert(plain.low==1&&plain.high==1&&!field_proof());
        enabled(true);observe(2200,"recovery_warm");auto recovered=observe(1200,"recovery_steady");
        std::cout<<"restored mode="<<mode<<" cap="<<cap<<" fps="<<recovered.fps<<" scale="<<recovered.low<<".."<<recovered.high<<" certified="<<field_proof()<<std::endl;
        assert(field_proof()&&recovered.fps>cap*.88&&recovered.fps<cap*1.1);
        // A different Java thread writes the shared anchor after the drawing
        // thread's clock store. Different, future and stale values cannot lend
        // a certified field gate to the drawing owner.
        if(mode!=16)for(int attack:{1,2,3}) {
            fixture(mode,cap);fixture_int("fieldAttack")=attack;observe(900);
            assert(!field_proof());
        }
    } else {const double native=1000.0/(mode==16?52:(mode==14?66:62));assert(steady.low==1&&steady.high==1&&!field_proof()&&steady.fps>native*.88&&steady.fps<native*1.1);}
    stop_thread();std::cout<<"PASS field gate ownership and sleep-only transition"<<std::endl;
}
