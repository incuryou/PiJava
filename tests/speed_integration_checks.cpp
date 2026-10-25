#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cmath>

static int checks;
static void check(bool ok,const char* why) {
    ++checks;
    if(!ok){std::cerr<<"FAIL SpeedIntegrationChecks "<<why<<"\n";std::exit(1);}
}
static void reset() {
    stop_thread();VMS_EXEC(g_speed_cap)=60;VMS_EXEC(g_speed_slept_us)=VMS_EXEC(g_speed_cut_us)=VMS_EXEC(g_speed_add_us)=0;
    VMS_RS(g_fb_dirty)=VMS_RS(g_repaint)=false;VMS_EXEC(g_serial_calls).clear();
    VMS_EXEC(g_midlet_state)=MidletState::Active;
}
int main() {
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    reset();
    VMS_EXEC(g_threads).emplace_back();auto& sleeping=VMS_EXEC(g_threads).back();
    sleeping.state=ThreadState::Sleeping;sleeping.wake_us=paint_clock_us()+12000;
    sleeping.wake=monotonic_ms()+12;
    VMS_EXEC(g_threads).emplace_back();VMS_EXEC(g_threads).back().state=ThreadState::Runnable;
    check(game_runnable(),"runnable peer fixture");
    speed_wait_paced_sleep();
    check(VMS_EXEC(g_speed_slept_us)==0,"runnable work prevents host sleep for an unrelated thread");
    VMS_EXEC(g_threads).pop_back();VMS_RS(g_fb_dirty)=true;speed_wait_paced_sleep();
    check(VMS_EXEC(g_speed_slept_us)==0,"pending presentation prevents host sleep");
    VMS_RS(g_fb_dirty)=false;VMS_RS(g_repaint)=true;speed_wait_paced_sleep();
    check(VMS_EXEC(g_speed_slept_us)==0,"pending paint prevents host sleep");
    VMS_RS(g_repaint)=false;VMS_EXEC(g_serial_calls).push_back(1);speed_wait_paced_sleep();
    check(VMS_EXEC(g_speed_slept_us)==0,"pending callback prevents host sleep");
    VMS_EXEC(g_serial_calls).clear();sleeping.state=ThreadState::Sleeping;sleeping.wake_us=paint_clock_us()+2000;
    speed_wait_paced_sleep();
    check(VMS_EXEC(g_speed_slept_us)>0,"fully sleeping game still waits without spinning");

    // Slow paints without clock gate evidence cannot accelerate Java time.
    reset();VMS_EXEC(g_threads).emplace_back();auto& unpaced=VMS_EXEC(g_threads).back();
    const int64_t baseline=paint_clock_us();
    for(int i=0;i<12;i++)speed_note_paint(baseline+i*21000,&unpaced);
    check(VMS_EXEC(g_game_clock).scale==1 && unpaced.auto_fps.stable==0,
          "no polling evidence keeps the clock at one despite 21ms paint gaps");
    reset();VMS_EXEC(g_threads).emplace_back();auto& verified=VMS_EXEC(g_threads).back();
    const int64_t verified_start=paint_clock_us();speed_note_paint(verified_start,&verified);
    for(int i=0;i<5;i++) {
        verified.auto_fps.gate_period(50000);
        speed_note_paint(verified_start+(i+1)*16667,&verified);
    }
    check(std::fabs(VMS_EXEC(g_game_clock).scale-1.2)<0.0001,
          "five verified gates earn a bounded 0.2 increase");
    for(int i=0;i<5;i++) {
        verified.auto_fps.gate_period(16667);
        speed_note_paint(verified_start+(i+6)*16667,&verified);
    }
    check(VMS_EXEC(g_game_clock).scale==1,"verified shorter gates can reduce the clock to one");
    reset();VMS_EXEC(g_threads).emplace_back();auto& timer=VMS_EXEC(g_threads).back();VMS_EXEC(g_running_thread)=&timer;
    check(speed_frame_wake_us(8)==0 && speed_frame_wake_us(70)==0 && !timer.sleep_paced,
          "background timer without a drawing retains its sleep");
    const int thread_cls=ensure_class("java/lang/Thread");
    Method* sleep_method=find_method(thread_cls,"sleep","(J)V",false);
    for(int requested:{10,100}) {
        Frame sleep_frame;sleep_frame.method=sleep_method;sleep_frame.locals={Value::of_l(requested)};
        const bool old_suspend=VMS_EXEC(g_can_suspend);VMS_EXEC(g_can_suspend)=true;
        const int64_t started=monotonic_ms();thread_native(sleep_method,sleep_frame);VMS_EXEC(g_can_suspend)=old_suspend;
        check(timer.state==ThreadState::Sleeping && timer.sleep_request==0 && timer.frame_due_us==0 && timer.wake_us==0,
              "background native sleep has no cap or legacy-vsync association");
        check(timer.wake>=started+requested && timer.wake<=monotonic_ms()+requested,
              "background native sleep retains its Java duration");
        timer.state=ThreadState::Runnable;timer.wake=0;VMS_EXEC(g_yield)=false;
    }
    timer.frame_due_us=paint_clock_us()+9000;
    check(speed_frame_wake_us(50)>0 && timer.frame_due_us==0,
          "owned frame deadline is consumed once");
    check(speed_frame_wake_us(50)==0,"extra sleep cannot invent another frame deadline");
    timer.frame_due_us=paint_clock_us()-1000;
    const int64_t short_before=paint_clock_us(),short_due=speed_frame_wake_us(8);
    check(short_due-short_before>=1000 && short_due-short_before<2000,
          "expired owned short sleep does not append a full period");
    VMS_EXEC(g_running_thread)=nullptr;
    reset();
    int cls=ensure_class("SpeedIntegrationChecks");VMS_RS(g_current)=alloc_obj(cls);
    run(find_method(cls,"<init>","()V",false),{Value::of_ref(VMS_RS(g_current))});
    VMS_EXEC(g_threads).emplace_back();auto& unrelated=VMS_EXEC(g_threads).back();
    unrelated.state=ThreadState::Sleeping;unrelated.wake_us=paint_clock_us()+100000;
    unrelated.wake=monotonic_ms()+100;
    VMS_EXEC(g_speed_next_us)=paint_clock_us()+6000;
    repaint_current();
    check(VMS_EXEC(g_speed_add_us)>0,"unrelated sleeper cannot bypass the paint deadline");
    check(VMS_EXEC(g_main_thread).frame_due_us==VMS_EXEC(g_speed_next_us),"completed host paint records its next deadline");

    // A peer's pending repaint cannot borrow the current caller's group.
    reset();speed_sync_clock();VMS_EXEC(g_threads).emplace_back();auto& grouped=VMS_EXEC(g_threads).back();
    VMS_EXEC(g_threads).emplace_back();auto& requester=VMS_EXEC(g_threads).back();grouped.ref=11;requester.ref=12;
    grouped.auto_fps.drawing(1000000,16667,1);
    for(int i=0;i<5;i++){grouped.auto_fps.gate_period(50000);grouped.auto_fps.drawing(1050000+i*50000,16667,1);}
    grouped.frame_group_started_us=paint_clock_us();VMS_EXEC(g_speed_budget_owner)=&grouped;
    VMS_RS(g_current)=alloc_obj(cls);run(find_method(cls,"<init>","()V",false),{Value::of_ref(VMS_RS(g_current))});
    VMS_EXEC(g_running_thread)=&grouped;VMS_EXEC(g_repaint_owner_ref)=requester.ref;VMS_RS(g_repaint)=true;
    VMS_EXEC(g_speed_next_us)=paint_clock_us();const auto peer_budget_before=VMS_EXEC(g_speed_budget_frames);
    repaint_current();
    check(VMS_EXEC(g_speed_budget_owner)==&requester && VMS_EXEC(g_speed_budget_frames)==peer_budget_before+1,
          "peer pending repaint retains requester ownership and its own cap slot");
    VMS_EXEC(g_speed_budget_owner)=&grouped;VMS_EXEC(g_repaint_owner_ref)=0;VMS_RS(g_repaint)=true;VMS_EXEC(g_speed_next_us)=paint_clock_us();
    const auto host_budget_before=VMS_EXEC(g_speed_budget_frames);repaint_current();
    check(VMS_EXEC(g_speed_budget_owner)==&VMS_EXEC(g_main_thread) && VMS_EXEC(g_speed_budget_frames)==host_budget_before+1,
          "zero-ref host pending repaint does not borrow a servicing worker group");
    grouped.auto_fps.group_open=true;grouped.auto_fps.group_drawn=true;grouped.auto_fps.group_complete=false;
    VMS_EXEC(g_speed_budget_owner)=&grouped;grouped.frame_group_started_us=paint_clock_us()-200000;
    VMS_EXEC(g_game_clock).rate(paint_clock_us(),3.0);
    const int64_t continuous_before=VMS_EXEC(g_game_clock).value_us(paint_clock_us());
    check(!speed_group_incomplete() && !grouped.auto_fps.verified_gate_seen,
          "missing primary gate expires grouping on the real clock");
    check(VMS_EXEC(g_game_clock).scale==1 && VMS_EXEC(g_game_clock).value_us(paint_clock_us())>=continuous_before,
          "expired authority loses its boost without reversing Java time");
    VMS_EXEC(g_running_thread)=nullptr;VMS_RS(g_repaint)=false;

    // The owner survives a host slice and a later unrelated frame. Only that
    // owner consumes its completed-frame deadline, once.
    reset();VMS_EXEC(g_threads).emplace_back();auto& owner=VMS_EXEC(g_threads).back();
    VMS_EXEC(g_threads).emplace_back();auto& peer=VMS_EXEC(g_threads).back();
    const int64_t anchor=paint_clock_us()-20000;
    speed_note_paint(anchor,&owner);
    const int64_t owned=owner.frame_due_us;
    speed_note_paint(paint_clock_us(),&peer);
    check(owner.frame_due_us==owned && peer.frame_due_us>owned,"another thread's drawing does not replace owner deadline");
    VMS_EXEC(g_running_thread)=&owner;
    int64_t now=paint_clock_us(),due=speed_frame_wake_us(50);
    check(due>=now && due-now<4000,"late post-paint sleep does not append another frame period");
    check(owner.frame_due_us==0,"frame deadline consumed once");
    owner.frame_due_us=paint_clock_us()+9000;
    check(speed_frame_wake_us(1)==0 && owner.frame_due_us!=0,"short polls do not consume a frame sleep");
    check(speed_frame_wake_us(500)==0 && owner.frame_due_us==0,"long scene wait ends frame association");
    VMS_EXEC(g_speed_cap)=0;owner.frame_due_us=paint_clock_us()+9000;
    check(speed_frame_wake_us(50)==0 && owner.frame_due_us==0,"native mode clears pacing association");
    VMS_EXEC(g_running_thread)=nullptr;VMS_EXEC(g_threads).clear();

    // Learn from one owner's wake-to-draw work, then spend its single
    // completed-draw credit early while preserving the global paint deadline.
    for(int cap:{30,60}) {
        reset();VMS_EXEC(g_speed_cap)=cap;speed_sync_clock();
        VMS_RS(g_current)=alloc_obj(cls);
        VMS_EXEC(g_threads).emplace_back();auto& preparer=VMS_EXEC(g_threads).back();
        VMS_EXEC(g_threads).emplace_back();auto& background=VMS_EXEC(g_threads).back();
        const int period=speed_period_us();
        auto learn=[&](JavaThreadContext& context,int64_t work) {
            for(int n=0;n<4;++n) {
                context.frame_prep.arm(VMS_RS(g_current),period,nullptr,-1);
                context.frame_prep.woke(1000000);
                context.frame_prep.before_draw(VMS_RS(g_current),period,1000000+work);
            }
        };
        learn(preparer,4000);
        VMS_EXEC(g_running_thread)=&preparer;
        const int64_t draw_due=paint_clock_us()+period;
        preparer.frame_due_us=VMS_EXEC(g_speed_next_us)=draw_due;
        check(speed_frame_wake_us(50)==draw_due-4500,
              "trained owner wakes for preparation before its 30/60 cap deadline");
        check(preparer.frame_due_us==0 && preparer.sleep_paced && VMS_EXEC(g_speed_next_us)==draw_due,
              "early wake spends one owner credit and preserves the paint cap");
        check(speed_frame_wake_us(50)==0,
              "preparation cannot arm a second paced sleep without another drawing");
        VMS_EXEC(g_running_thread)=&background;
        check(speed_frame_wake_us(50)==0 && background.frame_prep.count==0 && !background.frame_prep.awaiting_wake,
              "background sleeper cannot acquire the drawing owner's prep budget");

        // An overdue scheduled wake is not the start of preparation. The VM
        // records when the sleeper actually becomes runnable, once.
        preparer.frame_prep.reset();preparer.frame_prep.arm(VMS_RS(g_current),period,nullptr,-1);
        preparer.state=ThreadState::Sleeping;
        preparer.wake_us=paint_clock_us()+100000;
        preparer.wake=monotonic_ms()+100;
        refresh_thread(preparer);
        check(preparer.frame_prep.awaiting_wake && preparer.frame_prep.woke_us==0,
              "planned future wake does not start preparation timing");
        preparer.wake_us=paint_clock_us()-20000;
        const int64_t runnable_before=paint_clock_us();refresh_thread(preparer);
        const int64_t runnable_after=paint_clock_us(),actual_wake=preparer.frame_prep.woke_us;
        check(preparer.state==ThreadState::Runnable && actual_wake>=runnable_before && actual_wake<=runnable_after,
              "preparation starts at the actual sleeping-to-runnable transition");
        refresh_thread(preparer);
        check(preparer.frame_prep.woke_us==actual_wake,
              "refreshing an already runnable owner does not restart preparation timing");
        const int64_t before_draw=paint_clock_us();
        // A future cap wait must not become part of wake-to-draw preparation.
        VMS_EXEC(g_speed_next_us)=before_draw+5000;
        const int64_t cap_due=VMS_EXEC(g_speed_next_us),paint_started=speed_begin_frame(&preparer);
        check(preparer.frame_prep.count==1 && preparer.frame_prep.recent[0]>=before_draw-actual_wake &&
              preparer.frame_prep.recent[0]<paint_started-actual_wake,
              "preparation is sampled before cap waiting rather than after it");
        check(paint_started>=cap_due && VMS_EXEC(g_speed_next_us)==cap_due,
              "early preparation never bypasses or moves the drawing deadline");
        const auto sample_count=preparer.frame_prep.count;
        speed_begin_frame(&preparer);
        check(preparer.frame_prep.count==sample_count,
              "another drawing without a new owner sleep cannot append a prep sample");

        // Real Display/Thread entry points and the lifecycle clock reset must
        // invalidate every owner, including the host/main context.
        preparer.frame_prep.reset();learn(preparer,4000);learn(background,2000);learn(VMS_EXEC(g_main_thread),3000);
        for(auto* context:{&preparer,&background,&VMS_EXEC(g_main_thread)}) {
            context->frame_due_us=paint_clock_us()+period;context->sleep_paced=true;
        }
        const int display_cls=ensure_class("javax/microedition/lcdui/Display");
        const int display=alloc_obj(display_cls),next_canvas=alloc_obj(cls);
        run(find_method(display_cls,"setCurrent0","(Ljavax/microedition/lcdui/Displayable;ILjavax/microedition/lcdui/Displayable;)V",false),
            {Value::of_ref(display),Value::of_ref(next_canvas),Value::of_i(-1),Value::of_ref(0)});
        for(auto* context:{&preparer,&background,&VMS_EXEC(g_main_thread)})
            check(context->frame_prep.count==0 && !context->frame_prep.awaiting_draw &&
                  context->frame_due_us==0 && !context->sleep_paced,
                  "Canvas switch clears preparation and completed-draw credit for every owner");
        VMS_RS(g_repaint)=false;
        learn(preparer,4000);preparer.frame_prep.arm(VMS_RS(g_current),period,nullptr,-1);
        preparer.ref=alloc_obj(thread_cls);preparer.state=ThreadState::Sleeping;
        preparer.wake_us=paint_clock_us()+period;preparer.wake=monotonic_ms()+100;
        VMS_EXEC(g_running_thread)=nullptr;
        run(find_method(thread_cls,"interrupt","()V",false),{Value::of_ref(preparer.ref)});
        check(preparer.state==ThreadState::Runnable && preparer.wake_us==0 &&
              preparer.frame_prep.count==0 && !preparer.frame_prep.awaiting_draw,
              "interrupting a paced sleeper discards its preparation history and pending sample");
        learn(preparer,4000);learn(background,2000);learn(VMS_EXEC(g_main_thread),3000);
        speed_pause_clock(true);
        for(auto* context:{&preparer,&background,&VMS_EXEC(g_main_thread)})
            check(context->frame_prep.count==0 && !context->frame_prep.awaiting_draw && context->frame_due_us==0,
                  "pause clears preparation and drawing credit for workers and host");
        speed_pause_clock(false);
        check(preparer.frame_prep.lead_us(period)==0 && !VMS_EXEC(g_game_clock).paused,
              "resume starts preparation warmup again");
    }

    reset();
    run(find_method(cls,"setupCallbacks","()I",false),{});
    int before=VMS_RS(g_game_frames);advance_game_frame();
    check(VMS_RS(g_game_frames)==before+2 && !VMS_EXEC(g_serial_calls).empty(),"initial pending paint precedes callback without starving it");
    before=VMS_RS(g_game_frames);advance_game_frame();
    check(VMS_RS(g_game_frames)==before+1 && !VMS_EXEC(g_serial_calls).empty(),"self-rescheduling callback draws once per host advance");
    run(find_method(cls,"startWorker","()V",false),{});
    Field* worker=find_field(cls,"workerSteps","I");
    const int steps_before=VMS_CL(g_classes)[cls].statics[worker->slot].i;
    for(int n=0;n<3;++n)advance_game_frame();
    check(VMS_CL(g_classes)[cls].statics[worker->slot].i>steps_before,
          "drawing serial callbacks cannot starve a runnable worker");
    Field* continuous=find_field(cls,"continuous","Z");
    Field* callbacks=find_field(cls,"callbacks","I");
    VMS_CL(g_classes)[cls].statics[continuous->slot]=Value::of_i(1);
    const int calls_before=VMS_CL(g_classes)[cls].statics[callbacks->slot].i;
    VMS_RS(g_repaint)=true;
    for(int n=0;n<5;++n)advance_game_frame();
    check(VMS_CL(g_classes)[cls].statics[callbacks->slot].i==calls_before+5,
          "continuous repaint cannot starve serial callbacks or dispatch twice per host frame");
    stop_thread();
    CanvasPaintStats stats;stats.reset(1000,0);
    auto window=stats.take(1001000,60);
    check(window.canvas_fps()==0 && std::fabs(window.draw_fps()-60.0)<0.001,"flush-only windows contribute to total draw FPS");
    std::cout<<"PASS SpeedIntegrationChecks "<<checks<<"\n";
}
