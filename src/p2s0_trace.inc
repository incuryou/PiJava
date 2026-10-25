// P2.2-S0: three safepoint assertions (count only, never crash, never refuse) and the C08
// request->paint->surface->present trace (PX-1 doc 02, PX-2 doc 03).
// Observation only: it never changes scheduling, never drops or adds a paint, and is not part of
// the J2S5 instant state. Default OFF. Switch: sdmc:/switch/j2me/p2-s0-trace.enable ; desktop: J2ME_S0_TRACE=1.
// With it off every hook is a single branch on s0.on (paint entry tags are one byte store).
#ifndef J2ME_S0_COMPILED
#define J2ME_S0_COMPILED 1
#endif
#if J2ME_S0_COMPILED
static bool cont_running_any();
static bool s0_audio_unfrozen(bool& pending_ends);
struct S0Trace {
    bool on=false;
    // safepoints
    uint64_t poll_checks=0,poll_violation=0,poll_v_native=0,poll_v_pc=0,poll_v_suspend=0;
    uint64_t gc_checks=0,gc_violation=0,gc_skipped_run_depth=0,gc_v_depth=0,gc_v_executing=0,gc_v_roots=0;
    uint64_t snap_checks=0,snap_violation=0,snap_v_gc=0,snap_v_serial=0,snap_v_cont=0,snap_v_audio_running=0,snap_v_audio_ends=0,snap_gpu_untracked=0;
    // C08 trace
    enum Entry:uint8_t{E_OTHER=0,E_ADVANCE=1,E_SERIAL_PREPAINT=2,E_SERVICE=3,E_COUNT=4};
    uint8_t entry=E_OTHER;
    uint64_t turn_id=0,req_id=0,paint_id=0,surf_ver=0,present_id=0,presented_ver=0;
    uint64_t paints_by_entry[E_COUNT]{},superseded_by_entry[E_COUNT]{},presented_by_entry[E_COUNT]{};
    uint64_t turn_paints=0,turns_with_paint=0,turns_multi_paint=0,max_paints_per_turn=0,dup_present=0;
    uint8_t pending_entries[64]{};unsigned pending=0;uint64_t pending_overflow=0;
    struct Rec{uint64_t turn,id,aux;int64_t us;uint8_t type,entry;};
    static constexpr unsigned kRing=1024; Rec ring[kRing]{}; uint64_t ring_n=0;
    int64_t last_summary_us=0;
    // --- P2.2 S2-S4: observation only (user ruling option B). Nothing here changes what is painted or shown. ---
    // S2 "queue the 2nd paint of a turn": which paints would be deferred, would-be backlog/age, which frame would be shown.
    uint64_t s2_would_defer=0,s2_would_defer_by_entry[E_COUNT]{},s2_backlog=0,s2_backlog_max=0,s2_backlog_turns=0;
    int64_t s2_backlog_since_us=0,s2_age_max_us=0; uint64_t s2_age_sum_us=0,s2_age_n=0;
    uint64_t s2_turn_first_ver=0,s2_turn_last_ver=0; uint8_t s2_turn_first_entry=0;
    uint64_t s2_shown_differs=0,s2_shown_same=0,s2_would_show_by_entry[E_COUNT]{};
    // request -> completion matching (every request must be covered by a later completed paint)
    uint64_t req_covered=0,req_unmatched_at_present=0;
    // S3 "pausable long paint": paints longer than the slice / completed-paint budget
    int64_t paint_started_us=0,paint_max_us=0; uint64_t s3_over_slice=0,s3_over_budget=0,s3_would_yields=0,s3_unfinished_presented=0;
    // S4 "serviceRepaints waits on target paint": target id matching and r02 BCI-retry occurrences
    uint64_t s4_calls=0,s4_pending=0,s4_no_pending=0,s4_target_matched=0,s4_target_mismatch=0,s4_r02_retries=0,s4_target=0;
};
static S0Trace s0;
static const char* s0_entry_name(int e){static const char* n[]={"other","advance","serial_prepaint","serviceRepaints"};return n[e&3];}
static void s0_rec(uint8_t type,uint64_t id,uint64_t aux){
    auto& r=s0.ring[s0.ring_n++%S0Trace::kRing];r={s0.turn_id,id,aux,paint_clock_us(),type,s0.entry};
}
static void s0_configure(){
#ifdef __SWITCH__
    struct stat st{};s0.on=stat("sdmc:/switch/j2me/p2-s0-trace.enable",&st)==0&&S_ISREG(st.st_mode);
#else
    const char* v=std::getenv("J2ME_S0_TRACE");s0.on=v&&std::strcmp(v,"1")==0;
#endif
    if(s0.on)log_line("S0CONFIG on=1 ring=1024 summary_period_us=1000000 mode=observe_only");
}
struct S0EntryScope{uint8_t saved;explicit S0EntryScope(uint8_t e):saved(s0.entry){s0.entry=e;}~S0EntryScope(){s0.entry=saved;}};
// --- 1. scheduler poll (interpreter slice cut) ---
static void s0_assert_poll(const Frame& top,bool will_yield,bool can_suspend,bool active_is_thread){
    ++s0.poll_checks;bool bad=false;
    if(top.method->native){++s0.poll_v_native;bad=true;}
    if(top.pc<0||size_t(top.pc)>top.method->code.size()){++s0.poll_v_pc;bad=true;}
    if(will_yield&&!(can_suspend&&active_is_thread)){++s0.poll_v_suspend;bad=true;}
    if(bad&&++s0.poll_violation<=8)log_line("SAFEPOINT poll_violation n="+std::to_string(s0.poll_violation));
}
// --- 2. GC safepoint (gc_safepoint, was maybe_gc; just before gc_collect) ---
static bool s0_gc_conditions(bool count){
    bool ok=true;
    if(VMS_EXEC(g_run_depth)!=0||VMS_EXEC(g_pump_depth)!=0||VMS_RS(g_paint_depth)!=0){ok=false;if(count)++s0.gc_v_depth;}
    for(auto& t:VMS_EXEC(g_threads))if(t.executing){ok=false;if(count)++s0.gc_v_executing;break;}
    for(auto& p:VMS_HEAP(g_root_stacks))if(!p.second){ok=false;if(count)++s0.gc_v_roots;break;}
    return ok;
}
static void s0_assert_gc_safe(){
    ++s0.gc_checks;
    if(!s0_gc_conditions(true)&&++s0.gc_violation<=8)log_line("SAFEPOINT gc_violation n="+std::to_string(s0.gc_violation));
}
// --- 3. snapshot safepoint (save_instant_state entry). Count only: S0 never refuses a save (that is S7). ---
static void s0_assert_snapshot_safe(){
    ++s0.snap_checks;bool bad=false;
    if(!s0_gc_conditions(false)){++s0.snap_v_gc;bad=true;}
    if(VMS_EXEC(g_run_depth)!=0){++s0.snap_v_serial;bad=true;} // a serial callback is mid-run only inside run()
    if(cont_running_any()){++s0.snap_v_cont;bad=true;}             // S1: no continuation in phase Running
    bool ends=false; if(s0_audio_unfrozen(ends)){++s0.snap_v_audio_running;bad=true;} // S1: audio sink not paused
    if(ends){++s0.snap_v_audio_ends;bad=true;}                     // S1: undelivered end-of-media native steps
    ++s0.snap_gpu_untracked;                                       // GPU fences: structure only (GpuFenceState), P5
    if(bad){++s0.snap_violation;log_line("SAFEPOINT snapshot_violation n="+std::to_string(s0.snap_violation)+" (counted only, save not refused in S0)");}
}
// --- C08 hooks ---
static void s0_turn_begin(){++s0.turn_id;s0.turn_paints=0;s0_rec(0,s0.turn_id,0);
    if(s0.s2_backlog){ // the would-be deferred paints would run now: record their age, then they leave the queue
        const int64_t age=paint_clock_us()-s0.s2_backlog_since_us;s0.s2_age_max_us=std::max(s0.s2_age_max_us,age);
        s0.s2_age_sum_us+=uint64_t(std::max<int64_t>(0,age));++s0.s2_age_n;++s0.s2_backlog_turns;s0.s2_backlog=0;}
}
static void s0_turn_end(){
    if(s0.turn_paints){++s0.turns_with_paint;if(s0.turn_paints>1)++s0.turns_multi_paint;s0.max_paints_per_turn=std::max(s0.max_paints_per_turn,s0.turn_paints);}
    s0_rec(1,s0.turn_id,s0.turn_paints);
    if(s0.turn_paints>1){ // with deferral the present at this turn would show the first paint of the turn
        ++s0.s2_shown_differs;++s0.s2_would_show_by_entry[s0.s2_turn_first_entry&3];
    } else if(s0.turn_paints==1){++s0.s2_shown_same;++s0.s2_would_show_by_entry[s0.s2_turn_first_entry&3];}
}
static void s0_request(){++s0.req_id;s0_rec(2,s0.req_id,0);}
static void s0_paint_begin(){++s0.paint_id;s0_rec(3,s0.paint_id,0);s0.paint_started_us=paint_clock_us();}
static void p23_surface();
static void s0_surface(){ // a paint completed into g_fb
    p23_surface();
    ++s0.surf_ver;++s0.turn_paints;++s0.paints_by_entry[s0.entry&3];
    s0.req_covered=s0.req_id; // a completed paint covers every request made before it (coalescing)
    if(s0.turn_paints==1){s0.s2_turn_first_ver=s0.surf_ver;s0.s2_turn_first_entry=s0.entry;}
    else { ++s0.s2_would_defer;++s0.s2_would_defer_by_entry[s0.entry&3];
        if(!s0.s2_backlog)s0.s2_backlog_since_us=paint_clock_us();++s0.s2_backlog;s0.s2_backlog_max=std::max(s0.s2_backlog_max,s0.s2_backlog);}
    s0.s2_turn_last_ver=s0.surf_ver;
    { const int64_t d=paint_clock_us()-s0.paint_started_us; s0.paint_max_us=std::max(s0.paint_max_us,d);
      const int64_t slice_us=int64_t(std::max(1,VMS_EXEC(g_slice_ms)))*1000;
      if(d>slice_us){++s0.s3_over_slice;s0.s3_would_yields+=uint64_t(d/slice_us);}
      if(d>8000)++s0.s3_over_budget; }
    if(s0.pending<64)s0.pending_entries[s0.pending++]=s0.entry;else ++s0.pending_overflow;
    s0_rec(4,s0.surf_ver,s0.paint_id);
}
static void s0_summary(const char* why){
    std::string s="C08TRACE why="+std::string(why)+" turns="+std::to_string(s0.turn_id)+" requests="+std::to_string(s0.req_id)+
        " paints="+std::to_string(s0.paint_id)+" surf_ver="+std::to_string(s0.surf_ver)+" presents="+std::to_string(s0.present_id)+
        " dup_present="+std::to_string(s0.dup_present)+" turns_with_paint="+std::to_string(s0.turns_with_paint)+
        " turns_multi_paint="+std::to_string(s0.turns_multi_paint)+" max_paints_per_turn="+std::to_string(s0.max_paints_per_turn);
    for(int e=0;e<4;++e)s+=std::string(" ")+s0_entry_name(e)+"=paint:"+std::to_string(s0.paints_by_entry[e])+"/presented:"+
        std::to_string(s0.presented_by_entry[e])+"/superseded:"+std::to_string(s0.superseded_by_entry[e]);
    s+=" pending_overflow="+std::to_string(s0.pending_overflow);
    log_line(s);
    log_line("P22OBS S2 would_defer="+std::to_string(s0.s2_would_defer)+" by_entry=other:"+std::to_string(s0.s2_would_defer_by_entry[0])+
        "/advance:"+std::to_string(s0.s2_would_defer_by_entry[1])+"/serial_prepaint:"+std::to_string(s0.s2_would_defer_by_entry[2])+
        "/serviceRepaints:"+std::to_string(s0.s2_would_defer_by_entry[3])+" backlog_max="+std::to_string(s0.s2_backlog_max)+
        " backlog_turns="+std::to_string(s0.s2_backlog_turns)+" age_max_us="+std::to_string(s0.s2_age_max_us)+
        " age_avg_us="+std::to_string(s0.s2_age_n?s0.s2_age_sum_us/s0.s2_age_n:0)+
        " shown_frame_would_differ_turns="+std::to_string(s0.s2_shown_differs)+" shown_same_turns="+std::to_string(s0.s2_shown_same)+
        " would_show=other:"+std::to_string(s0.s2_would_show_by_entry[0])+"/advance:"+std::to_string(s0.s2_would_show_by_entry[1])+
        "/serial_prepaint:"+std::to_string(s0.s2_would_show_by_entry[2])+"/serviceRepaints:"+std::to_string(s0.s2_would_show_by_entry[3])+
        " requests="+std::to_string(s0.req_id)+" covered="+std::to_string(s0.req_covered)+" pending_at_present="+std::to_string(s0.req_unmatched_at_present)+
        " mode=observe_only");
    log_line("P22OBS S3 paint_max_us="+std::to_string(s0.paint_max_us)+" over_slice="+std::to_string(s0.s3_over_slice)+
        " over_8ms_budget="+std::to_string(s0.s3_over_budget)+" would_yields="+std::to_string(s0.s3_would_yields)+
        " unfinished_presented="+std::to_string(s0.s3_unfinished_presented)+" mode=observe_only");
    log_line("P22OBS S4 serviceRepaints_calls="+std::to_string(s0.s4_calls)+" with_pending_paint="+std::to_string(s0.s4_pending)+
        " without_pending="+std::to_string(s0.s4_no_pending)+" target_matched="+std::to_string(s0.s4_target_matched)+
        " target_mismatch="+std::to_string(s0.s4_target_mismatch)+" r02_bci_retries="+std::to_string(s0.s4_r02_retries)+" mode=observe_only");
    log_line("SAFEPOINT summary poll="+std::to_string(s0.poll_checks)+"/"+std::to_string(s0.poll_violation)+
        " (native="+std::to_string(s0.poll_v_native)+" pc="+std::to_string(s0.poll_v_pc)+" suspend="+std::to_string(s0.poll_v_suspend)+")"
        " gc="+std::to_string(s0.gc_checks)+"/"+std::to_string(s0.gc_violation)+" (depth="+std::to_string(s0.gc_v_depth)+
        " executing="+std::to_string(s0.gc_v_executing)+" roots="+std::to_string(s0.gc_v_roots)+") gc_skipped_run_depth="+
        std::to_string(s0.gc_skipped_run_depth)+" snapshot="+std::to_string(s0.snap_checks)+"/"+std::to_string(s0.snap_violation)+" (gc="+std::to_string(s0.snap_v_gc)+
        " in_run="+std::to_string(s0.snap_v_serial)+" cont_running="+std::to_string(s0.snap_v_cont)+" audio_running="+std::to_string(s0.snap_v_audio_running)+
        " audio_ends="+std::to_string(s0.snap_v_audio_ends)+" gpu_fence=untracked:"+std::to_string(s0.snap_gpu_untracked)+")"+
        " rootscope=not_present_until_P2.4");
}
static void s0_present(){ // a present call consumed the current g_fb (C08: "present call", not "scanned out")
    ++s0.present_id;
    if(s0.req_id>s0.req_covered)++s0.req_unmatched_at_present; // a request still waiting for its paint at present time (not lost: covered later)
    if(VMS_RS(g_paint_depth)!=0)++s0.s3_unfinished_presented;   // present during an unfinished paint (must stay 0)
    if(s0.surf_ver==s0.presented_ver)++s0.dup_present;
    else if(s0.pending){
        for(unsigned i=0;i+1<s0.pending;++i)++s0.superseded_by_entry[s0.pending_entries[i]&3];
        ++s0.presented_by_entry[s0.pending_entries[s0.pending-1]&3];
    }
    s0_rec(5,s0.present_id,s0.surf_ver);s0.presented_ver=s0.surf_ver;s0.pending=0;
    const int64_t now=paint_clock_us();if(now-s0.last_summary_us>=1000000){s0.last_summary_us=now;s0_summary("period");}
}
// S4: serviceRepaints entry/exit. would_wait = a paint is pending; target = the next paint id.
static void s0_service_enter(bool pending){++s0.s4_calls;if(pending){++s0.s4_pending;s0.s4_target=s0.paint_id+1;}else{++s0.s4_no_pending;s0.s4_target=0;}}
static void s0_service_exit(){if(!s0.s4_target)return;if(s0.paint_id>=s0.s4_target&&s0.surf_ver>0)++s0.s4_target_matched;else ++s0.s4_target_mismatch;s0.s4_target=0;}
static void s0_flush_ring(){
    const uint64_t n=std::min<uint64_t>(s0.ring_n,S0Trace::kRing);
    static const char* tn[]={"turn_begin","turn_end","request","paint_begin","surface","present"};
    for(uint64_t i=s0.ring_n-n;i<s0.ring_n;++i){auto& r=s0.ring[i%S0Trace::kRing];
        log_line("C08REC "+std::string(tn[r.type])+" turn="+std::to_string(r.turn)+" id="+std::to_string(r.id)+" aux="+std::to_string(r.aux)+" entry="+s0_entry_name(r.entry)+" us="+std::to_string(r.us));}
}
static void s0_configure_hook(){s0_configure();}
static void s0_report(){if(!s0.on)return;s0_summary("report");s0_flush_ring();}
#define S0_HOOK(x) do{ if(s0.on){ x; } }while(0)
#define S0_ENTRY(e) S0EntryScope s0_entry_scope(e)
#else
#define S0_HOOK(x) do{}while(0)
#define S0_ENTRY(e) do{}while(0)
static inline void s0_configure_hook(){}
static inline void s0_report(){}
#endif
