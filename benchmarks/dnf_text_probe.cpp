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
    const int cid=ensure_class("j");
    Method* method=find_method(cid,"a","(Ljava/lang/String;II)V",false);
    const Field* graphics=find_field(cid,"a","Ljavax/microedition/lcdui/Graphics;");
    const Field* table=find_field(cid,"e","Ljava/util/Hashtable;");
    int receiver=0, receivers=0;
    for(size_t i=1;i<g_heap.size();++i)if(g_heap[i].cls==cid && graphics && table &&
        g_heap[i].fields[graphics->slot].ref && g_heap[i].fields[table->slot].ref){
        ++receivers;if(!receiver)receiver=int(i);
    }
    if(!receiver||!method){std::cerr<<"no live reviewed DNF font object\n";return 5;}
    // Harness-only fixed-work replay, not gameplay. No bytecode/field patch.
    const std::string text=u8"\u963f\u5c14\u6770 \u54e5\u5e03\u6797 \u653b\u51fb \u6280\u80fd 0123456789";
    ++g_paint_depth;
    auto draw=[&](){int str=make_string(text);run(method,{Value::of_ref(receiver),Value::of_ref(str),Value::of_i(4),Value::of_i(24)});return !g_thrown;};
    if(!draw())return 6;
    const auto start=std::clock();const int64_t wall=paint_clock_us();
    for(int i=0;i<count;++i)if(!draw())return 7;
    double cpu=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;double ms=(paint_clock_us()-wall)/1000.0;--g_paint_depth;
    const auto hash=hash_pixels();
    std::cout<<std::setprecision(10)<<"{\"version\":\""<<kVer<<"\",\"variant\":\""<<argv[5]<<"\",\"method\":\"j.a(String,int,int)\",\"iterations\":"<<count<<",\"process_cpu_ms\":"<<cpu<<",\"wall_ms\":"<<ms<<",\"pixel_digest\":\""<<std::hex<<hash<<"\",\"scope\":\"DNF actual font method fixed text replay; NOT full gameplay or Switch FPS\"}\n";
    return 0;
}
