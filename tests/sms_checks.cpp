#define main emulator_main
#ifndef R5_VM
#define R5_VM "../src/j2mevm.cpp"
#endif
#include R5_VM
#undef main
static void check(bool b,const char* s){if(!b){std::cerr<<"FAIL SMS "<<s<<'\n';std::exit(1);}}
static int call(const char* name){auto* m=find_method(ensure_class("SmsOfflineChecks"),name,"()I",false);check(m,"Java method");return run(m,{}).i;}
int main(int argc,char** argv){
    configure_runtime_diagnostics();
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    check(sizeof(Value)==16,"Value ABI");int n=call("run");check(n==16,"all 16 Java assertions reached");
    check(call("keep")==1,"owned roots setup");gc_collect("SMS connection/container liveness");check(call("verify")==5,"live Java fields keep payload");check(call("release")==1&&call("discard")==1,"release and batch");gc_collect("SMS no hidden transport roots");
    int leaked=0;for(size_t i=1;i<VMS_HEAP(g_heap).size();i++)if(gc_slot_live(int(i))&&VMS_HEAP(g_heap)[i].cls>=0&&VMS_CL(g_classes)[VMS_HEAP(g_heap)[i].cls].name.find("javax/wireless/messaging/OfflineSmsConnection")==0)leaked++;
    check(leaked==0,"unreferenced connections/messages reclaimed; no global queue/root");std::cout<<"PASS SmsAlwaysSentChecks assertions="<<n<<" no_hidden_roots=1\n";p7::pool().shutdown();
}
