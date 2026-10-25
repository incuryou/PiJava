#include <cassert>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
// Harness only. Includes unmodified production VM and shared host work runner.
// Real desktop clocks; no injected Java values, modified JAR, or fake returns.
static int field(const char* c,const char* n,const char* d){
 auto it=VMS_CL(g_class_index).find(c);if(it==VMS_CL(g_class_index).end())return -999;
 auto* f=find_field(it->second,n,d);if(!f||!f->is_static)return -999;
 return VMS_CL(g_classes)[f->owner].statics[f->slot].i;
}
static void snapshot(const char* where,int64_t start){
 std::cout<<"SNAP "<<where<<" ms="<<monotonic_ms()-start<<" ca.a:B="<<field("ca","a","B")
 <<" j.k:Z="<<field("j","k","Z")<<" j.a:Z="<<field("j","a","Z")
 <<" j.a:B="<<field("j","a","B")<<" j.d:B="<<field("j","d","B")
 <<" draw="<<VMS_DIAG(g_p1).draw_id<<" live="<<VMS_EXEC(g_thread_live)<<" thrown="<<VMS_EXEC(g_thrown)
 <<" text="<<VMS_RS(g_drawacc).textblob<<"\n";
 if(std::strcmp(where,"pre_key")==0){
   for(const auto& t:VMS_EXEC(g_threads)){
     const Frame* f=t.frames.empty()?nullptr:&t.frames.back();
     std::cout<<"PREKEY_WORKER ref="<<t.ref<<" state="<<int(t.state)<<" executing="<<t.executing
       <<" monitor="<<t.monitor<<" join="<<t.join_target<<" frames="<<t.frames.size()
       <<" top="<<(f?VMS_CL(g_classes)[f->method->class_id].name+"."+f->method->name+f->method->desc:"-")
       <<" bci="<<(f?f->op_pc:-1)<<"\n";
   }
   for(const auto& kv:VMS_EXEC(g_monitors))if(kv.second.owner)
     std::cout<<"PREKEY_MONITOR ref="<<kv.first<<" owner_ref="<<kv.second.owner->ref<<" depth="<<kv.second.depth<<"\n";
 }
}
int main(int argc,char**argv){
 assert(argc==5);std::cout<<std::unitbuf;VMS_PLAT(g_boot_ms)=now_ms();VMS_DIAG(g_log_draw)=true;
 VMS_PLAT(g_log).open(argv[3]);setenv("J2ME_P1_PROFILE",argv[2],1);configure_runtime_diagnostics();
 VMS_CL(g_classpath)={"romfs/classes"};VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
 std::vector<uint8_t> jar;std::string error;assert(read_whole(argv[1],jar));
 auto id=rms_jar_identity(jar.data(),jar.size());
 assert(id=="563f753243eefeeef25f392f1cb13c22725ad3cdf019a232c6314064cf8897ab");
 assert(unzip(jar,VMS_CL(g_jar_files),error));load_props();assert(manifest_midlet()=="Main");
 int64_t start=monotonic_ms();std::cout<<"BEGIN production_launch\n";
 launch_class("Main",id);snapshot("launch_done",start);
 struct Event{int at,code;bool release;};
 // Relative to the original 12262ms launch-button timestamp in 224756.log.
 std::vector<Event> events={{649,53,true},{11737,-6,false},{11784,-6,true},
 {15902,53,false},{15960,53,true},{16558,53,false},{16613,53,true},
 {18205,53,false},{18260,53,true},{23000,53,false},{23100,53,true}};
 size_t e=0;int64_t last_report=-1000,last_present=0;unsigned long long turns=0;
 int duration=std::atoi(argv[4]);
 while(monotonic_ms()-start<duration){
   host_lifecycle_poll242();int64_t elapsed=monotonic_ms()-start;
   if(e<events.size()&&elapsed>=events[e].at){
     auto ev=events[e];snapshot("pre_key",start);write_png(std::string(argv[3])+"-key"+std::to_string(e)+".png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));
     std::cout<<"KEY_BEGIN event="<<e<<" target_ms="<<ev.at<<" code="<<ev.code<<" release="<<ev.release<<"\n";VMS_PLAT(g_log).flush();
     dispatch_key(ev.code,ev.release);
     std::cout<<"KEY_END event="<<e++<<"\n";snapshot("post_key",start);VMS_PLAT(g_log).flush();
   }
   bool heartbeat=last_present&&paint_clock_us()-last_present>=50000;
   p1_host_work_turn(false,heartbeat,[](bool){scheduled_frame([](){},game_wake_delay,
      [](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));},advance_game_frame,
      VMS_EXEC(g_frame_wait_budget),paint_clock_us,&VMS_EXEC(g_wake_stats));},
      [](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
   if(p1::should_show(true,false,false,VMS_RS(g_fb_dirty),3,heartbeat,speed_group_incomplete())){
      p1_host_post_present(true,false);last_present=paint_clock_us();
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
   }
   ++turns;
   if(elapsed-last_report>=1000){snapshot("host_turn",start);last_report=elapsed;VMS_PLAT(g_log).flush();}
   if(VMS_EXEC(g_midlet_state)==MidletState::Destroyed)break;
 }
 snapshot("finish",start);write_png(std::string(argv[3])+"-finish.png",VMS_RS(g_fb),VMS_RS(g_screen_w),VMS_RS(g_screen_h));
 std::cout<<"FINISH turns="<<turns<<" events="<<e<<"\n";p1_report();lifecycle_destroy();VMS_PLAT(g_log).close();
}
