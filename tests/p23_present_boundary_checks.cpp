// P2.3 acceptance (observation only): with the C08 trace on, every present is classified at a presentable boundary,
// and the observation does not change Java-visible results (serial FIFO, sync repaint, worker, releases, paint count).
#include <cassert>
#include <iostream>
#include <string>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){ ++fails; std::cout<<"CHECK FAIL "<<msg<<"\n"; } else std::cout<<"CHECK ok "<<msg<<"\n"; }while(0)
static int fixture;
static int value(const char* n){auto* f=find_field(fixture,n,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static int invoke(const char* n,const char* d="()I",std::vector<Value> a={}){auto* m=find_method(fixture,n,d,false);assert(m);int r=run(m,a).i;assert(!VMS_EXEC(g_thrown));return r;}
static std::string scenario(int mode,bool worker){
    launch_class("P1HostChecks");fixture=ensure_class("P1HostChecks");
    VMS_EXEC(g_speed_cap)=0;speed_sync_clock();
    assert(invoke("setup","(I)I",{Value::of_i(mode)})==1);
    if(worker)assert(invoke("startWorker")==1);
    for(int i=0;i<200&&(value("calls")<6||(worker&&VMS_EXEC(g_thread_live)));++i){
        host_lifecycle_poll242();advance_game_frame();
        if(VMS_RS(g_fb_dirty))p1_host_post_present(true,false);
    }
    return "mode="+std::to_string(mode)+" worker="+std::to_string(worker)+" calls="+std::to_string(value("calls"))+" order="+std::to_string(value("order"))+
        " workers="+std::to_string(value("workers"))+" afterService="+std::to_string(value("afterService"))+" syncPairs="+std::to_string(value("syncPairs"))+" releases="+std::to_string(value("releases"));
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    VMS_EXEC(g_completed_paint241).enabled=true;
    std::string off,on;
    s0.on=false; for(auto mw:{std::make_pair(0,false),std::make_pair(2,true)}) off+=scenario(mw.first,mw.second)+"\n";
    s0.on=true;  for(auto mw:{std::make_pair(0,false),std::make_pair(2,true)}) on+=scenario(mw.first,mw.second)+"\n";
    std::cout<<"OFF\n"<<off<<"ON\n"<<on;
    CHECK(off==on,"trace on/off: identical Java-visible results (P2.3 is observation only)");
    CHECK(p23.presents>0&&p23.presents==s0.present_id,"every present observed ("+std::to_string(p23.presents)+")");
    CHECK(p23.unsafe==0,"no present inside a paint or Java run (unsafe=0)");
    CHECK(p23.safe_new+p23.dup==p23.presents,"each present is either a new completed surface or a duplicate");
    CHECK(p23.last_consumed==s0.surf_ver||p23.last_consumed+0==s0.presented_ver,"consumed id = last completed surface version presented");
    CHECK(p23.safe_new+p23.superseded_versions<=s0.surf_ver,"presented + superseded versions never exceed completed versions");
    CHECK(p23.services[0]>0,"host input/lifecycle service observed");
    p23_report();
    std::cout<<"P23_PRESENT_BOUNDARY_RESULT "<<(fails?"FAIL":"PASS")<<" fails="<<fails<<" presents="<<p23.presents<<" safe_new="<<p23.safe_new<<" dup="<<p23.dup
             <<" superseded="<<p23.superseded_versions<<" surf_ver="<<s0.surf_ver<<"\n";
    return fails?1:0;
}
