#define main emulator_main
#ifndef M1_VM
#define M1_VM "../src/j2mevm.cpp"
#endif
#include M1_VM
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/micro3d-owned.j2s"; }
#include "../src/instant_state.inc"
static void check(bool b,const char* what){if(!b){std::cerr<<"FAIL M1 "<<what<<" thrown="<<VMS_EXEC(g_thrown)<<'\n';std::exit(1);}}
static int call(const char* name){auto* m=find_method(ensure_class("Micro3DFoundationChecks"),name,"()I",false);check(m,"method");int result=run(m,{}).i;check(!VMS_EXEC(g_thrown),name);return result;}
int main(int argc,char**){
    configure_runtime_diagnostics();VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    if(argc==2){check(call("baseline")==0,"baseline missing foundation");std::cout<<"BASELINE_REPRODUCED missing Vector3D\n";return 17;}
    check(sizeof(Value)==16,"Value ABI");int n=call("run"),sig=call("signature");check(n>300,"all numeric assertions reached");
    std::cout<<"M1DATA assertions="<<n<<" signature="<<sig<<'\n';
    launch_class("Micro3DFoundationChecks","owned-micro3d-id");gc_collect("M1 live graph");check(call("verify")==57,"Java graph survives GC");
    check(sheet_save_instant_state("owned-micro3d.jar"),"J2S5 save");check(call("mutate")==1&&call("verify")==0,"live graph changed");
    check(restore_instant_state("owned-micro3d.jar","owned-micro3d-id"),"J2S5 restore");gc_collect("M1 restored graph");check(call("verify")==57,"restored field and alias graph");
    check(call("release")==1&&call("discard")==1,"release and batch");gc_collect("M1 no native roots");
    int leaked=0;for(size_t i=1;i<VMS_HEAP(g_heap).size();i++)if(gc_slot_live(int(i))&&VMS_HEAP(g_heap)[i].cls>=0&&VMS_CL(g_classes)[VMS_HEAP(g_heap)[i].cls].name.find("com/mascotcapsule/micro3d/v3/")==0)leaked++;
    check(leaked==0,"unreferenced foundation objects reclaimed");std::cout<<"PASS Micro3D foundation GC=1 J2S5=1 no_hidden_roots=1\n";stop_thread();p7::pool().shutdown();
}
