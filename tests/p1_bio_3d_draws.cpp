#include <cassert>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
// P2.1-c6: Bio 3D-scene draw-count harness (derived from p1_bio_smoke). Runs J2ME_BIO_TURNS turns (default 900),
// pulses FIRE every 40 turns to walk menus into the 3D scene, then prints completed draws + M3G work counters.
// Acceptance: the same numbers on c5 and c6 (RenderSurface migration must not change what is drawn).
// Exact user fixture, shared production dispatch, host I/O simulated.
// This verifies queue advancement only. No scene/level reachability verdict.
int main(int argc,char** argv){
    assert(argc==4);VMS_PLAT(g_boot_ms)=now_ms();VMS_DIAG(g_log_draw)=false;
    VMS_PLAT(g_log).open(argv[3]);setenv("J2ME_P1_PROFILE",argv[2],1);configure_runtime_diagnostics();
    VMS_CL(g_classpath)={"romfs/classes"};VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    std::vector<uint8_t> jar;std::string error;assert(read_whole(argv[1],jar));
    const auto identity=rms_jar_identity(jar.data(),jar.size());
    assert(identity=="95f4e836247c5d0f00c7056c59cd80d57e0706e1a5159f4f39cb9656597f37bd");
    assert(unzip(jar,VMS_CL(g_jar_files),error));load_props();assert(manifest_midlet()=="BioMain");
    launch_class("BioMain",identity);
    const uint64_t before=VMS_DIAG(g_p1).serial_dispatches;unsigned sleeps=0;
    const int turns=getenv("J2ME_BIO_TURNS")?atoi(getenv("J2ME_BIO_TURNS")):900;
    // J2ME_BIO_KEYS="turn:code,turn:code,..." legal key presses (press at turn, release next turn); empty = FIRE every 40.
    std::vector<std::pair<int,int>> script; if(const char* ks=getenv("J2ME_BIO_KEYS")){ std::string s=ks; size_t p=0;
        while(p<s.size()){ size_t c=s.find(',',p); if(c==std::string::npos)c=s.size(); auto t=s.substr(p,c-p); auto m=t.find(':');
            if(m!=std::string::npos)script.push_back({atoi(t.c_str()),atoi(t.c_str()+m+1)}); p=c+1; } }
    const int hold=getenv("J2ME_BIO_HOLD")?atoi(getenv("J2ME_BIO_HOLD")):1;
    const int snap=getenv("J2ME_BIO_SNAP")?atoi(getenv("J2ME_BIO_SNAP")):0;
    for(int i=0;i<turns;++i){
        host_lifecycle_poll242();
        if(script.empty()){ if(i%40==12)dispatch_key(-5,false); if(i%40==13)dispatch_key(-5,true); }
        else for(auto& k:script){ if(k.first==i)dispatch_key(k.second,false); if(k.first+hold==i)dispatch_key(k.second,true); }
        if(snap>0&&i%snap==0)write_png(std::string(argv[3])+"."+std::to_string(i)+".png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));
        if(i%100==0){auto q=p1_m3g_values();std::cout<<"TURN "<<i<<" draws="<<VMS_DIAG(g_p1).draw_id<<" span="<<q.span<<" row="<<q.row<<" gathered="<<q.gathered<<"\n";}
        p1_host_work_turn(false,false,[](bool){advance_game_frame();},[&](int64_t ms){++sleeps;std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
        if(p1::should_show(true,false,false,VMS_RS(g_fb_dirty),3,false,speed_group_incomplete()))p1_host_post_present(true,false);
        if(VMS_EXEC(g_notify_destroyed)||VMS_EXEC(g_midlet_state)==MidletState::Destroyed)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    assert(VMS_DIAG(g_p1).serial_dispatches>before);assert(VMS_DIAG(g_p1).start_outcome==0);
    assert(VMS_DIAG(g_p1).reason_counts[0]>=1&&VMS_DIAG(g_p1).reason_counts[1]==0);
    auto mv=p1_m3g_values();std::cout<<"BIO3D gathered="<<mv.gathered<<" emitted="<<mv.emitted<<" span="<<mv.span<<" row="<<mv.row<<" texture="<<mv.texture<<" identity="<<mv.identity<<"\n";
    std::cout<<"PASS exact_Bio serial_dispatches="<<VMS_DIAG(g_p1).serial_dispatches<<" normal_worker_returns="<<VMS_DIAG(g_p1).reason_counts[0]<<" main_task="<<p1_bio_field("m_MainTask")<<" load_task="<<p1_bio_field("m_LoadTask")<<" completed_draws="<<VMS_DIAG(g_p1).draw_id<<" idle_sleeps="<<sleeps<<" scope=shared_dispatch_desktop no_device_play_verdict=1\n";
    p1_report();write_png(std::string(argv[3])+".png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));lifecycle_destroy();VMS_PLAT(g_log).close();
}
