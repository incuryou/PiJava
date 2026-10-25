// External-JAR bounded functional replay. No private game fields are written.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <set>
#include <map>
#include "../src/presentation/layers.h"
static std::string json_string(const std::string& text){
    std::string out="\"";const char* hex="0123456789abcdef";
    for(unsigned char c:text){if(c=='\"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'\"';
}
int main(int argc,char** argv){
    if(argc!=7){std::cerr<<"jar api_classpath output_dir cap seconds live_input_tape\n";return 2;}
    const std::string jar=argv[1],out=argv[3],tape=argv[6];const int cap=std::atoi(argv[4]),seconds=std::atoi(argv[5]);
    if((cap!=0&&cap!=60)||seconds<30||seconds>90)return 2;
    setenv("J2ME_RMS_DIR",(out+"/rms").c_str(),1);VMS_PLAT(g_boot_ms)=now_ms();log_open((out+"/vm.log").c_str(),true);
    VMS_CL(g_classpath)={argv[2]};VMS_DIAG(g_log_draw)=true;configure_runtime_diagnostics();
    std::vector<uint8_t> bytes;std::string error;if(!read_whole(jar,bytes)||!unzip(bytes,VMS_CL(g_jar_files),error)){std::cerr<<error;return 3;}
    load_props();load_sidecar_jad(jar);auto main_class=manifest_midlet();if(main_class.empty())main_class=find_midlet_class();for(char& c:main_class)if(c=='.')c='/';
    const auto identity=rms_jar_identity(bytes.data(),bytes.size());select_screen_sidecar(jar,identity);
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    VMS_EXEC(g_speed_cap)=cap;speed_sync_clock();const int64_t launch_at=paint_clock_us();launch_class(main_class,identity);
    VMS_EXEC(g_speed_cap)=cap;speed_sync_clock();const int64_t begin=paint_clock_us(),launch_us=begin-launch_at;
    std::ofstream journal(out+"/progress.jsonl");std::string last_text;std::set<int> done,held;
    int active_down_seq=0;int64_t active_return_us=0,active_callback_us=0;
    int64_t last_release_return_us=0,last_release_callback_us=0,active_prior_gap_us=-1;
    int holds_checked=0;bool all_holds_valid=true;
    std::ofstream hold_journal(out+"/input-holds.jsonl");
    int64_t next_record=0;size_t capture=0;const int64_t captures[]={0,5000,10000,15000,30000,45000,60000,75000};
    std::ofstream images(out+"/image-checks.jsonl");int image_count=0;bool all_images_valid=true;
    auto snapshot=[&](const std::string& name){
        const auto& fb=VMS_RS(g_fb);const int w=VMS_RS(g_screen_w),h=VMS_RS(g_screen_h);const auto before=fb;
        constexpr uint32_t sentinel=0xff342619;std::vector<uint32_t> chrome(1280*720,sentinel),cpu=chrome,gpu,rgba;
        const bool valid=j2me_present::game_rgba(fb,w,h,rgba);
        if(valid)p7present::blit_game(cpu.data(),1280,fb.data(),w,h);
        const bool composed=valid&&j2me_present::compose_reference(chrome,&rgba,gpu),equal=composed&&cpu==gpu;
        bool outside=true;for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if((x<400||x>=880||y<40||y>=680)&&cpu[size_t(y)*1280+x]!=sentinel)outside=false;
        const bool immutable=before==fb;all_images_valid&=equal&&outside&&immutable;++image_count;
        std::vector<uint8_t> rgb(cpu.size()*3);for(size_t i=0;i<cpu.size();++i){rgb[i*3]=uint8_t(cpu[i]);rgb[i*3+1]=uint8_t(cpu[i]>>8);rgb[i*3+2]=uint8_t(cpu[i]>>16);}
        if(valid)write_png(out+"/"+name+"-raw.png",fb,w,h);write_png(out+"/"+name+"-host.png",rgb,1280,720);
        const logical_screen::View view(w,h);
        images<<"{\"name\":"<<json_string(name)<<",\"elapsed_ms\":"<<(paint_clock_us()-begin)/1000.0<<",\"cpu_presenter\":\"p7present::blit_game\",\"host_size\":[1280,720],\"fixed_window\":[400,40,480,640],\"fit_rect\":["<<view.x+400<<","<<view.y+40<<","<<view.w<<","<<view.h<<"],\"cpu_gpu_reference_equal\":"<<(equal?"true":"false")<<",\"outside_window_sentinel_unchanged\":"<<(outside?"true":"false")<<",\"game_framebuffer_unchanged\":"<<(immutable?"true":"false")<<",\"physical_gpu_capture\":false,\"background\":\"test sentinel; no host menu chrome\"}\n";images.flush();
    };
    bool pause_attempted=false,pause_stable=false,pause_clock_stable=false,pause_audio=false,resume_active=false;
    auto record=[&](const char* phase,int seq=0,int key=0,bool up=false){
        if(!VMS_RS(g_drawacc).textblob.empty())last_text=VMS_RS(g_drawacc).textblob.substr(0,6000);
        const int w=VMS_RS(g_screen_w),h=VMS_RS(g_screen_h);const auto& fb=VMS_RS(g_fb);const bool consistent=w>0&&h>0&&fb.size()==size_t(w)*size_t(h)*3;
        int minx=w,miny=h,maxx=-1,maxy=-1;
        if(consistent)for(int y=0;y<h;++y)for(int x=0;x<w;++x){const size_t i=(size_t(y)*w+x)*3;if(fb[i]>2||fb[i+1]>2||fb[i+2]>2){minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);}}
        const int canvas=VMS_RS(g_current);const std::string cn=canvas>0?VMS_CL(g_classes)[VMS_HEAP(g_heap)[canvas].cls].name:"";
        journal<<"{\"phase\":"<<json_string(phase)<<",\"elapsed_ms\":"<<(paint_clock_us()-begin)/1000.0<<",\"seq\":"<<seq<<",\"key\":"<<key<<",\"up\":"<<(up?"true":"false")
            <<",\"canvas_class\":"<<json_string(cn)<<",\"logical_width\":"<<w<<",\"logical_height\":"<<h<<",\"forced_size\":false,\"requested_cap\":"<<cap<<",\"active_cap\":"<<VMS_EXEC(g_speed_cap)
            <<",\"clock_scale\":"<<VMS_EXEC(g_game_clock).scale<<",\"draw_completions\":"<<VMS_RS(g_game_frames)<<",\"native_game_sleep_requested_ms\":"<<g_frame_sleep_stats.requested_ms<<",\"effective_sleep_ms\":"<<g_frame_sleep_stats.effective_ms
            <<",\"framebuffer_consistent\":"<<(consistent?"true":"false")<<",\"nonblack_bbox\":["<<minx<<","<<miny<<","<<maxx<<","<<maxy<<"],\"frame_sha256\":"<<json_string(rms_jar_identity(fb.data(),fb.size()))
            <<",\"text_sample\":"<<json_string(last_text)<<",\"midlet_state\":"<<int(VMS_EXEC(g_midlet_state))<<",\"thrown\":"<<(VMS_EXEC(g_thrown)?"true":"false")<<",\"error\":"<<json_string(VMS_EXEC(g_error))<<"}\n";journal.flush();
    };
    record("launched");
    while(paint_clock_us()-begin<seconds*1000000LL&&VMS_EXEC(g_midlet_state)!=MidletState::Destroyed){
        const int64_t turn=paint_clock_us();host_lifecycle_poll242();
        // Bind each release to its own press by seq, including repeated keys.
        // Serialize complete actions; exactly one real dispatch per host turn.
        struct Event{int seq,key,up;int64_t at;};struct Action{Event down,up;};
        std::ifstream input(tape);std::map<int,Event> events;Event event;
        while(input>>event.seq>>event.at>>event.key>>event.up)events.emplace(event.seq,event);
        std::map<int,std::deque<Event>> unmatched;std::vector<Action> actions;
        for(const auto& item:events){const auto& e=item.second;if(!e.up)unmatched[e.key].push_back(e);else if(!unmatched[e.key].empty()){actions.push_back({unmatched[e.key].front(),e});unmatched[e.key].pop_front();}}
        std::sort(actions.begin(),actions.end(),[](const Action& a,const Action& b){return a.down.seq<b.down.seq;});
        for(const auto& action:actions){
            if(done.count(action.up.seq))continue;
            const auto hold_ms=std::max<int64_t>(80,action.up.at-action.down.at);const auto now=paint_clock_us();
            const bool release=done.count(action.down.seq);const Event& e=release?action.up:action.down;
            if(!release){if(active_down_seq||now-begin<e.at*1000||now-begin>=seconds*1000000LL-500000||(last_release_return_us&&now<last_release_return_us+150000))break;}
            else if(active_down_seq!=action.down.seq||now<active_return_us+hold_ms*1000||now-begin<e.at*1000)break;
            done.insert(e.seq);record("before_input",e.seq,e.key,release);const int64_t callback_at=paint_clock_us();dispatch_key(e.key,release);
            if(release){
                const auto actual_us=callback_at-active_callback_us;const bool valid=actual_us>=hold_ms*1000&&(active_prior_gap_us<0||active_prior_gap_us>=150000);all_holds_valid&=valid;++holds_checked;
                hold_journal<<"{\"down_seq\":"<<action.down.seq<<",\"up_seq\":"<<action.up.seq<<",\"key\":"<<e.key<<",\"requested_hold_ms\":"<<hold_ms<<",\"actual_dispatch_start_gap_ms\":"<<actual_us/1000.0<<",\"previous_release_to_this_down_ms\":"<<active_prior_gap_us/1000.0<<",\"valid\":"<<(valid?"true":"false")<<"}\n";hold_journal.flush();held.erase(e.key);active_down_seq=0;last_release_callback_us=callback_at;last_release_return_us=paint_clock_us();
            }else{held.insert(e.key);active_down_seq=e.seq;active_callback_us=callback_at;active_return_us=paint_clock_us();active_prior_gap_us=last_release_callback_us?callback_at-last_release_callback_us:-1;}
            record("after_input",e.seq,e.key,release);break;
        }
        sched_game_work(false,[]{},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
        sched_present_turn(true,false,false,3,[&]{const int64_t left=16000-(paint_clock_us()-turn);if(left>0)std::this_thread::sleep_for(std::chrono::microseconds(left));VMS_RS(g_fb_dirty)=false;});
        if(!VMS_RS(g_drawacc).textblob.empty())last_text=VMS_RS(g_drawacc).textblob.substr(0,6000);
        const int64_t elapsed=(paint_clock_us()-begin)/1000;
        if(elapsed>=next_record){record("observe");next_record=elapsed+1000;}
        if(capture<sizeof(captures)/sizeof(captures[0])&&elapsed>=captures[capture]){
            snapshot("at-"+std::to_string(captures[capture]/1000)+"s");++capture;
        }
        if(!pause_attempted&&elapsed>=39000&&held.empty()&&VMS_EXEC(g_midlet_state)==MidletState::Active){
            pause_attempted=true;record("before_host_freeze");host_freeze236(true);const auto pixels=VMS_RS(g_fb);const auto draws=VMS_RS(g_game_frames);const auto clock=game_current_ms();pause_audio=VMS_PLAT(g_audio).paused();
            for(int i=0;i<10;++i){advance_game_frame();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            pause_stable=pixels==VMS_RS(g_fb)&&draws==VMS_RS(g_game_frames);pause_clock_stable=clock==game_current_ms();host_freeze236(false);resume_active=VMS_EXEC(g_midlet_state)==MidletState::Active;record("after_host_resume");
        }
    }
    record("final");snapshot("final");
    const auto elapsed_us=paint_clock_us()-begin;const bool ended_early=elapsed_us<seconds*1000000LL;const int remaining_exception=VMS_EXEC(g_thrown)?1:0;
    const size_t cleanup_releases=held.size();for(int key:held)dispatch_key(key,true);held.clear();lifecycle_destroy();const bool destroyed=VMS_EXEC(g_midlet_state)==MidletState::Destroyed&&VMS_RS(g_current)==0;
    std::ofstream result(out+"/result.json");result<<"{\"build\":"<<json_string(kVer)<<",\"jar_sha256\":"<<json_string(identity)<<",\"main_class\":"<<json_string(main_class)<<",\"cap\":"<<cap<<",\"seconds_requested\":"<<seconds<<",\"elapsed_ms\":"<<elapsed_us/1000.0<<",\"launch_ms\":"<<launch_us/1000.0<<",\"ended_early\":"<<(ended_early?"true":"false")<<",\"delivered_tape_events\":"<<done.size()<<",\"remaining_exception\":"<<remaining_exception<<",\"pause_attempted\":"<<(pause_attempted?"true":"false")<<",\"freeze_pixels_and_draws_stable\":"<<(pause_stable?"true":"false")<<",\"freeze_clock_stable\":"<<(pause_clock_stable?"true":"false")<<",\"freeze_audio_paused\":"<<(pause_audio?"true":"false")<<",\"resume_active\":"<<(resume_active?"true":"false")<<",\"destroyed\":"<<(destroyed?"true":"false")<<",\"gameplay_visual_review_required\":true,\"physical_switch_test\":false}\n";
    std::ofstream image_result(out+"/image-result.json");image_result<<"{\"snapshot_pairs\":"<<image_count<<",\"all_cpu_gpu_reference_and_bounds_checks\":"<<(all_images_valid?"true":"false")<<"}\n";
    std::ofstream holds_result(out+"/input-result.json");holds_result<<"{\"completed_holds\":"<<holds_checked<<",\"all_actual_holds_at_least_requested\":"<<(all_holds_valid?"true":"false")<<",\"cleanup_release_attempts\":"<<cleanup_releases<<",\"outstanding_after_cleanup\":"<<held.size()<<",\"runtime_pressed_keys_after_destroy\":"<<VMS_PLAT(g_pressed_keys).size()<<"}\n";
    p7::pool().shutdown();log_shutdown();return remaining_exception?5:!all_images_valid?6:!all_holds_valid||cleanup_releases?7:ended_early?4:0;
}
