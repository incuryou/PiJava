// Headless game smoke/profiling harness. No game bytecode or fields are changed.
// There is no display/GPU here; counters must not be reported as Switch FPS.
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/resource.h>
#define main emulator_main
#include PROBE_VM
#undef main
#include PROBE_MENU
#include PROBE_SPEED
static host_menu::Settings g_menu_settings;
static std::string snapshot_file;
static std::string instant_state_path(const std::string&) { return snapshot_file; }
#include PROBE_SAVE
static std::string quoted(const std::string& s) {
    std::ostringstream out;out<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')out<<'\\'<<c;else if(c<32){const char* h="0123456789abcdef";out<<"\\u00"<<h[c>>4]<<h[c&15];}else out<<c;}out<<'"';return out.str();
}
static Value read_field(int ref,const char* name,const char* desc){
    if(ref<=0||ref>=int(g_heap.size()))return {};Field* f=find_field(g_heap[ref].cls,name,desc);
    if(!f)return {};return f->is_static?g_classes[f->owner].statics[f->slot]:g_heap[ref].fields[f->slot];
}
static void scene(std::ostream& out) {
    const std::string name=g_current>0 && g_heap[g_current].cls>=0?g_classes[g_heap[g_current].cls].name:"";
    out<<",\"canvas_class\":"<<quoted(name);
    if(name=="n")out<<",\"read_only_n_d_int\":"<<read_field(g_current,"d","I").i
        <<",\"read_only_n_d_bool\":"<<read_field(g_current,"d","Z").i
        <<",\"read_only_n_c_bool\":"<<read_field(g_current,"c","Z").i
        <<",\"read_only_n_height\":"<<read_field(g_current,"b","I").i
        <<",\"read_only_n_player\":"<<read_field(g_current,"a","Ljavax/microedition/media/Player;").ref;
    if(name=="v")out<<",\"read_only_v\":"<<read_field(g_current,"v","I").i<<",\"read_only_w\":"<<read_field(g_current,"w","I").i<<",\"read_only_d\":"<<read_field(g_current,"d","I").i;
}
int main(int argc,char** argv) {
    if(argc!=8){std::cerr<<"jar prefix seconds cap keys save_at_ms load_snapshot_or_dash\n";return 2;}
    const std::string prefix=argv[2];const int seconds=std::atoi(argv[3]),cap=std::atoi(argv[4]),save_at=std::atoi(argv[6]);
    if(seconds<1||seconds>3600||(cap!=0&&cap!=30&&cap!=60))return 2;
    snapshot_file=prefix+".sav";setenv("J2ME_RMS_DIR",(prefix+"-rms").c_str(),1);
    std::vector<std::pair<int,int>> keys;std::istringstream input(argv[5]);std::string item;
    while(std::getline(input,item,',')){auto c=item.find(':');if(c==std::string::npos)return 2;keys.emplace_back(std::stoi(item.substr(0,c)),std::stoi(item.substr(c+1)));}
    g_boot_ms=now_ms();g_classpath={PROBE_ROM};g_log.open(prefix+".log");g_log_draw=true;g_speed_cap=cap;g_slice_ms=14;
#ifdef PROBE_OPT
    configure_runtime_diagnostics();
#endif
    std::vector<uint8_t> bytes;std::string err;if(!read_whole(argv[1],bytes)||!unzip(bytes,g_jar_files,err)){std::cerr<<err;return 3;}
    load_props();std::string midlet=manifest_midlet();if(midlet.empty())midlet=find_midlet_class();for(char&c:midlet)if(c=='.')c='/';
    g_fb.assign(size_t(g_screen_w)*g_screen_h*3,0);
    log_line(std::string("PROBE version=")+kVer+" jar="+argv[1]+" midlet="+midlet+" cap="+std::to_string(cap)+" scope=headless_no_display");
    const std::string identity=rms_jar_identity(bytes.data(),bytes.size());launch_class(midlet,identity);
    bool loaded=true;
    if(std::string(argv[7])!="-"){snapshot_file=argv[7];loaded=restore_instant_state(argv[1],identity);snapshot_file=prefix+".sav";if(!loaded)return 4;}
    const int64_t start=paint_clock_us();const clock_t cpu_start=std::clock();
    int64_t last_window=start,last_capture=start;size_t key=0;int held=0,mask=0;int64_t release=0;bool saved=false,restored=false,exact=false;
    uint64_t consumed=0,measured_draws=0;std::ofstream windows(prefix+"-windows.jsonl");
    bool graceful_exit=false,pause_attempted=false,pause_stable=false,resume_active=false;
    const bool exercise_pause=std::getenv("PROBE_PAUSE")!=nullptr;
    uint64_t total_jit_calls=0,total_jit_ops=0;
    while(paint_clock_us()-start<seconds*1000000LL) {
        if(g_midlet_state==MidletState::Destroyed) { graceful_exit=true;break; }
        if(exercise_pause&&!pause_attempted&&(paint_clock_us()-start)>3500000) {
            pause_attempted=true;host_freeze236(true);
            const auto frozen=g_fb;const auto frames=g_game_frames;
            for(int i=0;i<50;++i){advance_game_frame();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
            pause_stable=g_host_frozen236 && g_host_paused && frozen==g_fb && frames==g_game_frames;
            host_freeze236(false);host_events();resume_active=g_midlet_state==MidletState::Active;
            log_line("PROBE_PAUSE stable="+std::to_string(pause_stable)+" resumed="+std::to_string(resume_active));
        }
        int64_t now=paint_clock_us(),elapsed=(now-start)/1000;
        if(held&&now>=release){g_key_states&=~mask;dispatch_key(held,true);held=mask=0;}
        if(!held&&key<keys.size()&&elapsed>=keys[key].first){
            held=keys[key++].second;int action=game_key_action(held);mask=action>0&&action<31?1<<action:0;
            g_key_states|=mask;dispatch_key(held,false);release=paint_clock_us()+100000;
            log_line("PROBE_KEY elapsed_ms="+std::to_string(elapsed)+" code="+std::to_string(held));
        }
        speed_wait_paced_sleep();advance_game_frame();
        if(g_fb_dirty&&!speed_group_incomplete()){++consumed;g_fb_dirty=false;if(game_runnable()&&!speed_ahead())advance_game_frame();}
        now=paint_clock_us();
        if(save_at>=0&&!saved&&(now-start)/1000>=save_at) {
            const auto partial=g_paint_stats.take(now,g_game_frames);measured_draws+=partial.canvas_paints+partial.gamecanvas_flushes;
#ifdef PROBE236
            total_jit_calls+=g_native_jit.stats.executed;total_jit_ops+=g_native_jit.stats.ops;
#endif
            const auto before=g_fb;saved=save_instant_state(argv[1]);restored=saved&&restore_instant_state(argv[1],identity);exact=restored&&g_fb==before;
            log_line("PROBE_ROUNDTRIP saved="+std::to_string(saved)+" restored="+std::to_string(restored)+" snapshot_RGB_equal="+std::to_string(exact));
            if(!saved||!restored||!exact)return 5;
            now=paint_clock_us();last_window=now;
        }
        if(now-last_window>=1000000) {
            auto ps=g_paint_stats.take(now,g_game_frames);measured_draws+=ps.canvas_paints+ps.gamecanvas_flushes;auto work=g_runtime_profile.take();
            windows<<std::setprecision(10)<<"{\"elapsed_ms\":"<<(now-start)/1000.0<<",\"window_ms\":"<<ps.elapsed_us/1000.0
                <<",\"canvas_draws\":"<<ps.canvas_paints<<",\"canvas_fps\":"<<ps.canvas_fps()<<",\"paint_ms\":"<<ps.paint_us/1000.0
                <<",\"worker_wall_ms\":"<<work.worker_gross_us/1000.0<<",\"worker_remaining_wall_ms\":"<<work.remaining_wall_us()/1000.0
                <<",\"clock_scale\":"<<g_game_clock.scale<<",\"consumed_final_buffers\":"<<consumed;
#ifdef PROBE_OPT
            windows<<",\"worker_cpu_ms\":"<<work.worker_cpu_ns/1000000.0<<",\"paint_cpu_ms\":"<<work.paint_cpu_ns/1000000.0
                <<",\"content_changes\":"<<g_frame_metrics.pending.content_frames;
            report_optimization_window(ps.elapsed_us,work);
#endif
            windows<<",\"m3g_triangles_cumulative\":"<<g_m3g_native_triangles<<",\"m3g_pixels_cumulative\":"<<g_m3g_native_pixels;
#ifdef PROBE236
            windows<<",\"jit_calls_current_epoch\":"<<g_native_jit.stats.executed;
#endif
            scene(windows);windows<<"}\n";windows.flush();last_window=now;
        }
        if(now-last_capture>=4000000){write_png(prefix+"-at-"+std::to_string((now-start)/1000000)+".png",g_fb,g_screen_w,g_screen_h);last_capture=now;}
        const int64_t left=1000-(paint_clock_us()-now);if(left>0)std::this_thread::sleep_for(std::chrono::microseconds(left));
    }
    if(held){g_key_states&=~mask;dispatch_key(held,true);}
    const auto partial=g_paint_stats.take(paint_clock_us(),g_game_frames);measured_draws+=partial.canvas_paints+partial.gamecanvas_flushes;
    const double cpu_ms=1000.0*(std::clock()-cpu_start)/CLOCKS_PER_SEC;const double wall_ms=(paint_clock_us()-start)/1000.0;
    write_png(prefix+"-final.png",g_fb,g_screen_w,g_screen_h);struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
    std::ofstream result(prefix+"-result.json");
    result<<std::setprecision(10)<<"{\"version\":"<<quoted(kVer)<<",\"midlet\":"<<quoted(midlet)<<",\"jar_identity\":"<<quoted(identity)
        <<",\"cap\":"<<cap<<",\"wall_ms\":"<<wall_ms<<",\"process_cpu_ms\":"<<cpu_ms<<",\"draws\":"<<measured_draws
        <<",\"max_rss_kib\":"<<usage.ru_maxrss<<",\"snapshot_saved\":"<<(saved?"true":"false")<<",\"snapshot_restored\":"<<(restored?"true":"false")
        <<",\"snapshot_RGB_equal\":"<<(exact?"true":"false")<<",\"external_snapshot_loaded\":"<<(std::string(argv[7])!="-"&&loaded?"true":"false")
        <<",\"keys_sent\":"<<key<<",\"thread_live\":"<<(g_thread_live?"true":"false");scene(result);
#ifdef PROBE236
    total_jit_calls+=g_native_jit.stats.executed;total_jit_ops+=g_native_jit.stats.ops;
    result<<",\"flag_spin_pumps\":"<<g_flag_spin_pumps;
#endif
    result<<",\"m3g_triangles\":"<<g_m3g_native_triangles<<",\"m3g_pixels\":"<<g_m3g_native_pixels
        <<",\"jit_calls\":"<<total_jit_calls<<",\"jit_bytecodes\":"<<total_jit_ops
        <<",\"pause_attempted\":"<<pause_attempted<<",\"pause_stable\":"<<pause_stable<<",\"resume_active\":"<<resume_active
        <<",\"graceful_exit\":"<<graceful_exit;
    result<<",\"scope\":\"bounded headless desktop smoke; no display/GPU; CPU includes logging/screenshots; no full gameplay or hardware claim\"}\n";
    log_line("PROBE_COMPLETE normal=1 scope=bounded_desktop_smoke");g_log.flush();
    return 0;
}
