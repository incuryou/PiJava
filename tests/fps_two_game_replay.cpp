#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <iomanip>
// Serial desktop A/B smoke replay using the production shared scheduler.
// argv: JAR, absolute API classpath, output directory, duration seconds.
int main(int argc,char** argv) {
    if(argc!=5)return 2;
    const std::string out=argv[3];VMS_PLAT(g_boot_ms)=now_ms();log_open((out+"/vm.log").c_str(),true);
    VMS_CL(g_classpath)={argv[2]};VMS_DIAG(g_log_draw)=false;configure_runtime_diagnostics();
    if(std::getenv("J2ME_TEST_FULL_PRIOR_SUITE")){g_jit_region_stats.bytes=JitRegionMaxBytes;g_jit_region_stats.compiled=490;}
    std::vector<uint8_t> bytes;std::string error;
    if(!read_whole(argv[1],bytes) || !unzip(bytes,VMS_CL(g_jar_files),error))return 3;
    load_props();std::string midlet=manifest_midlet();for(char& c:midlet)if(c=='.')c='/';
    select_screen_sidecar(argv[1]);VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    launch_class(midlet,rms_jar_identity(bytes.data(),bytes.size()));VMS_EXEC(g_speed_cap)=60;speed_sync_clock();
    const int64_t begin=monotonic_ms();int64_t next_key=2500,release=0,window=begin;int held=0;
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
    }
    if(held)dispatch_key(held,true);
    write_png(out+"/final.png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));
    const bool live=VMS_EXEC(g_thread_live);lifecycle_destroy();p7::pool().shutdown();log_shutdown();
    return live?0:4;
}
