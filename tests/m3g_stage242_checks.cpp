#include <cassert>
#include <ctime>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static std::vector<int> array(int r){assert(r>0);std::vector<int> a;for(size_t i=0;i<VMS_HEAP(g_heap)[r].elems.size();i++)a.push_back(VMS_HEAP(g_heap)[r].elems.get(i).i);return a;}
static uint64_t digest(const std::vector<int>& a,uint64_t h){for(int v:a)for(int s=0;s<32;s+=8){h^=(uint32_t(v)>>s)&255;h*=1099511628211ull;}return h;}
#include "m3g_geometry242_checks.inc"
int main(int argc,char** argv){
    std::cout<<std::unitbuf;int cases=argc>1?std::atoi(argv[1]):128;bool legacy=argc>2&&std::string(argv[2])=="legacy";int start=argc>3?std::atoi(argv[3]):0;
    VMS_CL(g_classpath)={"build/test-classes",legacy?"build/legacy241-classes":"romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    int cid=ensure_class("javax/microedition/m3g/M3GStage242Checks");
    for(const char* name:{"transform","pose","scene","inverse","treeFallback","assembly","customPose"}){uint64_t hash=1469598103934665603ull,words=0;double cpu[2]={};
        for(int seed=start;seed<start+cases;++seed){std::vector<int> states[2];
            for(int p=0;p<(legacy?1:2);++p){int mode=legacy?0:(seed+p)%2;VMS_RS(g_m3g_mesh242_enabled)=mode!=0;
                std::clock_t begin=std::clock();int r=run(find_method(cid,name,"(I)[I",false),{Value::of_i(seed)}).ref;
                cpu[mode]+=1000.0*(std::clock()-begin)/CLOCKS_PER_SEC;states[mode]=array(r);gc_collect("stage242-test");}
            if(!legacy&&states[0]!=states[1]){std::cerr<<"FAIL "<<name<<" seed="<<seed<<" size="<<states[0].size()<<","<<states[1].size()<<"\n";
                for(size_t i=0;i<std::min(states[0].size(),states[1].size());++i)if(states[0][i]!=states[1][i]){std::cerr<<"word="<<i<<" ref="<<std::hex<<uint32_t(states[0][i])<<" got="<<uint32_t(states[1][i])<<std::dec<<"\n";break;}return 1;}
            words+=states[0].size();hash=digest(states[0],hash);
        }
        std::cout<<"PASS "<<name<<" cases="<<cases<<" start="<<start<<" words="<<words<<" hash="<<std::hex<<hash<<std::dec<<" ref_cpu_ms="<<cpu[0]<<" native_cpu_ms="<<cpu[1]<<"\n";
    }
    if(!lighting242Checks(cid,cases*8,start*8,legacy))return 1;
    if(!legacy)declines242(cid);
    for(int i=0;i<2;++i){VMS_RS(g_m3g_mesh242_enabled)=i!=0;assert(run(find_method(cid,"errors","()I",false),{}).i==31);}
    std::cout<<"PASS public_exceptions_and_disconnected_tree=5\n";m3g_mesh242::report();m3g_lighting242::report();m3g_assembly242::report();
}
