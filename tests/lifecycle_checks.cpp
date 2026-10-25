#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "runtime_stats_checks.h"
#include "../src/host_menu.h"  // 0.2.112 F2: status-bar geometry for the self-exit hint checks

static int checks = 0;
static void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { std::cerr << "FAIL LifecycleChecks " << label << "\n"; std::exit(1); }
}
static int fixture() { return ensure_class("LifecycleChecks"); }
static int field(const char* name) {
    int c=fixture();
    return VMS_CL(g_classes)[c].statics[find_field(c,name,"I")->slot].i;
}
static void set_field(const char* name, int value) {
    int c=fixture();
    VMS_CL(g_classes)[c].statics[find_field(c,name,"I")->slot]=Value::of_i(value);
}
static int invoke(const char* name) {
    return run(find_method(fixture(),name,"()I",false),{}).i;
}
static void launch(const char* name="LifecycleChecks") { launch_class(name); }
static void scopes_clean() {
    check(VMS_EXEC(g_run_depth)==0 && VMS_EXEC(g_pump_depth)==0 && VMS_RS(g_paint_depth)==0 && !VMS_EXEC(g_lifecycle_busy),
          "callback scopes balance after host transition");
}
static void destroyed_clean(int handle) {
    check(VMS_EXEC(g_midlet_state)==MidletState::Destroyed && !VMS_EXEC(g_midlet) && !VMS_RS(g_current) && !VMS_RS(g_shown) && !VMS_RS(g_display),
          "destroy clears MIDlet and display references");
    check(VMS_EXEC(g_threads).empty() && !VMS_EXEC(g_thread_live) && VMS_EXEC(g_monitors).empty() && VMS_HEAP(g_root_stacks).empty() && !VMS_EXEC(g_active_stack),
          "destroy clears worker and interpreter state");
    check(VMS_EXEC(g_serial_calls).empty() && !VMS_RS(g_alert_deadline) && !VMS_RS(g_alert_return) && !VMS_PLAT(g_key_states) && VMS_PLAT(g_pressed_keys).empty(),
          "destroy clears callbacks, alert and pressed keys");
    check(VMS_PLAT(g_audio).state(handle)==j2me_audio::Engine::Closed && VMS_PLAT(g_tones).empty(),
          "destroy closes audio handles and tones");
    scopes_clean();
}

int main(int argc, char** argv) {
    VMS_CL(g_classpath) = {argc>1 ? argv[1] : "build/test-classes", "romfs/classes"};
    VMS_DIAG(g_log_draw)=false;
    launch();
    check(VMS_EXEC(g_midlet_state)==MidletState::Active && field("starts")==1 && field("showA")==1 && invoke("shown")==1,
          "first launch starts and shows canvas exactly once");
    host_events();
    check(field("showA")==1 && field("hideA")==0,"idle host does not duplicate visibility callbacks");
    int tone=VMS_PLAT(g_audio).playTone(69,60000,80); VMS_PLAT(g_tones).insert(tone);
    check(tone>0 && invoke("workers")==1,"live worker and audio fixture");
    thread_resume();
    int ticks=field("workerTicks");
    check(ticks>0,"active worker executes");
    VMS_PLAT(g_key_states)=31;
    dispatch_key(49,false);
    check(field("held")==49 && field("keys")==1,"canvas records a held physical key before pause");
    // MIDP getKeyStates returns zero for a GameCanvas that is not shown; the
    // fixture's poll canvas is hidden while ordinary canvas A receives input.
    check(invoke("polledKeys")==0,"hidden GameCanvas does not see another canvas's held host keys");
    seed_runtime_stats();
    const auto before_pause_generation=VMS_EXEC(g_runtime_profile).generation;
    lifecycle_pause(true);
    assert_runtime_stats_reset();
    check(VMS_EXEC(g_runtime_profile).generation>before_pause_generation,"pause resets reporting generations");
    host_events();
    check(VMS_EXEC(g_midlet_state)==MidletState::Paused && VMS_PLAT(g_host_paused) && field("pauses")==1 && field("hideA")==1 && invoke("shown")==0,
          "host pause calls pauseApp and hides canvas");
    check(!VMS_PLAT(g_key_states) && invoke("polledKeys")==0 && VMS_PLAT(g_audio).paused(),"host pause clears polled held keys and suspends audio");
    check(field("held")==0 && field("releases")==1,"host pause releases held canvas key before hiding");
    int paints=field("paints"), keys=field("keys");
    thread_resume(); repaint_current(); dispatch_key(49,false); host_events();
    check(field("workerTicks")==ticks && field("paints")==paints && field("keys")==keys,
          "paused workers, paint and key callbacks do not advance");
    lifecycle_pause(true); host_events();
    check(field("pauses")==1 && field("hideA")==1,"repeated pause is idempotent");
    invoke("requestResume"); host_events();
    check(VMS_EXEC(g_midlet_state)==MidletState::Paused && field("starts")==1,"application request cannot override host pause");
    seed_runtime_stats();
    lifecycle_resume();
    assert_runtime_stats_reset();
    // Stale wake observations are filtered without modifying Java timers.
    const int64_t before_ignore=VMS_EXEC(g_wake_stats).ignore_before_ms;
    VMS_EXEC(g_wake_stats).dispatched(before_ignore-1,before_ignore+100);
    check(!VMS_EXEC(g_wake_stats).sleep_wakes,"resume excludes pre-pause wake deadlines");
    host_events();
    check(VMS_EXEC(g_midlet_state)==MidletState::Active && !VMS_PLAT(g_host_paused) && !VMS_PLAT(g_audio).paused() && field("starts")==2 && field("showA")==2,
          "host resume calls startApp and restores visibility/audio");
    check(field("held")==0 && invoke("polledKeys")==0 && field("releases")==1,"resume carries no stale physical or polled key");
    dispatch_key(49,false); VMS_PLAT(g_key_states)=31; invoke("selfPause");
    check(VMS_EXEC(g_midlet_state)==MidletState::Active && VMS_EXEC(g_notify_paused),"notifyPaused is deferred to host boundary");
    host_events();
    check(VMS_EXEC(g_midlet_state)==MidletState::Paused && !VMS_PLAT(g_host_paused) && field("pauses")==1 && field("hideA")==2,
          "notifyPaused does not call pauseApp again");
    check(!field("held") && field("releases")==2 && invoke("polledKeys")==0,
          "notifyPaused releases physical and polled keys at safe boundary");
    invoke("requestResume"); host_events();
    check(VMS_EXEC(g_midlet_state)==MidletState::Active && field("starts")==3 && field("showA")==3,
          "resumeRequest starts a self-paused MIDlet");

    invoke("switchB");
    check(field("showB")==0,"setCurrent queues showNotify");
    host_events();
    check(field("hideA")==3 && field("showB")==1,"screen switch hides old and shows new canvas");
    invoke("sameCurrent"); host_events();
    check(field("showB")==1 && field("hideB")==0,"setCurrent same object does not repeat callbacks");
    invoke("hideCurrent"); host_events();
    check(!VMS_RS(g_current) && !VMS_RS(g_shown) && field("hideB")==1,"setCurrent null hides the old screen");
    invoke("showRedirect"); host_events(); host_events();
    check(field("showA")==4 && field("hideA")==4 && field("showB")==2 && VMS_RS(g_current)==VMS_RS(g_shown),
          "reentrant showNotify setCurrent settles without inline recursion");
    invoke("showAAgain"); host_events();
    invoke("hideRedirect"); host_events();
    check(field("showC")==1 && field("showB")==2 && VMS_RS(g_current)==VMS_RS(g_shown),
          "hideNotify redirect supersedes the originally requested screen");
    check(field("violations")==0,"show/hide isShown and callback non-recursion invariants");

    check(invoke("queueSerial")==0 && VMS_EXEC(g_serial_calls).size()==1,"callSerially queues instead of invoking inline");
    gc_collect("lifecycle queued callback root");
    host_events();
    check(field("serials")==18 && VMS_EXEC(g_serial_calls).size()==1,"queued closure and payload survive full GC; nested call waits");
    host_events();
    check(field("serials")==19 && VMS_EXEC(g_serial_calls).empty(),"nested serial callback executes on next host event");
    check(invoke("nullSerial")==1,"callSerially rejects null with Java exception");
    int serials=field("serials");invoke("queueAfterPaint");host_events();
    check(field("serials")==serials+1 && !field("violations"),"callSerially executes after pending repaint completes");
    invoke("alert"); host_events();
    int alert_ref=VMS_RS(g_current);
    check(VMS_RS(g_alert_deadline)>monotonic_ms() && VMS_RS(g_alert_return) && VMS_RS(g_current)==VMS_RS(g_shown),"timed Alert is shown and records return screen");
    lifecycle_pause(true);
    check(!VMS_RS(g_alert_deadline) && VMS_RS(g_alert_remaining)>0,"pause freezes remaining Alert visible time");
    const int64_t remaining=VMS_RS(g_alert_remaining); host_events();
    check(VMS_RS(g_current)==alert_ref && VMS_RS(g_alert_remaining)==remaining,"paused host preserves Alert countdown");
    // Resume's startApp deliberately sets canvas A, so separately verify active expiry.
    lifecycle_resume(); host_events(); invoke("alert"); host_events();
    int return_ref=VMS_RS(g_alert_return); VMS_RS(g_alert_deadline)=monotonic_ms()-1; host_events();
    check(VMS_RS(g_current)==return_ref && VMS_RS(g_shown)==return_ref && !VMS_RS(g_alert_deadline) && !VMS_RS(g_alert_return),
          "expired Alert returns and clears timeout state");
    invoke("listenerAlert");host_events();VMS_RS(g_alert_deadline)=monotonic_ms()-1;host_events();
    check(field("alertDismissals")==1 && !field("violations") && invoke("currentIsC")==1 && VMS_RS(g_current)==VMS_RS(g_shown) && !VMS_RS(g_alert_deadline),
          "Alert expiry dispatches listener and honors its selected return screen");
    invoke("foreverAlert"); host_events();
    check(VMS_RS(g_alert_deadline)==0 && VMS_RS(g_alert_return),"FOREVER Alert has no expiry deadline");

    invoke("queueSerial"); VMS_PLAT(g_key_states)=7; set_field("destroyMode",1);
    lifecycle_destroy();
    check(field("destroys")==1 && field("unconditional")==1,"host destruction calls destroyApp(true), including throwing callback");
    destroyed_clean(tone);
    lifecycle_destroy(); host_events();
    check(field("destroys")==1,"destroy remains idempotent");
    check(!VMS_EXEC(g_self_exit),"0.2.112 F2: host destruction is not a self exit");

    launch();
    check(field("starts")==1 && field("pauses")==0 && field("destroys")==0 && field("serials")==0 && field("workerTicks")==0 &&
          !VMS_EXEC(g_notify_destroyed) && !VMS_EXEC(g_notify_paused) && !VMS_EXEC(g_midlet_resume) && !VMS_PLAT(g_host_paused) && !VMS_PLAT(g_audio).paused(),
          "relaunch begins with fresh Java and host lifecycle state");
    invoke("workers"); tone=VMS_PLAT(g_audio).playTone(72,60000,80);VMS_PLAT(g_tones).insert(tone);
    VMS_PLAT(g_key_states)=3;
    check(VMS_EXEC(g_last_console_line).empty() && !VMS_EXEC(g_self_exit),"0.2.112 F2: relaunch clears last console line and self-exit flag");
    check(invoke("printNotice")==1 && VMS_EXEC(g_last_console_line)=="  \u6388\u6743\u8fc7\u671f\t","0.2.112 F2: last non-empty console line is kept, blank lines ignored");
    check(invoke("selfDestroy")==15 && field("afterDestroy")==15 && VMS_EXEC(g_notify_destroyed) && VMS_EXEC(g_midlet_state)==MidletState::Active && !VMS_EXEC(g_threads).empty(),
          "notifyDestroyed leaves Java frames and workers intact until safe host boundary");
    scopes_clean(); host_events();
    check(field("destroys")==0 && field("violations")==0,"notifyDestroyed skips duplicate destroyApp and queued callbacks");
    destroyed_clean(tone);
    {   // 0.2.112 F2: self-exit hint text
        check(VMS_EXEC(g_self_exit),"notifyDestroyed marks a self exit");
        const auto fallback=[](const std::string& s){return host_menu::text_advance(s,host_menu::status_px);};
        check(self_exit_hint::status(VMS_EXEC(g_last_console_line),fallback,host_menu::status_width)=="游戏已自行退出：授权过期","hint carries the trimmed last message");
        check(self_exit_hint::status("",fallback,host_menu::status_width)=="游戏已自行退出","hint without a message");
        check(self_exit_hint::status(" \t\r ",fallback,host_menu::status_width)=="游戏已自行退出","blank message gives the bare hint");
        check(self_exit_hint::clean("a\x01" "b\nc")=="a b c","control characters become spaces");
        std::string thirty; for(int i=0;i<30;++i) thirty+="授";
        std::string twentyfour; for(int i=0;i<24;++i) twentyfour+="授";
        check(self_exit_hint::clean(thirty)==twentyfour+"…","message capped at 24 code points with an ellipsis");
        const std::string longest=self_exit_hint::status(thirty,fallback,host_menu::status_width);
        check(longest=="游戏已自行退出："+twentyfour+"…" && fallback(longest)<=host_menu::status_width,"24 CJK characters fit the status bar at 17 px");
        const std::string tight=self_exit_hint::status(thirty,fallback,17*14);
        check(fallback(tight)<=17*14 && tight=="游戏已自行退出：授授授授授…","narrow bar truncates further at code-point boundaries");
        check(self_exit_hint::status(thirty,fallback,10)=="游戏已自行退出","bar narrower than the prefix keeps the bare hint");
        check(self_exit_hint::clean(std::string("ab\xe6\x8e"))=="ab\xe6\x8e","malformed UTF-8 tail is kept byte-wise, never split mid-sequence");
    }

    launch("LifecycleChecks$RetryStart");
    check(VMS_EXEC(g_midlet_state)==MidletState::Paused && field("starts")==1 && field("showA")==0 && !VMS_RS(g_shown),
          "MIDletStateChangeException keeps failed startup paused");
    set_field("startMode",0); invoke("requestResume"); host_events();
    check(VMS_EXEC(g_midlet_state)==MidletState::Active && field("starts")==2 && field("showA")==1,"retryable startup can later resume");
    lifecycle_pause(true); set_field("pauseMode",1); lifecycle_resume();host_events();
    lifecycle_pause(true); host_events();
    check(field("pauses")==2 && VMS_EXEC(g_midlet_state)==MidletState::Destroyed,"pauseApp unchecked exception triggers safe cleanup");
    destroyed_clean(0);
    launch("LifecycleChecks$BrokenStart");
    check(field("starts")==1 && field("showA")==0 && VMS_EXEC(g_midlet_state)==MidletState::Destroyed,"unchecked startup exception destroys before screen callbacks");
    destroyed_clean(0);
    launch("LifecycleChecks$SelfPausedStart");
    check(VMS_EXEC(g_midlet_state)==MidletState::Paused && field("starts")==1 && field("pauses")==0 && field("showA")==0,
          "notifyPaused from startApp suppresses activation without pauseApp");
    set_field("startMode",0); invoke("requestResume"); host_events();
    check(VMS_EXEC(g_midlet_state)==MidletState::Active && field("showA")==1,"startup self-pause can resume normally");
    lifecycle_destroy(); scopes_clean();
    {   // 0.2.112 F4: the FPS counter advances once per executed present, never per paint and never without a present
        VMS_RS(g_fb_dirty)=true; const uint64_t before=VMS_RS(g_present_frames); const int paints_before=VMS_RS(g_game_frames); int presents=0;
        const bool shown=sched_present_turn(true,false,false,3,[&]{++presents;});
        check(shown && presents==1 && VMS_RS(g_present_frames)==before+1 && VMS_RS(g_game_frames)==paints_before,"present counter counts one executed present");
        VMS_RS(g_fb_dirty)=false; VMS_PLAT(g_last_present_us)=paint_clock_us(); presents=0;
        const bool idle=sched_present_turn(true,false,false,3,[&]{++presents;});
        check(!idle && presents==0 && VMS_RS(g_present_frames)==before+1,"no present, no count");
    }
    seed_runtime_stats();stop_thread();assert_runtime_stats_reset();
    std::cout << "PASS LifecycleChecks " << checks << " lifecycle, display, safepoint, GC, callback and cleanup checks\n";
}
