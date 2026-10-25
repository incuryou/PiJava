#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

static int checks=0;
static void check(bool ok,const char* reason) {
    ++checks;
    if(!ok) {std::cerr<<"FAIL FrameScheduleChecks "<<reason<<"\n";std::exit(1);}
}
static int fixture() {return ensure_class("FrameScheduleChecks");}
static int field(const char* name) {
    int c=fixture();return VMS_CL(g_classes)[c].statics[find_field(c,name,"I")->slot].i;
}
static int reference(const char* name) {
    int c=fixture();return VMS_CL(g_classes)[c].statics[find_field(c,name,"Ljava/lang/Thread;")->slot].ref;
}
static void invoke(const char* name) {run(find_method(fixture(),name,"()I",false),{});}
static JavaThreadContext& worker(const char* name="sleeper") {
    auto* context=thread_context(reference(name));
    check(context!=nullptr,"fixture worker has a live Java context");return *context;
}
static void launch_sleeping() {
    VMS_EXEC(g_frame_wait_budget).observe(16000,500);
    launch_class("FrameScheduleChecks");
    check(!VMS_EXEC(g_frame_wait_budget).samples && VMS_EXEC(g_frame_wait_budget).cap_ms()==2,
          "launch discards old game's scheduling cost history");
    advance_game_frame();
    invoke("startSleeper");advance_game_frame();
    check(field("ticks")==1 && worker().state==ThreadState::Sleeping,
          "actual Java worker parks in Thread.sleep before the frame boundary");
    VMS_EXEC(g_wake_stats)={};
}

static void callback_order() {
    for(int64_t remaining:{-1,0,1,2,3,16,33,60000}) {
        std::string order;int acquires=0,delays=0,waits=0,advances=0;
        int64_t requested=0;
        scheduled_frame([&]{++acquires;order+='A';},
            [&]{++delays;order+='D';return remaining;},
            [&](int64_t ms){++waits;requested=ms;order+='W';},
            [&]{++advances;order+='J';});
        bool near=remaining>0 && remaining<=2;
        check(order==(near?"ADWJ":"ADJ"),"slot acquire precedes deadline read, optional wait, and Java work");
        check(acquires==1 && delays==1 && waits==(near?1:0) && advances==1,
              "each frame has one deadline read and one worker phase without polling or spinning");
        check(!near || requested==remaining,"near wait preserves the exact one or two millisecond delay");
    }
    int waits=0,advances=0;
    scheduled_frame([]{},[]{return int64_t(1);},[&](int64_t){++waits;},[&]{++advances;},0);
    check(waits==0 && advances==1,"zero wait budget keeps the GPU path free of optional waits");
    scheduled_frame([]{},[]{return int64_t(2);},[&](int64_t){++waits;},[&]{++advances;},1);
    check(waits==0 && advances==2,"one millisecond budget rejects a two millisecond deadline");
    int64_t remaining=1;
    scheduled_frame([]{},[&]{return remaining;},[&](int64_t){++waits;remaining=1;},[&]{++advances;});
    check(waits==1 && advances==3,"one wait advances even if a fake clock still reports a near deadline");
}

static void stats_checks() {
    WakeStats stats;stats.ignore_before_ms=100;
    stats.dispatched(0,200);stats.dispatched(99,200);
    check(stats.sleep_wakes==0,"missing deadlines and sleeps spanning a host pause are excluded");
    stats.dispatched(100,107);stats.dispatched(120,120);stats.dispatched(130,129);
    check(stats.sleep_wakes==3 && stats.wake_late_ms==7 && stats.wake_late_max_ms==7,
          "dispatch lateness sums actual due deadlines and clamps early dispatch to zero");
    check(stats.wake_late_3ms_count==1,"dispatches at least three milliseconds late have a separate counter");
    stats.waited(3700);stats.waited(1100);stats.waited(-1);
    check(stats.near_waits==3 && stats.near_wait_us==4800 && stats.near_wait_max_us==3700,
          "wait statistics retain overshoot above the requested two millisecond budget");
    stats.budget_decision(5,6);stats.budget_decision(4,4);stats.budget_decision(0,0);
    check(stats.deferred_wakes==1 && stats.extended_waits==1 && stats.budget_samples==3 &&
          stats.budget_cap_min_ms==0 && stats.budget_cap_max_ms==5,
          "budget telemetry distinguishes deferred and extended waits and includes zero caps");
    WakeStats old=stats.take();
    check(old.sleep_wakes==3 && old.near_waits==3 && old.ignore_before_ms==100 &&
          !stats.sleep_wakes && !stats.near_waits && stats.ignore_before_ms==100,
          "taking a stats window clears counters and preserves the resume cutoff");
    check(old.wake_late_3ms_count==1 && old.deferred_wakes==1 && old.extended_waits==1 &&
          old.budget_samples==3 && !stats.wake_late_3ms_count && !stats.deferred_wakes &&
          !stats.extended_waits && !stats.budget_samples && !stats.budget_cap_max_ms,
          "all budget and missed-deadline window counters reset together");
}

static void actual_frame_order() {
    launch_sleeping();
    int ticks=field("ticks"),paints=field("paints"),slices=VMS_EXEC(g_slices);
    auto acquire=[] {worker().wake=monotonic_ms()-7;};
    // Reproduce the old work-before-blocking-acquire sequence without wall sleeps.
    advance_game_frame();acquire();
    check(field("ticks")==ticks && field("paints")==paints,
          "old frame order misses a Java deadline elapsed while acquiring its display slot");
    int acquire_count=0,wait_count=0;
    FrameWaitBudget budget;budget.observe(8000,500);
    scheduled_frame([&]{++acquire_count;acquire();},game_wake_delay,
        [&](int64_t){++wait_count;},advance_game_frame,budget,paint_clock_us,&VMS_EXEC(g_wake_stats));
    // This is the mocked submission boundary: actual Canvas paint must be complete.
    check(field("ticks")==ticks+1 && field("paintedTicks")==ticks+1 && field("paints")>paints,
          "new frame order resumes actual sleeping Java and paints its update before submission");
    check(acquire_count==1 && wait_count==0 && VMS_EXEC(g_slices)==slices+2,
          "expired wake uses a runnable fast path and exactly one worker phase per frame");
    check(worker().state==ThreadState::Sleeping && worker().wake>monotonic_ms(),
          "same-cycle resume preserves the worker's next long Java sleep");
    check(VMS_EXEC(g_wake_stats).sleep_wakes==1 && VMS_EXEC(g_wake_stats).wake_late_ms>=7,
          "one expired Java sleep contributes one dispatch lateness sample");

    // Refresh alone does not record latency: the worker may remain undispatched.
    VMS_EXEC(g_wake_stats)={};worker().wake=monotonic_ms()-5;
    check(game_wake_delay()==0 && worker().sleep_sample_due>0 && !VMS_EXEC(g_wake_stats).sleep_wakes,
          "deadline refresh saves the due time without prematurely counting a dispatch");
    worker().sleep_sample_due=monotonic_ms()-23;
    advance_game_frame();
    check(VMS_EXEC(g_wake_stats).sleep_wakes==1 && VMS_EXEC(g_wake_stats).wake_late_ms>=23,
          "sleep lateness is sampled when actual Java dispatch occurs");
}

static void actual_budget_wait() {
    launch_sleeping();
    FrameWaitBudget budget;budget.observe(8000,500);
    const int ticks=field("ticks"),paints=field("paints"),slices=VMS_EXEC(g_slices);
    int waits=0;int64_t due=0;
    scheduled_frame([&]{worker().wake=due=monotonic_ms()+4;},
        // Keep the simulated four-millisecond deadline independent of host
        // preemption, while still checking the actual Java timer is preserved.
        [&]{check(worker().wake==due && worker().state==ThreadState::Sleeping,
                  "budget calculation leaves actual Thread.sleep timer untouched");return int64_t(4);},
        [&](int64_t ms){++waits;check(ms==4 && worker().wake==due,
                  "extended bridge requests the full deadline without shortening Java sleep");
            worker().wake=monotonic_ms()-1;},
        advance_game_frame,budget,[]{return int64_t(0);},&VMS_EXEC(g_wake_stats));
    check(waits==1 && field("ticks")==ticks+1 && field("paintedTicks")==ticks+1 && field("paints")>paints,
          "extended deadline bridge resumes and paints actual Java before submission");
    check(VMS_EXEC(g_slices)==slices+1 && VMS_EXEC(g_wake_stats).extended_waits==1 && VMS_EXEC(g_wake_stats).sleep_wakes==1,
          "extended bridge executes exactly one worker slice and one actual sleep dispatch");
    check(VMS_EXEC(g_midlet_state)==MidletState::Active && VMS_EXEC(g_frame_work_finished_us)>0 && VMS_EXEC(g_frame_work_us)>=0,
          "active MIDlet fixture retains a complete advance timing sample");
    check(worker().state==ThreadState::Sleeping && worker().wake>monotonic_ms(),
          "extended bridge preserves the worker's next real long sleep");
    budget.observe(16000,500);waits=0;
    const int after_ticks=field("ticks");
    due=worker().wake=monotonic_ms()+60000;
    scheduled_frame([]{},game_wake_delay,[&](int64_t){++waits;},
        advance_game_frame,budget,[]{return int64_t(0);},&VMS_EXEC(g_wake_stats));
    check(!waits && field("ticks")==after_ticks && worker().wake==due && VMS_EXEC(g_wake_stats).deferred_wakes==1,
          "high-cost slot defers actual sleeping Java without changing its deadline");
}

static void delays_and_waits() {
    launch_sleeping();
    check(game_wake_delay()>2,"a distant Java sleep is not a near wait");
    for(int64_t distant:{16,33,60000}) {
        int waits=0;int64_t deadline=0;
        scheduled_frame([&]{deadline=monotonic_ms()+distant;worker().wake=deadline;},
            game_wake_delay,[&](int64_t){++waits;},[&]{
                check(worker().state==ThreadState::Sleeping && worker().wake==deadline,
                      "16ms, 33ms and long deadlines remain exact before Java advance");
            });
        check(!waits,"distant Java deadlines incur no optional frame wait");
    }
    worker().wake=monotonic_ms()+60000;
    invoke("startJoiner");
    check(game_wake_delay()==0,"a newly runnable peer bypasses a distant sleeping deadline");
    advance_game_frame();
    check(worker("joiner").state==ThreadState::Joining,"actual Java peer parks in timed Thread.join");
    worker("joiner").wake=monotonic_ms()-1;VMS_EXEC(g_wake_stats)={};
    scheduled_frame([]{},game_wake_delay,[](int64_t){},advance_game_frame);
    check(field("joins")==1 && !VMS_EXEC(g_wake_stats).sleep_wakes,
          "timed join resumes without contaminating Thread.sleep lateness");
    invoke("startWaiter");advance_game_frame();
    check(worker("waiter").state==ThreadState::Waiting,"actual Java worker releases its monitor in timed wait");
    worker("waiter").wake=monotonic_ms()-1;VMS_EXEC(g_wake_stats)={};
    scheduled_frame([]{},game_wake_delay,[](int64_t){},advance_game_frame);
    check(field("waits")==1 && !VMS_EXEC(g_wake_stats).sleep_wakes,
          "timed Object.wait reacquires its monitor without a sleep lateness sample");
    invoke("interruptSleeper");VMS_EXEC(g_wake_stats)={};advance_game_frame();
    check(field("interrupts")==1 && !VMS_EXEC(g_thread_live) && !VMS_EXEC(g_wake_stats).sleep_wakes,
          "interrupted Thread.sleep wakes by exception and is excluded from timer lateness");
    check(game_wake_delay()==0,"no live Java timers uses the immediate frame path");
    VMS_EXEC(g_wake_stats)={};wait_game_wake(1);
    check(VMS_EXEC(g_wake_stats).near_waits==1 && VMS_EXEC(g_wake_stats).near_wait_us>0,
          "real optional wait reports elapsed host wait time");
}

static void lifecycle_gates() {
    launch_sleeping();
    FrameWaitBudget budget;budget.observe(8000,500);
    int ticks=field("ticks"),paints=field("paints"),waits=0;
    VMS_EXEC(g_frame_wait_budget).observe(16000,500);
    worker().wake=monotonic_ms()-10;lifecycle_pause(true);
    scheduled_frame([]{},game_wake_delay,[&](int64_t){++waits;},advance_game_frame,budget,paint_clock_us,&VMS_EXEC(g_wake_stats));
    check(!waits && field("ticks")==ticks && field("paints")==paints && !VMS_EXEC(g_wake_stats).sleep_wakes,
          "host pause skips deadline waits, Java execution, paints and lateness accounting");
    lifecycle_resume();
    check(!VMS_EXEC(g_frame_wait_budget).samples && !VMS_EXEC(g_frame_work_us) && !VMS_EXEC(g_frame_work_finished_us),
          "resume clears stale cost history and pending timing sample before Java resumes");
    advance_game_frame();
    check(field("ticks")==ticks+1 && !VMS_EXEC(g_wake_stats).sleep_wakes,
          "resume dispatches the overdue worker but excludes latency accumulated during host pause");
    invoke("selfPause");host_events();paints=field("paints");ticks=field("ticks");
    worker().wake=monotonic_ms()-1;advance_game_frame();
    check(VMS_EXEC(g_midlet_state)==MidletState::Paused && !VMS_PLAT(g_host_paused) && field("ticks")==ticks+1 && field("paints")==paints,
          "self pause keeps background workers available for resumeRequest while suppressing paint");
    VMS_EXEC(g_frame_wait_budget).observe(16000,500);
    lifecycle_destroy();ticks=field("ticks");paints=field("paints");
    check(!VMS_EXEC(g_frame_wait_budget).samples && !VMS_EXEC(g_frame_work_us) && !VMS_EXEC(g_frame_work_finished_us),
          "destroy clears recent scheduling costs and pending host sample");
    scheduled_frame([]{},game_wake_delay,[&](int64_t){++waits;},advance_game_frame,budget,paint_clock_us,&VMS_EXEC(g_wake_stats));
    check(!waits && field("ticks")==ticks && field("paints")==paints && !VMS_EXEC(g_thread_live),
          "destroyed MIDlet executes neither a frame wait nor Java work or repaint");
    launch_sleeping();ticks=field("ticks");paints=field("paints");
    VMS_EXEC(g_notify_destroyed)=true;worker().wake=monotonic_ms()-1;
    scheduled_frame([]{},game_wake_delay,[&](int64_t){++waits;},advance_game_frame,budget,paint_clock_us,&VMS_EXEC(g_wake_stats));
    check(!waits && field("ticks")==ticks && field("paints")==paints && VMS_EXEC(g_midlet_state)==MidletState::Destroyed,
          "pending notifyDestroyed reaches cleanup before any worker or paint callback");
    check(!VMS_EXEC(g_frame_wait_budget).samples && !VMS_EXEC(g_frame_work_us) && !VMS_EXEC(g_frame_work_finished_us),
          "notifyDestroyed handled inside advance cannot restore a stopped game's pending timing sample");
    check(!VMS_EXEC(g_run_depth) && !VMS_EXEC(g_pump_depth) && !VMS_RS(g_paint_depth) && !VMS_EXEC(g_active_stack) && VMS_HEAP(g_root_stacks).empty(),
          "frame and lifecycle callbacks leave interpreter scopes balanced");
}

int main(int argc,char** argv) {
    VMS_CL(g_classpath)={argc>1?argv[1]:"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    callback_order();stats_checks();actual_frame_order();actual_budget_wait();delays_and_waits();lifecycle_gates();
    std::cout<<"PASS FrameScheduleChecks "<<checks<<" acquire order, Java deadlines, lifecycle and wake statistics checks\n";
}
