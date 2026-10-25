// P2.1-c4 acceptance condition (added in c5): REAL Java threads switch through pump_one_thread(); the four
// per-thread members (running_thread, yield, thrown, active_stack) must be each thread's own.
#include <cassert>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static int fixture, fails;
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
static int value(const char* name){ auto* f=find_field(fixture,name,"I");assert(f&&f->is_static); return VMS_CL(g_classes)[f->owner].statics[f->slot].i; }
static int log_at(int i){ auto* f=find_field(fixture,"log","[I");assert(f); int arr=VMS_CL(g_classes)[f->owner].statics[f->slot].ref; return VMS_HEAP(g_heap)[arr].elems.int_data()[i]; }
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};
    VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();fixture=ensure_class("ThreadSliceChecks");
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    auto* m=find_method(fixture,"start","(I)I",false);assert(m);
    CHECK(run(m,{Value::of_i(0)}).i==1 && !VMS_EXEC(g_thrown),"two Java threads started");
    // outside any Java slice: what the scheduler must restore after every switch
    auto* rt0=VMS_EXEC(g_running_thread); auto* st0=VMS_EXEC(g_active_stack);
    bool restored=true, distinct_stacks=true, both_parked=false; int turns=0, a_ref=0, b_ref=0;
    for(;turns<200 && (value("doneA")==0||value("doneB")==0);++turns){
        if(!pump_one_thread())break;
        restored &= VMS_EXEC(g_running_thread)==rt0 && VMS_EXEC(g_active_stack)==st0 && !VMS_EXEC(g_yield) && !VMS_EXEC(g_thrown);
        // two live workers parked mid-call: each has its own, separate frame stack
        std::vector<JavaThreadContext*> live; for(auto& t:VMS_EXEC(g_threads)) if(!t.frames.empty()) live.push_back(&t);
        if(live.size()==2){ both_parked=true; distinct_stacks &= &live[0]->frames!=&live[1]->frames && live[0]->frames.size()!=live[1]->frames.size();
            if(!a_ref){a_ref=live[0]->ref;b_ref=live[1]->ref;} }
    }
    CHECK(&VMS_EXEC(g_running_thread)==&j2me_vmstate::g_vm_storage.exec.ctx.g_running_thread,"running_thread lives in exec.ctx");
    std::printf("INFO turns=%d log=",turns); for(int i=0;i<value("n");++i)std::printf("%d ",log_at(i)); std::printf("\n");
    CHECK(value("doneA")==1&&value("doneB")==1,"both threads ran to completion");
    CHECK(value("errors")==0,"inside each thread: own Thread identity, own catch/exception state, own deep locals after every resume");
    CHECK(value("resultA")==1000+21 && value("resultB")==2000+6,"deep recursion results (active_stack is the thread's own)");
    CHECK(value("caughtA")==3 && value("caughtB")==3,"each thread caught its own 3 exceptions while yielding inside catch");
    bool interleaved=false; for(int i=0;i+1<value("n");++i) if(log_at(i)/10!=log_at(i+1)/10) interleaved=true;
    int switches=0; for(int i=0;i+1<value("n");++i) if(log_at(i)/10!=log_at(i+1)/10) ++switches;
    std::printf("INFO thread_switches_in_log=%d\n",switches);
    CHECK(interleaved && switches>=4,"the other thread really ran between a yield and the resume");
    CHECK(both_parked && distinct_stacks,"two threads parked at once with separate frame stacks of different depth");
    CHECK(restored,"after every slice the scheduler restored running_thread/yield/thrown/active_stack");
    stop_thread();
    std::printf("THREAD_SLICE_REAL_RESULT %s fails=%d\n",fails?"FAIL":"PASS",fails); return fails?1:0;
}
