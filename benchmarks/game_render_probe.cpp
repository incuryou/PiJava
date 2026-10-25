// Replay one actual rendering method from a real JAR + immediate snapshot.
// Deliberately NOT a complete gameplay/FPS benchmark: no input or game tick
// advancement. Method bytecode and game data are unmodified; both VMs must
// produce identical mutable-image pixels from the same starting snapshot.
#include <ctime>
#include <iomanip>
#define main emulator_main
#include PROBE_VM
#undef main
#include PROBE_MENU
#include PROBE_SPEED
static host_menu::Settings g_menu_settings;
static std::string snapshot;
static std::string instant_state_path(const std::string&) {return snapshot;}
#include PROBE_SAVE
static uint64_t hash_pixels() {
    uint64_t hash=14695981039346656037ULL;
    auto add=[&](uint8_t v){hash^=v;hash*=1099511628211ULL;};
    for(auto b:g_fb)add(b);
    for(const auto& obj:g_heap)if(obj.cls>=0 && obj.width && obj.height){for(auto b:obj.pixels)add(b);for(auto b:obj.alpha)add(b);}
    return hash;
}
int main(int argc,char** argv) {
    if(argc!=6)return 2;
    snapshot=argv[2];const int count=std::atoi(argv[3]);if(count<1||count>10000)return 2;
    setenv("J2ME_RMS_DIR",argv[4],1);g_classpath={PROBE_ROM};g_log_draw=false;g_speed_cap=0;g_slice_ms=14;
    std::vector<uint8_t> bytes;std::string err;if(!read_whole(argv[1],bytes)||!unzip(bytes,g_jar_files,err))return 3;
    load_props();std::string midlet=manifest_midlet();if(midlet.empty())midlet=find_midlet_class();for(char& c:midlet)if(c=='.')c='/';
    g_fb.assign(size_t(g_screen_w)*g_screen_h*3,0);const auto identity=rms_jar_identity(bytes.data(),bytes.size());launch_class(midlet,identity);
    if(!restore_instant_state(argv[1],identity))return 4;
    g_speed_cap=0;speed_sync_clock();g_execution_profile.enabled=false;
    const int eclass=ensure_class("e");Method* method=find_method(eclass,"D","()V",false);
    const Field* map=find_field(eclass,"K","Lk;");int receiver=0;
    for(size_t i=1;i<g_heap.size();i++)if(g_heap[i].cls==eclass && map && g_heap[i].fields[map->slot].ref){if(receiver)return 8;receiver=int(i);}
    if(!method||!receiver){std::cerr<<"no unique live e with map K\n";return 5;}
    // D is a rendering subroutine, hence the same no-Java-peer-pump scope as
    // an actual paint callback. This scope exists only in this test harness.
    ++g_paint_depth;run(method,{Value::of_ref(receiver)});if(g_thrown)return 6;
    const auto start=std::clock();const int64_t wall=paint_clock_us();
    for(int i=0;i<count;i++){run(method,{Value::of_ref(receiver)});if(g_thrown)return 7;}
    double cpu=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;double ms=(paint_clock_us()-wall)/1000.0;--g_paint_depth;
    const auto hash=hash_pixels();
    std::cout<<std::setprecision(10)<<"{\"version\":\""<<kVer<<"\",\"variant\":\""<<argv[5]<<"\",\"method\":\"e.D()V\",\"iterations\":"<<count<<",\"process_cpu_ms\":"<<cpu<<",\"wall_ms\":"<<ms<<",\"pixel_digest\":\""<<std::hex<<hash<<"\",\"scope\":\"real game rendering subroutine replay; NOT full gameplay or Switch FPS\"}\n";
    return 0;
}
