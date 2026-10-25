#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iomanip>
#include "../src/host_menu_settings.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) {const char* p=std::getenv("J2ME_REPLAY_STATE");return p?p:"replay.sav";}
#include "../src/instant_state.inc"
// Headless functional replay using the production shared scheduler. The
// fixed sleep presenter does not model Switch GPU, display or audio output.
// argv: JAR, absolute API classpath, output directory, duration seconds.
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    g_menu_settings.bezel=1;g_menu_settings.show_fps=true;
    const std::string out=argv[3];VMS_PLAT(g_boot_ms)=now_ms();log_open((out+"/vm.log").c_str(),true);
    VMS_CL(g_classpath)={argv[2]};VMS_DIAG(g_log_draw)=true;configure_runtime_diagnostics();
    if(std::getenv("J2ME_TEST_FULL_PRIOR_SUITE")){g_jit_region_stats.bytes=JitRegionMaxBytes;g_jit_region_stats.compiled=490;}
    std::vector<uint8_t> bytes;std::string error;
    if(!read_whole(argv[1],bytes) || !unzip(bytes,VMS_CL(g_jar_files),error))return 3;
    load_props();std::string midlet=manifest_midlet();for(char& c:midlet)if(c=='.')c='/';
    select_screen_sidecar(argv[1]);VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_speed_cap)=60;speed_sync_clock();
    log_line("REPLAYPROFILE cap=60 canvas="+std::to_string(VMS_RS(g_screen_w))+"x"+std::to_string(VMS_RS(g_screen_h))+" bezel=1 show_fps=1 presenter=headless_16ms actual_switch_timing=0");
    if(std::getenv("J2ME_REPLAY_RESTORE")){if(!restore_instant_state(argv[1],rms_jar_identity(bytes.data(),bytes.size())))return 6;}
    else {launch_class(midlet,rms_jar_identity(bytes.data(),bytes.size()));VMS_EXEC(g_speed_cap)=60;speed_sync_clock();}
    if(const char* count=std::getenv("J2ME_REPLAY_PAINTS")) {
        // Fixed saved-scene microbenchmark, not gameplay FPS: do not advance
        // the updater or Java time. Keep the same paint bytecode and RGB path.
        VMS_EXEC(g_speed_cap)=0;VMS_EXEC(g_speed_frame_open)=false;
        VMS_EXEC(g_game_clock).pause(paint_clock_us(),true);audio_suspend(true);
        const int total=std::max(1,std::atoi(count));std::vector<int64_t> elapsed;
        Method* map_method=nullptr;int map=0,graphics=0;
        if(std::getenv("J2ME_REPLAY_ZHUXIAN_MAP")) {
            const int game_class=ensure_class("g"),canvas_class=ensure_class("y");
            auto* map_field=find_field(game_class,"a","Lm;");auto* graphics_field=find_field(canvas_class,"a","Lb;");
            if(!map_field || !graphics_field || !map_field->is_static || !graphics_field->is_static)return 10;
            map=VMS_CL(g_classes)[map_field->owner].statics[map_field->slot].ref;
            graphics=VMS_CL(g_classes)[graphics_field->owner].statics[graphics_field->slot].ref;
            if(map<=0 || graphics<=0)return 10;
            map_method=find_method(VMS_HEAP(g_heap)[map].cls,"c","(Lb;)V",true);
            if(!map_method)return 10;
            log_line("MAPBENCH_PROFILE method=m.c(Lb;)V fixed_scene=1 updater=paused clock=paused test_only=1 not_gameplay_fps=1");
        }
        std::string first;bool equal=true;
        for(int i=0;i<total+20;++i) {
            const int64_t at=paint_clock_us();
            if(map_method) {++VMS_RS(g_paint_depth);run(map_method,{Value::of_ref(map),Value::of_ref(graphics)});--VMS_RS(g_paint_depth);}
            repaint_current();const int64_t us=paint_clock_us()-at;
            const auto hash=rms_jar_identity(VMS_RS(g_fb).data(),VMS_RS(g_fb).size());
            if(i==0)first=hash;else equal&=hash==first;
            if(i>=20)elapsed.push_back(us);
        }
        std::sort(elapsed.begin(),elapsed.end());
        write_png(out+"/paint.png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));
        log_line(std::string(map_method?"MAPBENCH":"PAINTBENCH")+" samples="+std::to_string(total)+" warmup=20 median_us="+std::to_string(elapsed[elapsed.size()/2])+" p95_us="+std::to_string(elapsed[(elapsed.size()-1)*95/100])+" max_us="+std::to_string(elapsed.back())+" stable_rgb="+std::to_string(equal)+" rgb_sha256="+first+" gameplay_fps=unavailable");
        lifecycle_destroy();p7::pool().shutdown();log_shutdown();return equal?0:9;
    }
    const int64_t begin=monotonic_ms();int64_t next_key=2500,release=0,window=begin;int held=0;
    const int capture_pixels=std::getenv("J2ME_REPLAY_CAPTURE_PIX")?std::atoi(std::getenv("J2ME_REPLAY_CAPTURE_PIX")):0;
    bool captured=false;
    auto seen=VMS_RS(g_game_frames);auto budgets=VMS_EXEC(g_speed_budget_frames);bool music=false;
    int64_t snapshot_due=5000;
    std::vector<std::pair<int64_t,int>> keys;size_t key_index=0;
    if(const char* script=std::getenv("J2ME_REPLAY_KEYS")){std::ifstream f(script);int64_t at;int key;while(f>>at>>key)keys.push_back({at,key});}
    if(!keys.empty())next_key=keys.front().first;
    while(monotonic_ms()-begin<std::atoi(argv[4])*1000) {
        const int64_t started=monotonic_ms();host_lifecycle_poll242();
        sched_game_work(false,[] {},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
        sched_present_turn(true,false,false,3,[&] {
            const auto left=16-(monotonic_ms()-started);if(left>0)std::this_thread::sleep_for(std::chrono::milliseconds(left));
            VMS_RS(g_fb_dirty)=false;
        });
        maybe_emit_runtime_stats();
        auto now=monotonic_ms();
        if(held && now>=release){dispatch_key(held,true);held=0;VMS_PLAT(g_key_states)=0;}
        if(!held && now-begin>=next_key) {
            const auto text=VMS_RS(g_drawacc).textblob;
            int key=keys.empty()?53:keys[key_index].second;if(keys.empty() && !music && (text.find("音乐")!=std::string::npos || text.find("音效")!=std::string::npos)){key=-6;music=true;}
            dispatch_key(key,false);held=key;release=now+100;VMS_PLAT(g_key_states)=key==53?256:0;
            if(!keys.empty()){++key_index;next_key=key_index<keys.size()?keys[key_index].first:INT64_MAX;}else next_key=now-begin+1500;log_line("REPLAY key="+std::to_string(key)+" elapsed_ms="+std::to_string(now-begin));
        }
        if(now-window>=1000) {
            std::cout<<std::setprecision(8)<<"{\"elapsed_ms\":"<<now-begin<<",\"draw_fps\":"<<double(VMS_RS(g_game_frames)-seen)*1000/(now-window)
                <<",\"budget_fps\":"<<double(VMS_EXEC(g_speed_budget_frames)-budgets)*1000/(now-window)
                <<",\"scale\":"<<VMS_EXEC(g_game_clock).scale<<",\"jit_entries\":"<<g_jit_region_stats.entries<<",\"jit_bytes\":"<<g_jit_region_stats.bytes<<",\"thread_live\":"<<VMS_EXEC(g_thread_live)<<"}\n"<<std::flush;
            seen=VMS_RS(g_game_frames);budgets=VMS_EXEC(g_speed_budget_frames);window=now;
        }
        if(now-begin>=snapshot_due){write_png(out+"/at-"+std::to_string(snapshot_due/1000)+"s.png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));snapshot_due+=5000;}
        if(capture_pixels>0) {
            size_t at=0;int count=0;const auto& text=VMS_RS(g_drawacc).textblob;
            while((at=text.find("PIX ",at))!=std::string::npos){++count;at+=4;}
            if(count>=capture_pixels){log_line("SCENECAPTURE drawRGB_calls="+std::to_string(count));captured=true;break;}
        }
    }
    if(held)dispatch_key(held,true);
    write_png(out+"/final.png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));
    const bool live=VMS_EXEC(g_thread_live);
    if(std::getenv("J2ME_REPLAY_SAVE") && (!capture_pixels || captured)){host_freeze236(true);if(!sheet_save_instant_state(argv[1]))return 7;host_freeze236(false);}
    lifecycle_destroy();p7::pool().shutdown();log_shutdown();
    return capture_pixels>0 && !captured?8:live?0:4;
}
