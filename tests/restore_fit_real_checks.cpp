// Host-only, owned legacy-default repro snapshots from real selected JARs.
// No user snapshot or game JAR is modified; no Switch hardware is exercised.
#define main emulator_main
#include PROBE_VM
#undef main
#include PROBE_MENU
#include PROBE_SPEED
static host_menu::Settings g_menu_settings;
static std::string snapshot_file;
static std::string instant_state_path(const std::string&) { return snapshot_file; }
#include PROBE_SAVE
static void check(bool ok,const char* message){if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
static void shot(const std::string& path){
    logical_screen::View view(VMS_RS(g_screen_w),VMS_RS(g_screen_h));std::ofstream out(path,std::ios::binary);out<<"P6\n480 640\n255\n";
    for(int y=0;y<640;++y)for(int x=0;x<480;++x){auto rgb=view.sample(VMS_RS(g_fb).data(),x,y);char p[]={char(rgb),char(rgb>>8),char(rgb>>16)};out.write(p,3);}
}
static void frames(int milliseconds){const int64_t until=paint_clock_us()+int64_t(milliseconds)*1000;while(paint_clock_us()<until){advance_game_frame();VMS_RS(g_fb_dirty)=false;}}
static bool same(const SavValue& a,const SavValue& b){return a.i==b.i&&a.ref==b.ref&&a.l==b.l&&a.is_long==b.is_long;}
int main(int argc,char** argv){
    check(argc==5,"mode jar snapshot output-prefix");const std::string mode=argv[1],jar=argv[2],prefix=argv[4];snapshot_file=argv[3];
    setenv("J2ME_RMS_DIR",(prefix+"-rms").c_str(),1);VMS_CL(g_classpath)={PROBE_ROM};VMS_PLAT(g_boot_ms)=now_ms();VMS_PLAT(g_log).open(prefix+".log");VMS_DIAG(g_log_draw)=false;VMS_EXEC(g_speed_cap)=60;VMS_EXEC(g_slice_ms)=14;
    std::vector<uint8_t> jar_bytes;std::string error;check(read_whole(jar,jar_bytes)&&unzip(jar_bytes,VMS_CL(g_jar_files),error),"load real JAR");load_props();
    const auto identity=rms_jar_identity(jar_bytes.data(),jar_bytes.size());std::string midlet=manifest_midlet();if(midlet.empty())midlet=find_midlet_class();for(char& c:midlet)if(c=='.')c='/';
    VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;VMS_RS(g_fb).assign(240*320*3,0);launch_class(midlet,identity);
    if(mode=="legacy"){
        const char* wait=std::getenv("RESTORE_REPRO_WAIT_MS");const int wait_ms=wait?std::max(0,std::min(15000,std::atoi(wait))):2000;
        frames(wait_ms);dispatch_key(-5,false);dispatch_key(-5,true);frames(1000);check(VMS_RS(g_current)>0,"real game has active screen");
        check(save_instant_state(jar),"generate owned old-default J2S5");shot(prefix+".ppm");
        std::vector<uint8_t> input;check(read_whole(snapshot_file,input),"read generated J2S5");
        std::cout<<"{\"mode\":\"legacy_default_repro\",\"jar_sha256\":\""<<identity<<"\",\"snapshot_sha256\":\""<<rms_jar_identity(input.data(),input.size())<<"\",\"width\":"<<VMS_RS(g_screen_w)<<",\"height\":"<<VMS_RS(g_screen_h)<<",\"heap\":"<<VMS_HEAP(g_heap).size()<<"}\n";
    }else{
        std::vector<uint8_t> input;check(read_whole(snapshot_file,input),"read old-default repro snapshot");const auto before=rms_jar_identity(input.data(),input.size());
        SavState old;check(decode_state(std::string(input.begin(),input.end()),old),"decode old-default repro");check(screen_meta(old.heap[old.current].text).w==240&&screen_meta(old.heap[old.current].text).h==320,"repro snapshot has legacy-default dimensions");
        select_screen_sidecar(jar,identity);const int expected_w=VMS_RS(g_screen_w),expected_h=VMS_RS(g_screen_h);check(expected_w!=240||expected_h!=320,"real JAR has trusted nondefault profile");
        check(restore_instant_state(jar,identity),"restore real old-default repro");check(VMS_RS(g_screen_w)==expected_w&&VMS_RS(g_screen_h)==expected_h,"real snapshot migrates to trusted dimensions");
        SavState live;check(capture_state(live,jar),"capture migrated real game");check(live.heap.size()==old.heap.size(),"migration preserves object count");
        std::unordered_set<int> buffers;for(auto kv:old.offscreen)buffers.insert(kv.second);
        size_t fields=0,arrays=0;for(size_t r=0;r<old.heap.size();++r){const auto& a=old.heap[r];const auto& b=live.heap[r];check(a.cls==b.cls&&a.is_array==b.is_array&&a.array_desc==b.array_desc,"game object identity/type preserved");
            check(a.array_data==b.array_data&&a.array_length==b.array_length,"Java array data/progress preserved");if(a.is_array)++arrays;
            check(a.fields.size()==b.fields.size(),"game field count preserved");for(size_t f=0;f<a.fields.size();++f){if(buffers.count(int(r)))continue;check(same(a.fields[f],b.fields[f]),"Java object fields/progress preserved");++fields;}
            if(!buffers.count(int(r)))check(a.pixels==b.pixels&&a.alpha==b.alpha,"game-owned image pixels preserved");
        }
        for(int y=0;y<std::min(320,VMS_RS(g_screen_h));++y)check(std::memcmp(VMS_RS(g_fb).data()+size_t(y)*VMS_RS(g_screen_w)*3,old.fb.data()+size_t(y)*240*3,size_t(std::min(240,VMS_RS(g_screen_w)))*3)==0,"real framebuffer overlap retained");
        shot(prefix+"-immediate.ppm");host_events(false);frames(1500);shot(prefix+".ppm");
        check(read_whole(snapshot_file,input)&&rms_jar_identity(input.data(),input.size())==before,"real repro snapshot SHA256 unchanged");
        std::cout<<"{\"mode\":\"migrated_real_jar_repro\",\"jar_sha256\":\""<<identity<<"\",\"snapshot_sha256_before\":\""<<before<<"\",\"snapshot_sha256_after\":\""<<rms_jar_identity(input.data(),input.size())<<"\",\"width\":"<<VMS_RS(g_screen_w)<<",\"height\":"<<VMS_RS(g_screen_h)<<",\"heap\":"<<live.heap.size()<<",\"unchanged_java_fields\":"<<fields<<",\"unchanged_java_arrays\":"<<arrays<<",\"snapshot_unmodified\":true,\"hardware_tested\":false}\n";
    }
    p7::pool().shutdown();
}
