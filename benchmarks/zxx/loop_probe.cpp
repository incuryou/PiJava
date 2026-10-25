#include <ctime>
#include <iomanip>
#ifndef BENCH_VM
#error BENCH_VM required
#endif
#define main emulator_main
#include BENCH_VM
#undef main

static uint64_t presentations=0,present_us=0,withheld=0,host_turns=0;
static int field(int cls,const char* name) {
    Field* f=find_field(cls,name,"I");return g_classes[f->owner].statics[f->slot].i;
}
static void advance() {
    ++host_turns;
    const int64_t before=paint_clock_us();
    speed_wait_paced_sleep();advance_game_frame();
    if(g_fb_dirty) {
        if(!speed_group_incomplete()) {
            const int64_t started=paint_clock_us();
            std::this_thread::sleep_for(std::chrono::milliseconds(g_bench_present_delay_ms));
            present_us+=paint_clock_us()-started;++presentations;g_fb_dirty=false;
            // Match the CPU host loop's post-presentation runnable catch-up.
            if(game_runnable() && !speed_ahead())advance_game_frame();
        } else ++withheld;
    }
    const int64_t left=1000-(paint_clock_us()-before);
    if(left>0)std::this_thread::sleep_for(std::chrono::microseconds(left));
}
static unsigned certified_owners(){unsigned n=0;for(const auto&t:g_threads)n+=t.poll_gate.certified;return n;}
static unsigned verified_owners(){unsigned n=0;for(const auto&t:g_threads)n+=t.auto_fps.verified_gate_seen && t.auto_fps.stable>=5;return n;}
int main(int argc,char**argv) {
    if(argc!=5)return 2;
    const bool sync=std::atoi(argv[1])!=0;
    const char* variant=argv[2];const int observation_ms=std::atoi(argv[3]),warmup_ms=std::atoi(argv[4]);
    g_bench_paint_delay_ms=5;g_bench_present_delay_ms=2;g_slice_ms=14;
    g_boot_ms=now_ms();g_log_draw=false;g_classpath={"classes",BENCH_ROM};
    g_fb.assign(size_t(g_screen_w)*g_screen_h*3,0);g_speed_cap=60;
    const int cls=ensure_class("GatedCanvas");
    run(find_method(cls,"setup","(Z)I",false),{Value::of_i(sync)});host_events();
    const int64_t warm_started=paint_clock_us();
    while(paint_clock_us()-warm_started<int64_t(warmup_ms)*1000)advance();
    const unsigned start_certified=certified_owners(),start_verified=verified_owners();
    const int start_updates=field(cls,"updates"),start_counter=field(cls,"counter"),start_paints=field(cls,"paints"),start_cuts=g_cuts;
    const double start_scale=g_game_clock.scale;
    reset_runtime_stats();presentations=present_us=withheld=host_turns=0;
    const int64_t started=paint_clock_us();const clock_t cpu_started=std::clock();
    while(paint_clock_us()-started<int64_t(observation_ms)*1000)advance();
    const int64_t finished=paint_clock_us();
    const double cpu_ms=1000.0*(std::clock()-cpu_started)/CLOCKS_PER_SEC;
    const auto paint=g_paint_stats.take(finished,g_game_frames);const auto work=g_runtime_profile.take();
    unsigned grouped=0,helper=0,primary=0,aux=0;
    for(const auto&t:g_threads){grouped+=t.auto_fps.grouped_update();helper+=t.auto_fps.helper_drawing();}
    auto* run_method=find_method(cls,"run","()V",false);
    for(const auto&g:run_method->time_gates){primary+=g.millis==50;aux+=g.millis==100;}
    std::cout<<std::setprecision(10)
      <<"{\"variant\":\""<<variant<<"\",\"sync\":"<<(sync?"true":"false")<<",\"slice_ms\":"<<g_slice_ms
      <<",\"simulated_paint_sleep_ms\":5,\"simulated_present_sleep_ms\":2,\"warmup_ms\":"<<(started-warm_started)/1000.0
      <<",\"observe_ms\":"<<(finished-started)/1000.0<<",\"process_cpu_ms\":"<<cpu_ms
      <<",\"canvas_paints\":"<<paint.canvas_paints<<",\"canvas_fps\":"<<paint.canvas_fps()
      <<",\"java_updates\":"<<((field(cls,"updates")-start_updates+1000)%1000)<<",\"java_paints\":"<<(field(cls,"paints")-start_paints)
      <<",\"auxiliary_counter\":"<<((field(cls,"counter")-start_counter+1000)%1000)
      <<",\"host_presentations\":"<<presentations<<",\"present_sleep_wall_ms\":"<<present_us/1000.0<<",\"presentation_withheld_turns\":"<<withheld<<",\"host_turns\":"<<host_turns
      <<",\"clock_scale_start\":"<<start_scale<<",\"clock_scale_end\":"<<g_game_clock.scale
      <<",\"poll_calls\":"<<g_poll_calls<<",\"poll_gate_reads\":"<<g_poll_gate_reads<<",\"poll_waits\":"<<g_poll_waits
      <<",\"poll_wait_ms\":"<<g_poll_wait_us/1000.0<<",\"worker_gross_ms\":"<<work.worker_gross_us/1000.0
      <<",\"worker_syncpaint_ms\":"<<work.worker_syncpaint_us/1000.0<<",\"worker_remaining_wall_ms\":"<<work.remaining_wall_us()/1000.0
      <<",\"cap_budget_frames\":"<<g_speed_budget_window_frames<<",\"group_timeouts\":"<<g_speed_group_timeouts
      <<",\"worker_turns\":"<<work.worker_turns<<",\"worker_yields\":"<<work.worker_yields<<",\"worker_suspends\":"<<work.worker_suspends
      <<",\"early_repaint_handoffs\":"<<work.early_repaint_handoffs<<",\"budget_cuts\":"<<(g_cuts-start_cuts)
      <<",\"certified_owners_start\":"<<start_certified<<",\"verified_stable_owners_start\":"<<start_verified
      <<",\"certified_owners_end\":"<<certified_owners()<<",\"grouped_owners_end\":"<<grouped<<",\"helper_owners_end\":"<<helper
      <<",\"recognized_primary_50ms\":"<<primary<<",\"recognized_aux_100ms\":"<<aux
      <<",\"scope\":\"desktop scheduling skeleton with actual ZXX wait edges; host latency is sleep wall time; process CPU uses std::clock; not device/gameplay performance\"}\n";
    return 0;
}
