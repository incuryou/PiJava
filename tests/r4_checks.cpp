#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static int state_path_calls=0;
static std::string instant_state_path(const std::string&){++state_path_calls;return "build/r4-must-not-write.state";}
#include "../src/instant_state.inc"
static void check(bool ok,const char* reason){if(!ok){std::cerr<<"FAIL R4 "<<reason<<'\n';std::exit(1);}}
static Value field(const char* name,const char* desc="I"){
    auto found=VMS_CL(g_class_index).find("R4Fixture");check(found!=VMS_CL(g_class_index).end(),"loaded-only probe");
    Field* f=find_field(found->second,name,desc);check(f&&f->is_static,"probe field");return VMS_CL(g_classes)[found->second].statics[f->slot];
}
int main(int argc,char** argv){
    check(argc>=2&&r4_configure(argv[1]),"profile");
    log_open((r4::state.dir+"/vm.log").c_str(),true);configure_runtime_diagnostics();
    auto requested=[](const char* key){const char* p=std::getenv(key);return p&&std::string(p)=="1";};
    if(requested("J2ME_JIT"))check(VMS_NATIVE(g_native_jit).enabled,"requested JIT must actually enable");
    if(requested("J2ME_P7_WORKER"))check(p7::pool().enabled(),"requested worker must actually enable");
    if(requested("J2ME_P7_AUDIO"))check(p7audio::g_enabled,"requested audio worker must actually enable");
    if(requested("J2ME_P7_GC"))check(p7gc::g_enabled,"requested GC worker must actually enable");
    if(requested("J2ME_P7_PRESENT"))check(p7present::g_enabled,"requested presentation worker must actually enable");
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_RS(g_fb).assign(240*320*3,0);
    check(sizeof(Value)==16,"Value ABI");
    if(argc==3&&std::string(argv[2])=="block"){launch_class("R4Blocking");r4_run_loop();check(false,"blocked paint must not pass");}
    launch_class("R4Fixture");
    auto& s=r4::state;check(game_current_ms()==s.cfg.epoch_ms&&monotonic_ms()==0,"real Java clock bound at tick zero");
    check(field("explicit").i==int32_t(0xbb20b460u),"actual Java explicit Random seed");
    check(field("dateTime","J").l==s.cfg.epoch_ms&&field("calendarTime","J").l==s.cfg.epoch_ms,"Date Calendar Java clock");
    check(field("stream").i==3&&field("table").i==19&&field("errors").i==0,"actual stream Hashtable exception order");
    check(field("rmsRecords").i==1,"actual RecordStore uses fresh isolated directory");
#ifndef __SWITCH__
    if(argc==3&&std::string(argv[2])=="io"){
        check(std::fclose(r4::state.journal)==0,"close diagnostic sink");r4::state.journal=std::fopen("/dev/full","wb");check(r4::state.journal,"failure sink");r4_tick();check(false,"output I/O failure must not succeed");
    }
#endif
    if(argc==3&&std::string(argv[2])=="stop"){
        check(r4_tick()&&r4_stop()&&!r4_tick()&&s.tick==1,"boundary stop freezes committed progress");p7::pool().shutdown();std::cout<<"PASS R4Stop\n";return 0;
    }
    const int stream=field("costStream","Ljava/io/ByteArrayInputStream;").ref;
    auto* pos=find_field(VMS_HEAP(g_heap)[stream].cls,"pos","I");auto* read=find_method(VMS_HEAP(g_heap)[stream].cls,"read","()I",true);
    Value arg=Value::of_ref(stream),value;unsigned cost=0;auto ops=s.opcodes;
#if J2ME_OPT_LIBRARY_INDEX
    check(r4_shortcut_cost(read,&arg,1,4096,cost,value)&&value.i==3&&cost>1,"bytecode cost admission");
    check(VMS_HEAP(g_heap)[stream].fields[pos->slot].i==0&&s.opcodes==ops,"shadow read leaves actual position and execution fuel unchanged");
    unsigned partial=0;check(!r4_shortcut_cost(read,&arg,1,cost-1,partial,value)&&VMS_HEAP(g_heap)[stream].fields[pos->slot].i==0,"insufficient budget declines without partial commit");
#else
    check(!r4_shortcut_cost(read,&arg,1,4096,cost,value),"unbound library cannot acquire cost certificate");
#endif
    check(!save_instant_state("owned.jar")&&!sheet_save_instant_state("owned.jar")&&!restore_instant_state("owned.jar")&&state_path_calls==0,"test state refusal before path or I/O");
    if(argc==3){
        for(int i=0;i<20;i++)check(r4_tick(),"prepause tick");
        auto before=s.us();auto frames=s.audio_frames;auto ops=s.opcodes;check(r4_pause(true),"pause boundary");
        for(int i=0;i<3;i++)check(!r4_tick(),"paused no advance");
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        check(s.us()==before&&s.audio_frames==frames&&s.opcodes==ops,"harness pause freezes all logical progress");check(r4_pause(false),"resume boundary");
    }
    while(r4_tick()){}
    check(std::none_of(VMS_EXEC(g_threads).begin(),VMS_EXEC(g_threads).end(),[](const JavaThreadContext& t){return t.state==ThreadState::Terminated&&!t.executing;}),"terminated host thread contexts reclaimed at safe boundary");
    check(field("progress").i>20&&field("timers").i==8&&field("pollEnd","J").l>=s.cfg.epoch_ms+17,"workers Timer polling real consumers");
    check(field("slept","J").l>=s.cfg.epoch_ms+5&&field("waited","J").l>=s.cfg.epoch_ms+7,"sleep and monitor wait not early");
    check(field("joined","J").l>=s.cfg.epoch_ms+9&&field("interrupts").i==1&&field("initializedA").i==31&&field("initializedB").i==31,"join, interrupt and contended class initialization");
    check(field("pressed").i==48&&field("released").i==48,"native input callbacks");
    check(field("errors").i==0&&s.audio_frames==uint64_t(s.us())*48000/1000000&&s.captures==uint64_t(s.us())*s.cfg.capture_hz/1000000,"audio sampling remainder/capture/no exception");
    std::cout<<"R4DATA progress="<<field("progress").i<<" checksum="<<field("checksum").i<<" meter="<<field("meterData").i<<" paints="<<field("paints").i<<" slept="<<field("slept","J").l<<" waited="<<field("waited","J").l<<" poll="<<field("pollEnd","J").l<<" random="<<field("defaultRandom").i<<" opcodes="<<s.opcodes<<"\n";
    s.remaining=1000000;s.worker_phase=false;release_game_keys();lifecycle_destroy();p7audio::fence();p7::pool().shutdown();r4::finish("COMPLETE","DIRECT_CHECK");
    std::cout<<"PASS R4 actual Java clock/scheduler/input/Random/Timer/wait/audio/capture/state/pause\n";
}
