// P2.2-S5 acceptance (PX-2 doc 04 row S5, A2 under GC stress): RootScope covers native temporaries across
// native->Java re-entry. Test-only hook forces a FULL GC at EVERY run() entry (including nested re-entry at
// run_depth>0, which production never does), then checks:
//  - Java-visible results identical to the unforced run (serial FIFO, sync repaint, worker, releases),
//  - no RootScope-registered object was freed or its slot reused while in scope (stale_at_close==0),
//  - stream_from (bytes held across ensure_class + <init>) survives forced GC, contents intact,
//  - lifecycle reset clears scopes abandoned by a longjmp (residue) and a new suite starts with none open,
//  - negative control: with registration disabled the same forced GC frees the native temporary (detector works).
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
static bool g_force_gc=false; static unsigned long long g_forced=0, g_forced_nested=0;
static void test_reentry_gc();
#define J2ME_TEST_REENTRY_GC_HOOK() test_reentry_gc()
#define J2ME_TEST_ROOTSCOPE_VERIFY 1
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static void test_reentry_gc(){ if(!g_force_gc)return; ++g_forced; if(VMS_EXEC(g_run_depth)>0)++g_forced_nested; gc_collect("s5-test-forced"); gc_verify_frames("forced"); } // P2.4: verify active frames after every forced GC
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){ ++fails; std::cout<<"CHECK FAIL "<<msg<<"\n"; } else std::cout<<"CHECK ok "<<msg<<"\n"; }while(0)
static int fixture;
static int value(const char* n){auto* f=find_field(fixture,n,"I");assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].i;}
static int invoke(const char* n,const char* d="()I",std::vector<Value> a={}){auto* m=find_method(fixture,n,d,false);assert(m);int r=run(m,a).i;assert(!VMS_EXEC(g_thrown));return r;}
static std::string scenario(int mode,bool worker){
    launch_class("P1HostChecks");fixture=ensure_class("P1HostChecks");
    VMS_EXEC(g_speed_cap)=0;speed_sync_clock();
    CHECK(g_native_roots.entries.empty(),"new suite starts with no open RootScope");
    assert(invoke("setup","(I)I",{Value::of_i(mode)})==1);
    if(worker)assert(invoke("startWorker")==1);
    for(int i=0;i<200&&(value("calls")<6||(worker&&VMS_EXEC(g_thread_live)));++i){
        host_lifecycle_poll242();advance_game_frame();
        if(VMS_RS(g_fb_dirty))p1_host_post_present(true,false);
        assert(VMS_RS(g_paint_depth)==0&&VMS_EXEC(g_run_depth)==0&&!VMS_EXEC(g_thrown));
        if(!g_native_roots.entries.empty()){CHECK(false,"RootScope open at host safepoint");break;}
    }
    return "mode="+std::to_string(mode)+" worker="+std::to_string(worker)+" calls="+std::to_string(value("calls"))+" order="+std::to_string(value("order"))+
        " workers="+std::to_string(value("workers"))+" afterService="+std::to_string(value("afterService"))+
        " syncPairs="+std::to_string(value("syncPairs"))+" releases="+std::to_string(value("releases"));
}
static bool stream_ok(int ref,const std::vector<uint8_t>& data){
    if(ref<=0||VMS_HEAP(g_heap)[ref].cls!=ensure_class("java/io/ByteArrayInputStream"))return false;
    auto* f=find_field(VMS_HEAP(g_heap)[ref].cls,"buf","[B"); if(!f)return false;
    const int buf=VMS_HEAP(g_heap)[ref].fields[f->slot].ref;
    if(buf<=0||!VMS_HEAP(g_heap)[buf].is_array||VMS_HEAP(g_heap)[buf].elems.size()!=data.size())return false;
    return std::memcmp(VMS_HEAP(g_heap)[buf].elems.byte_data(),data.data(),data.size())==0;
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    VMS_EXEC(g_completed_paint241).enabled=true;
    std::string base,forced;
    for(auto mw:{std::make_pair(0,false),std::make_pair(2,true)}) base+=scenario(mw.first,mw.second)+"\n";
    g_force_gc=true; const auto stale0=g_native_roots.stale_at_close;
    for(auto mw:{std::make_pair(0,false),std::make_pair(2,true)}) forced+=scenario(mw.first,mw.second)+"\n";
    std::cout<<"BASE\n"<<base<<"FORCED\n"<<forced;
    CHECK(base==forced,"forced GC at every run() entry gives identical Java-visible results");
    CHECK(g_forced>0&&g_forced_nested>0,"forced GCs happened, including nested re-entry (forced="+std::to_string(g_forced)+" nested="+std::to_string(g_forced_nested)+")");
    // stream_from: bytes is a native temporary across ensure_class and BAIS.<init>; churn the heap so freed slots are reused.
    std::vector<uint8_t> data(4096); for(size_t i=0;i<data.size();++i)data[i]=uint8_t(i*7+3);
    int ok_streams=0;
    for(int k=0;k<20;++k){ for(int j=0;j<50;++j)alloc_array("[I",16); int s=stream_from(data); ok_streams+=stream_ok(s,data); }
    CHECK(ok_streams==20,"stream_from survives forced GC with slot reuse, contents intact ("+std::to_string(ok_streams)+"/20)");
    CHECK(g_native_roots.stale_at_close==stale0,"no RootScope object freed/reused while in scope (stale_at_close="+std::to_string(g_native_roots.stale_at_close)+")");
    // P2.4: class-initialization failures (ExceptionInInitializerError wrap holds the in-flight exception across
    // ensure_class) under the same forced GC; active frames verified after every collection.
    { const auto wraps0=g_native_roots.opened_at_depth[size_t(ReentrySite::ThrowWrap)];
      int c=ensure_class("ClassInitChecks"); Method* m=find_method(c,"run","()I",false);
      const int r=m?run(m,{}).i:-1; const bool thrown=VMS_EXEC(g_thrown)!=0; VMS_EXEC(g_thrown)=0; current_context().unhandled=0;
      CHECK(m&&!thrown&&r>0,"ClassInitChecks.run under forced GC (result="+std::to_string(r)+")");
      CHECK(g_native_roots.opened_at_depth[size_t(ReentrySite::ThrowWrap)]>wraps0,"initializer-failure wrap path exercised with RootScope (throw_wrap scopes="+std::to_string(g_native_roots.opened_at_depth[size_t(ReentrySite::ThrowWrap)]-wraps0)+")");
      CHECK(g_native_roots.stale_at_close==stale0,"no registered object freed during initializer-failure wraps"); }
    CHECK(g_gc_verify.runs>0&&g_gc_verify.dead==0,"active frames verified after every forced GC: runs="+std::to_string(g_gc_verify.runs)+" refs="+std::to_string(g_gc_verify.refs)+" dead="+std::to_string(g_gc_verify.dead)+" first="+(g_gc_verify.first_dead.empty()?std::string("-"):g_gc_verify.first_dead));
    // Residue (scope abandoned by longjmp): simulate an entry left behind, then a lifecycle reset must clear it.
    static int leaked=0; g_native_roots.entries.push_back({&leaked,nullptr,-1,false,0,0}); const auto res0=g_native_roots.residue_cleared;
    g_force_gc=false; launch_class("P1HostChecks");
    CHECK(g_native_roots.entries.empty()&&g_native_roots.residue_cleared==res0+1,"lifecycle reset clears abandoned RootScope entries");
    // Negative control: registration disabled -> the same forced GC must break stream_from (proves the detector).
    // Runs in a forked child: an unrooted, freed temporary may crash it (a crash also counts as "detected").
    pid_t pid=fork();
    if(pid==0){ g_force_gc=true; g_native_roots.test_disable_registration=true;
        int broken=0; for(int k=0;k<5;++k){ for(int j=0;j<50;++j)alloc_array("[I",16); broken+=!stream_ok(stream_from(data),data); }
        _exit(broken>0?42:0); }
    int st=0; waitpid(pid,&st,0);
    const bool detected=(WIFEXITED(st)&&WEXITSTATUS(st)==42)||WIFSIGNALED(st);
    CHECK(detected,std::string("negative control: without RootScope the forced GC breaks stream_from (child ")+(WIFSIGNALED(st)?"crashed sig="+std::to_string(WTERMSIG(st)):"exit="+std::to_string(WEXITSTATUS(st)))+")");
    native_roots_report(); gc_safepoint_report();
    std::cout<<"ROOTSCOPE_REENTRY_RESULT "<<(fails?"FAIL":"PASS")<<" fails="<<fails<<" forced_gc="<<g_forced<<" nested="<<g_forced_nested<<" scopes="<<g_native_roots.scopes<<"\n";
    return fails?1:0;
}
