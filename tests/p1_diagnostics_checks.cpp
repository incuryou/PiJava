#include <cassert>
#include <cstring>
#include <sstream>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

static void thread_launch(int mode){
    VMS_CL(g_props)["p1.test.mode"]=std::to_string(mode);
    launch_class("P1ThreadEndingChecks","p1-diagnostics-thread");
}
static volatile uint64_t overhead_sink=0;
static void check_last32(){
    assert(VMS_DIAG(g_p1).caught_count==40&&VMS_DIAG(g_p1).caught_retained==32&&VMS_DIAG(g_p1).caught_overwritten==8);
    const auto first=(VMS_DIAG(g_p1).caught_next+32-VMS_DIAG(g_p1).caught_retained)%32;
    for(size_t i=0;i<32;++i){
        const std::string line=VMS_DIAG(g_p1).caught_lines[(first+i)%32].data();
        assert(line.find(" seq="+std::to_string(i+9)+" ")!=std::string::npos);
        assert(line.find("throw_owner=P1ThreadEndingChecks$Task")!=std::string::npos);
    }
}
static void check_dump_order(const std::string& text,size_t dumps){
    size_t at=0;
    for(size_t dump=0;dump<dumps;++dump){
        at=text.find("P1CAUGHTSET ",at);assert(at!=std::string::npos);
        for(size_t seq=9;seq<=40;++seq){
            at=text.find("P1CAUGHT schema=1 ",at);assert(at!=std::string::npos);
            const auto end=text.find('\n',at);
            assert(text.substr(at,end-at).find(" seq="+std::to_string(seq)+" ")!=std::string::npos);
            at=end;
        }
    }
    assert(text.find("P1CAUGHT schema=1 ",at)==std::string::npos);
}
static void caught_rolling_checks(){
    std::ostringstream trace;auto* previous=std::cerr.rdbuf(trace.rdbuf());
    thread_launch(6);assert(!VMS_EXEC(g_thread_live));check_last32();
    assert(VMS_DIAG(g_p1).caught_dumps==1);stop_thread();assert(VMS_DIAG(g_p1).caught_dumps==2);
    stop_thread();assert(VMS_DIAG(g_p1).caught_dumps==2);
    std::cerr.rdbuf(previous);check_dump_order(trace.str(),2);std::cerr<<trace.str();

    trace.str("");trace.clear();previous=std::cerr.rdbuf(trace.rdbuf());
    thread_launch(7);assert(VMS_EXEC(g_thread_live));check_last32();assert(VMS_DIAG(g_p1).caught_dumps==0);
    auto& t=VMS_EXEC(g_threads).front();const auto heap=VMS_HEAP(g_heap).size(),free=VMS_HEAP(g_free).size();const int allocated=VMS_HEAP(g_alloc_since);
    const auto saved=VMS_DIAG(g_p1);
    p1_caught(0,t.frames.back(),0,t.frames.back().method,t.frames.back().op_pc,t.frames.size());
    assert(VMS_HEAP(g_heap).size()==heap&&VMS_HEAP(g_free).size()==free&&VMS_HEAP(g_alloc_since)==allocated);
    assert(VMS_DIAG(g_p1).caught_count==41&&VMS_DIAG(g_p1).caught_retained==32&&VMS_DIAG(g_p1).caught_overwritten==9);
    VMS_DIAG(g_p1)=saved;const auto copied=VMS_DIAG(g_p1).caught_lines;
    stop_thread();assert(VMS_DIAG(g_p1).caught_dumps==1);check_last32();
    assert(VMS_DIAG(g_p1).caught_lines==copied&&VMS_EXEC(g_threads).empty());
    stop_thread();assert(VMS_DIAG(g_p1).caught_dumps==1);
    std::cerr.rdbuf(previous);check_dump_order(trace.str(),1);std::cerr<<trace.str();

    trace.str("");trace.clear();previous=std::cerr.rdbuf(trace.rdbuf());
    thread_launch(8);assert(VMS_EXEC(g_thread_live)&&VMS_DIAG(g_p1).caught_count==0);
    while(VMS_DIAG(g_p1).events<P1Diagnostics::Limit){
        if(VMS_DIAG(g_p1).queued==P1Diagnostics::Ring)p1_flush("test");p1_event("ordinary before catches");
    }
    VMS_EXEC(g_threads).front().wake=monotonic_ms();
    const auto until=monotonic_ms()+1000;
    while(VMS_EXEC(g_thread_live)&&monotonic_ms()<until)thread_resume();
    assert(!VMS_EXEC(g_thread_live));p1_flush("host_turn");check_last32();assert(VMS_DIAG(g_p1).caught_dumps==1);
    stop_thread();assert(VMS_DIAG(g_p1).caught_dumps==2&&VMS_DIAG(g_p1).dropped>0);
    std::cerr.rdbuf(previous);check_dump_order(trace.str(),2);std::cerr<<trace.str();

    VMS_DIAG(g_p1).enabled=false;thread_launch(6);assert(!VMS_EXEC(g_thread_live)&&VMS_DIAG(g_p1).caught_count==40);
    assert(VMS_DIAG(g_p1).caught_retained==0&&VMS_DIAG(g_p1).caught_overwritten==0&&VMS_DIAG(g_p1).caught_dumps==0);
    VMS_DIAG(g_p1).enabled=true;stop_thread();
    // A different launch releases prior methods/heap; copied trace owns bytes.
    launch_class("P1ThreadEndingOtherChecks","p1-caught-other");
    for(const auto& line:copied)assert(std::strstr(line.data(),"throw_owner=P1ThreadEndingChecks$Task"));
    std::cout<<"PASS rolling last32 catches seq9..40 zero/reset order cap-independent no Java allocations disabled capture copied bytes survive relaunch\n";
}
static int64_t measure(bool enabled){
    VMS_DIAG(g_p1).enabled=enabled;
    JavaThreadContext t;Frame f;f.op_pc=19;
    const int64_t began=paint_clock_us();
    for(unsigned i=0;i<100000;++i){
        overhead_sink=overhead_sink*1664525u+i+1013904223u;
        p1_snapshot(t,f,1);p1_event("toy index=%u",i);
    }
    return paint_clock_us()-began;
}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};
    VMS_PLAT(g_boot_ms)=now_ms();VMS_DIAG(g_log_draw)=false;VMS_EXEC(g_speed_cap)=0;VMS_DIAG(g_p1).enabled=true;
    thread_launch(2);
    assert(!VMS_EXEC(g_thread_live)&&VMS_DIAG(g_p1).reason_counts[1]==1);
    const std::string uncaught=VMS_DIAG(g_p1).last_end.data();
    assert(uncaught.find("throw_owner=P1ThreadEndingChecks$Task")!=std::string::npos);
    assert(uncaught.find("throw_method=run")!=std::string::npos);
    assert(uncaught.find("throw_bci=-1")==std::string::npos);
    assert(uncaught.find("throw_frames=2")!=std::string::npos);
    std::cout<<"PASS actual uncaught retains originating Task.run metadata\n";

    thread_launch(4);assert(VMS_EXEC(g_thread_live)&&!VMS_EXEC(g_threads).front().frames.empty());
    auto& t=VMS_EXEC(g_threads).front();const auto heap=VMS_HEAP(g_heap).size(),free=VMS_HEAP(g_free).size();
    const int allocated=VMS_HEAP(g_alloc_since);
    p1_snapshot(t,t.frames.back(),t.frames.size());p1_event("allocation-check");
    assert(VMS_HEAP(g_heap).size()==heap&&VMS_HEAP(g_free).size()==free&&VMS_HEAP(g_alloc_since)==allocated);
    // Flush native stderr only; there is no SD/file sink in this test.
    while(VMS_DIAG(g_p1).events<P1Diagnostics::Limit){
        if(VMS_DIAG(g_p1).queued==P1Diagnostics::Ring)p1_flush("test");
        p1_event("bounded ordinary event");
    }
    p1_flush("test");const auto dropped=VMS_DIAG(g_p1).dropped;
    p1_event("event beyond finite cap");assert(VMS_DIAG(g_p1).dropped==dropped+1);
    stop_thread();
    assert(VMS_DIAG(g_p1).reason_counts[2]==1&&VMS_DIAG(g_p1).zero_transitions==1);
    assert(std::string(VMS_DIAG(g_p1).last_end.data()).find("reason=2")!=std::string::npos);
    assert(VMS_DIAG(g_p1).events==P1Diagnostics::Limit+1&&VMS_DIAG(g_p1).dropped>=dropped+2);
    p1_report();
    assert(std::string(VMS_DIAG(g_p1).last_end.data()).find("reason=2")!=std::string::npos);
    std::cout<<"PASS cap512 first-zero reserved reset counters and last_end retained no Java allocation\n";

    VMS_DIAG(g_p1)=P1Diagnostics{};
    for(size_t i=0;i<P1Diagnostics::Ring;++i)p1_event("ring-fill %zu",i);
    p1_event("ring-overflow");assert(VMS_DIAG(g_p1).queued==64&&VMS_DIAG(g_p1).dropped==1);
    p1_event("first_live_to_zero reserved-ring-check");
    assert(VMS_DIAG(g_p1).queued==64&&VMS_DIAG(g_p1).dropped==2);
    assert(std::strstr(VMS_DIAG(g_p1).lines[63].data(),"first_live_to_zero"));
    std::cout<<"PASS ring64 retains one critical event with visible drops\n";
    p1_flush("test");

    caught_rolling_checks();

    launch_class("P1HostChecks","p1-diagnostics-canvas");
    assert(VMS_RS(g_current)>0);VMS_RS(g_repaint)=false;
    Method* repaint=find_method(VMS_HEAP(g_heap)[VMS_RS(g_current)].cls,"repaint","()V",true);
    const auto requests=VMS_DIAG(g_p1).request_id,paints=VMS_DIAG(g_p1).paint_count;
    const auto coalesced=VMS_DIAG(g_p1).coalesced_requests;
    assert(VMS_DIAG(g_p1).pending_first_id==0);
    run(repaint,{Value::of_ref(VMS_RS(g_current))});run(repaint,{Value::of_ref(VMS_RS(g_current))});
    assert(VMS_DIAG(g_p1).request_id==requests+2&&VMS_RS(g_repaint));
    assert(VMS_DIAG(g_p1).pending_first_id==requests+1);
    const auto highest=VMS_DIAG(g_p1).request_id;VMS_RS(g_repaint)=false;repaint_current();
    assert(VMS_DIAG(g_p1).paint_count==paints+1&&VMS_DIAG(g_p1).start_id==highest&&VMS_DIAG(g_p1).completed_id==highest);
    assert(VMS_DIAG(g_p1).started_first_id==requests+1&&VMS_DIAG(g_p1).completed_first_id==requests+1);
    assert(VMS_DIAG(g_p1).pending_first_id==0&&VMS_DIAG(g_p1).coalesced_requests==coalesced+1);
    const auto dup=VMS_DIAG(g_p1).duplicate_present;assert(VMS_RS(g_fb_dirty));
    const auto presents=VMS_DIAG(g_p1).present_count;
    const auto presented_first=VMS_DIAG(g_p1).presented_first_id,presented_latest=VMS_DIAG(g_p1).present_id;
    p1_host_post_present(false,false);
    assert(VMS_DIAG(g_p1).present_count==presents&&VMS_DIAG(g_p1).presented_first_id==presented_first&&VMS_DIAG(g_p1).present_id==presented_latest);
    p1_host_post_present(true,true);
    assert(VMS_DIAG(g_p1).present_count==presents&&VMS_DIAG(g_p1).presented_first_id==presented_first&&VMS_DIAG(g_p1).present_id==presented_latest);
    p1_host_post_present(true,false);
    assert(VMS_DIAG(g_p1).present_count==presents+1&&VMS_DIAG(g_p1).present_id==highest&&VMS_DIAG(g_p1).duplicate_present==dup);
    assert(VMS_DIAG(g_p1).presented_first_id==requests+1);
    p1_present();assert(VMS_DIAG(g_p1).duplicate_present==dup+1);
    const auto draw=VMS_DIAG(g_p1).draw_id,canvas=VMS_DIAG(g_p1).completed_id;
    draw_end("p1-plain-raw-draw");assert(VMS_DIAG(g_p1).draw_id==draw+1&&VMS_DIAG(g_p1).completed_id==canvas);
    p1_present();assert(VMS_DIAG(g_p1).duplicate_present==dup+1&&VMS_DIAG(g_p1).present_draw_id==draw+1);
    std::cout<<"PASS actual Canvas inclusive coalesced range highest IDs duplicate presents raw draw distinct menu/sheet excluded\n";

    VMS_DIAG(g_p1)=P1Diagnostics{};VMS_DIAG(g_p1).launch_id=99;
    for(int64_t us:{0,1000,1001,2048001})p1_paint_complete(us);
    uint64_t sum=0;for(auto n:VMS_DIAG(g_p1).hist)sum+=n;
    assert(sum==4&&VMS_DIAG(g_p1).paint_count==4&&VMS_DIAG(g_p1).paint_sum==2050002&&VMS_DIAG(g_p1).paint_max==2048001&&VMS_DIAG(g_p1).hist[12]==1);
    p1_report();std::cout<<"PASS histogram count sum max overflow consistent\n";
    for(int pair=0;pair<3;++pair){
        const auto off=measure(false),on=measure(true);
        std::cout<<"OVERHEAD scope=desktop_not_device profile=same_native_cpu_jit_off pair="<<pair
                 <<" iterations=100000 off_us="<<off<<" on_us="<<on
                 <<" volatile_optimization_prevented=1 SD_writes=0 ring_flushes=0 cap=512\n";
    }
    VMS_DIAG(g_p1).enabled=true;
}
