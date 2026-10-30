// Bounded native diagnostics. Never allocates Java objects or traces opcodes.
#include <array>
#include <cstdarg>
 VMS_DIAG_ACCESSOR(g_p1_profile, p1::Profile)
struct P1Diagnostics {
    static constexpr size_t Ring=64,Limit=512,CaughtRing=32;
    std::array<std::array<char,1024>,Ring> lines{};
    std::array<char,768> last_end{};
    std::array<std::array<char,1024>,CaughtRing> caught_lines{};
    size_t caught_next=0,caught_retained=0;
    uint64_t caught_overwritten=0,caught_dumps=0;
    bool caught_zero_pending=false,caught_end_dumped=false;
    size_t queued=0;
    bool enabled=true,first_zero=false;
    uint64_t session_id=uint64_t(paint_clock_us()),launch_id=0,events=0,dropped=0,caught_count=0,zero_transitions=0;
    uint64_t reason_counts[4]{},host_counts[6]{};
    int host_last=-1,start_outcome=-1;
    std::deque<int64_t> serial_times;
    int64_t repaint_since=0;
    uint64_t serial_enqueues=0,serial_dispatches=0;
    uint64_t pending_first_id=0,started_first_id=0,completed_first_id=0,presented_first_id=0,coalesced_requests=0;
    uint64_t draw_id=0,present_draw_id=0;
    uint64_t request_id=0,start_id=0,completed_id=0,present_id=0,present_count=0,duplicate_present=0,paint_requests=0;
    uint64_t paint_count=0,paint_sum=0,paint_max=0,hist[13]{};
} ; VMS_DIAG_ACCESSOR(g_p1, P1Diagnostics)
static void p1_event(const char* fmt,...){
    if(!VMS_DIAG(g_p1).enabled)return;
    const bool critical=std::strncmp(fmt,"first_live_to_zero",18)==0;
    if(!critical&&(VMS_DIAG(g_p1).events>=P1Diagnostics::Limit||VMS_DIAG(g_p1).queued==P1Diagnostics::Ring)){++VMS_DIAG(g_p1).dropped;return;}
    if(critical&&VMS_DIAG(g_p1).queued==P1Diagnostics::Ring){--VMS_DIAG(g_p1).queued;++VMS_DIAG(g_p1).dropped;}
    auto& line=VMS_DIAG(g_p1).lines[VMS_DIAG(g_p1).queued++];
    int n=std::snprintf(line.data(),line.size(),"P1EVENT schema=1 session=%llu launch=%llu mono_us=%lld ",
        (unsigned long long)VMS_DIAG(g_p1).session_id,(unsigned long long)VMS_DIAG(g_p1).launch_id,(long long)paint_clock_us());
    va_list ap;va_start(ap,fmt);std::vsnprintf(line.data()+n,line.size()-n,fmt,ap);va_end(ap);++VMS_DIAG(g_p1).events;
}
// At most two last-N dumps per launch: first live-to-zero and final reset/destroy.
// Capture strings while frames/objects exist; flush only at safe host boundaries.
static void p1_dump_caught_last(const char* boundary){
    if(!VMS_DIAG(g_p1).enabled||!VMS_DIAG(g_p1).caught_retained)return;
    ++VMS_DIAG(g_p1).caught_dumps;
    log_line("P1CAUGHTSET scope=launch_last_n session="+std::to_string(VMS_DIAG(g_p1).session_id)+" launch="+std::to_string(VMS_DIAG(g_p1).launch_id)+" boundary="+boundary+" retained="+std::to_string(VMS_DIAG(g_p1).caught_retained)+" overwritten="+std::to_string(VMS_DIAG(g_p1).caught_overwritten)+" dump="+std::to_string(VMS_DIAG(g_p1).caught_dumps));
    const size_t first=(VMS_DIAG(g_p1).caught_next+P1Diagnostics::CaughtRing-VMS_DIAG(g_p1).caught_retained)%P1Diagnostics::CaughtRing;
    for(size_t i=0;i<VMS_DIAG(g_p1).caught_retained;++i)log_line(VMS_DIAG(g_p1).caught_lines[(first+i)%P1Diagnostics::CaughtRing].data());
}
static void p1_flush(const char* boundary){
    if(!VMS_DIAG(g_p1).enabled)return;
    for(size_t i=0;i<VMS_DIAG(g_p1).queued;++i)log_line(VMS_DIAG(g_p1).lines[i].data());
    VMS_DIAG(g_p1).queued=0;
    if(VMS_DIAG(g_p1).caught_zero_pending){VMS_DIAG(g_p1).caught_zero_pending=false;p1_dump_caught_last("first_live_to_zero");}
    // Key lifecycle boundaries are durable; a crash can lose the current ring.
    if(std::strcmp(boundary,"host_turn")&&std::strcmp(boundary,"present")&&std::strcmp(boundary,"window"))log_drain();
}
static void p1_snapshot(JavaThreadContext& t,const Frame& f,size_t count){
    if(!VMS_DIAG(g_p1).enabled)return;
    t.p1_last_method=f.method;t.p1_last_bci=f.op_pc;t.p1_last_frames=count;
}
static const char* p1_canvas(){
    return VMS_RS(g_current)>0&&VMS_RS(g_current)<int(VMS_HEAP(g_heap).size())&&VMS_HEAP(g_heap)[VMS_RS(g_current)].cls>=0 ? VMS_CL(g_classes)[VMS_HEAP(g_heap)[VMS_RS(g_current)].cls].name.c_str():"-";
}
static int p1_bio_field(const char* name){
    if(VMS_PLAT(g_rms_suite)!="95f4e836247c5d0f00c7056c59cd80d57e0706e1a5159f4f39cb9656597f37bd"||std::strcmp(p1_canvas(),"BioCanvas"))return -1;
    const auto& o=VMS_HEAP(g_heap)[VMS_RS(g_current)];
    for(int c=o.cls;c>=0;c=VMS_CL(g_classes)[c].super_id)for(const auto& f:VMS_CL(g_classes)[c].fields)
        if(!f.is_static&&f.name==name&&f.slot>=0&&size_t(f.slot)<o.fields.size())return o.fields[f.slot].i;
    return -1;
}
static void p1_thread_end(JavaThreadContext& t){
    if(t.state==ThreadState::Terminated)return;
    const int reason=std::max(0,std::min(3,t.p1_end_reason));++VMS_DIAG(g_p1).reason_counts[reason];
    unsigned before=0,after=0;for(const auto& w:VMS_EXEC(g_threads))if(w.state!=ThreadState::Terminated){++before;if(&w!=&t)++after;}
    Method* m=t.p1_last_method,*origin=t.p1_throw_method;
    std::snprintf(VMS_DIAG(g_p1).last_end.data(),VMS_DIAG(g_p1).last_end.size(),"P1LASTEND launch=%llu ref=%d owner=%s method=%s last_bci=%d last_frames=%zu reason=%d workers_before=%u workers_after=%u mono_us=%lld throw_owner=%s throw_method=%s throw_bci=%d throw_frames=%zu",
        (unsigned long long)VMS_DIAG(g_p1).launch_id,t.ref,m?VMS_CL(g_classes)[m->class_id].name.c_str():"-",m?m->name.c_str():"-",t.p1_last_bci,t.p1_last_frames,reason,before,after,(long long)paint_clock_us(),origin?VMS_CL(g_classes)[origin->class_id].name.c_str():"-",origin?origin->name.c_str():"-",t.p1_throw_bci,t.p1_throw_frames);
    p1_event("worker finish ref=%d class=%s run_owner=%s method=%s descriptor=%s last_bci=%d last_frames=%zu frames_now=%zu reason=%s live_before=%d live_after=%d workers_before=%u workers_after=%u",
        t.ref,t.ref>0&&t.ref<int(VMS_HEAP(g_heap).size())&&VMS_HEAP(g_heap)[t.ref].cls>=0?VMS_CL(g_classes)[VMS_HEAP(g_heap)[t.ref].cls].name.c_str():"-",m?VMS_CL(g_classes)[m->class_id].name.c_str():"-",m?m->name.c_str():"-",m?m->desc.c_str():"-",t.p1_last_bci,t.p1_last_frames,t.frames.size(),
        reason==0?"normal_return":reason==1?"uncaught_exception":reason==2?"reset":"destroy",before!=0,after!=0,before,after);
    if(before&&!after){
        ++VMS_DIAG(g_p1).zero_transitions;
        if(!VMS_DIAG(g_p1).first_zero){
            VMS_DIAG(g_p1).first_zero=true;VMS_DIAG(g_p1).caught_zero_pending=true;
            int64_t age=VMS_DIAG(g_p1).serial_times.size()==VMS_EXEC(g_serial_calls).size()&&!VMS_DIAG(g_p1).serial_times.empty()?paint_clock_us()-VMS_DIAG(g_p1).serial_times.front():-1;
            p1_event("first_live_to_zero ref=%d last_bci=%d last_frames=%zu lifecycle=%d startApp_outcome=%d canvas=%s serial_pending=%zu serial_oldest_age_us=%lld repaint=%d repaint_age_us=%lld loading_phase=%d loading_profile=%d frame=%d bio_main_task=%d bio_load_task=%d bio_task_change=%d",
                t.ref,t.p1_last_bci,t.p1_last_frames,int(VMS_EXEC(g_midlet_state)),VMS_DIAG(g_p1).start_outcome,p1_canvas(),VMS_EXEC(g_serial_calls).size(),(long long)age,int(VMS_RS(g_repaint)),
                (long long)(VMS_DIAG(g_p1).repaint_since?paint_clock_us()-VMS_DIAG(g_p1).repaint_since:-1),int(VMS_EXEC(g_farm_startup).state),int(VMS_EXEC(g_farm_startup).profile),VMS_RS(g_game_frames),p1_bio_field("m_MainTask"),p1_bio_field("m_LoadTask"),p1_bio_field("m_TskChangeFlg"));
        }
    }
}
static void p8_caught_note(int ref,const char* cls,const char* where_cls,const char* where_method,int bci);
static void p1_caught(int ref,const Frame& f,int handler,Method* origin,int origin_bci,size_t origin_depth){
    diag_caught(f,handler);
    ++VMS_DIAG(g_p1).caught_count;
    p8_caught_note(ref,ref>0&&VMS_HEAP(g_heap)[ref].cls>=0?VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name.c_str():"-",VMS_CL(g_classes)[f.method->class_id].name.c_str(),f.method->name.c_str(),f.op_pc); // P8.2
    if(!VMS_DIAG(g_p1).enabled)return;
    auto& line=VMS_DIAG(g_p1).caught_lines[VMS_DIAG(g_p1).caught_next];
    std::snprintf(line.data(),line.size(),"P1CAUGHT schema=1 session=%llu launch=%llu seq=%llu mono_us=%lld ref=%d exception=%s worker=%d class=%s method=%s bci=%d handler=%d frames=%zu throw_owner=%s throw_method=%s throw_bci=%d throw_frames=%zu",
        (unsigned long long)VMS_DIAG(g_p1).session_id,(unsigned long long)VMS_DIAG(g_p1).launch_id,(unsigned long long)VMS_DIAG(g_p1).caught_count,(long long)paint_clock_us(),ref,
        ref>0&&VMS_HEAP(g_heap)[ref].cls>=0?VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name.c_str():"-",current_context().ref,
        VMS_CL(g_classes)[f.method->class_id].name.c_str(),f.method->name.c_str(),f.op_pc,handler,VMS_EXEC(g_active_stack)?VMS_EXEC(g_active_stack)->size():0,
        origin?VMS_CL(g_classes)[origin->class_id].name.c_str():"-",origin?origin->name.c_str():"-",origin_bci,origin_depth);
    VMS_DIAG(g_p1).caught_next=(VMS_DIAG(g_p1).caught_next+1)%P1Diagnostics::CaughtRing;
    if(VMS_DIAG(g_p1).caught_retained<P1Diagnostics::CaughtRing)++VMS_DIAG(g_p1).caught_retained;
    else ++VMS_DIAG(g_p1).caught_overwritten;
}
static void p1_serial_enqueue(){
    ++VMS_DIAG(g_p1).serial_enqueues;
    if(!VMS_DIAG(g_p1).enabled)return;
    if(VMS_DIAG(g_p1).serial_times.size()!=VMS_EXEC(g_serial_calls).size())VMS_DIAG(g_p1).serial_times.clear();
    // Bound auxiliary storage; unknown age is -1 if the actual queue exceeds it.
    if(VMS_EXEC(g_serial_calls).size()<512)VMS_DIAG(g_p1).serial_times.push_back(paint_clock_us());
    else VMS_DIAG(g_p1).serial_times.clear();
}
static void p1_serial_pop(){
    ++VMS_DIAG(g_p1).serial_dispatches;
    if(VMS_DIAG(g_p1).serial_times.size()==VMS_EXEC(g_serial_calls).size()&&!VMS_DIAG(g_p1).serial_times.empty())VMS_DIAG(g_p1).serial_times.pop_front();
    else VMS_DIAG(g_p1).serial_times.clear();
}
static void p1_request(){
    S0_HOOK(s0_request());
    if(!VMS_DIAG(g_p1).repaint_since)VMS_DIAG(g_p1).repaint_since=paint_clock_us();
    ++VMS_DIAG(g_p1).request_id;if(!VMS_DIAG(g_p1).pending_first_id)VMS_DIAG(g_p1).pending_first_id=VMS_DIAG(g_p1).request_id;
}
static void p1_paint_start(){
    if(VMS_DIAG(g_p1).request_id==VMS_DIAG(g_p1).paint_requests)p1_request(); // host/synchronous implicit request
    VMS_DIAG(g_p1).started_first_id=VMS_DIAG(g_p1).pending_first_id?VMS_DIAG(g_p1).pending_first_id:VMS_DIAG(g_p1).request_id;
    VMS_DIAG(g_p1).coalesced_requests+=VMS_DIAG(g_p1).request_id-VMS_DIAG(g_p1).started_first_id;VMS_DIAG(g_p1).pending_first_id=0;
    VMS_DIAG(g_p1).start_id=VMS_DIAG(g_p1).request_id;VMS_DIAG(g_p1).paint_requests=VMS_DIAG(g_p1).request_id;VMS_DIAG(g_p1).repaint_since=0;
}
static void p1_paint_complete(int64_t us){
    VMS_DIAG(g_p1).completed_id=VMS_DIAG(g_p1).start_id;VMS_DIAG(g_p1).completed_first_id=VMS_DIAG(g_p1).started_first_id;++VMS_DIAG(g_p1).paint_count;
    uint64_t duration=uint64_t(std::max<int64_t>(0,us));VMS_DIAG(g_p1).paint_sum+=duration;VMS_DIAG(g_p1).paint_max=std::max(VMS_DIAG(g_p1).paint_max,duration);
    static constexpr uint64_t upper[]={1000,2000,4000,8000,16000,32000,64000,128000,256000,512000,1024000,2048000};
    size_t bin=0;while(bin<12&&duration>upper[bin])++bin;++VMS_DIAG(g_p1).hist[bin];
}
static void p1_present(){
    ++VMS_DIAG(g_p1).present_count;VMS_DIAG(g_p1).duplicate_present+=VMS_DIAG(g_p1).present_draw_id==VMS_DIAG(g_p1).draw_id;VMS_DIAG(g_p1).present_draw_id=VMS_DIAG(g_p1).draw_id;VMS_DIAG(g_p1).present_id=VMS_DIAG(g_p1).completed_id;VMS_DIAG(g_p1).presented_first_id=VMS_DIAG(g_p1).completed_first_id;
}
static void p1_host_decision(int work,const p1::Turn& t){
    ++VMS_DIAG(g_p1).host_counts[work];
    if(work!=VMS_DIAG(g_p1).host_last){VMS_DIAG(g_p1).host_last=work;p1_event("host decision=%d runnable=%d pending=%d serial=%zu repaint=%d dirty=%d heartbeat=%d boundary_ready=%d handoff=%d nap_ms=%lld lifecycle=%d canvas=%s",
        work,int(t.runnable),int(t.pending),VMS_EXEC(g_serial_calls).size(),int(VMS_RS(g_repaint)),int(t.dirty),int(t.heartbeat),int(t.boundary_ready),int(VMS_EXEC(g_completed_paint241).enabled),(long long)t.nap_ms,int(VMS_EXEC(g_midlet_state)),p1_canvas());}
}
static void p1_report_m3g_epoch();
static void p1_begin_m3g_epoch();
static void cont_report();
static void native_roots_report();
static void gc_safepoint_report();
static void sched_core_report();
static void snapshot_check_report();
static void p23_report();
static void p1_report(){
    s0_report();cont_report();native_roots_report();gc_safepoint_report();sched_core_report();snapshot_check_report();p23_report();
    city_diag_report();
    if(!VMS_DIAG(g_p1).enabled)return;
    p1_flush("window");
    char line[2048];std::snprintf(line,sizeof(line),"P1STAT schema=1 session=%llu launch=%llu mono_us=%lld events=%llu dropped=%llu event_cap=512 ring=64 caught_cap=32 caught_policy=rolling_last_n caught_dump_limit=2 tail_loss=possible request_id=%llu start_id=%llu completed_id=%llu presented_id=%llu presents=%llu duplicate_presents=%llu paint_count=%llu paint_sum_us=%llu paint_max_us=%llu paint_hist_upper_us=1000,2000,4000,8000,16000,32000,64000,128000,256000,512000,1024000,2048000,overflow paint_hist=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu scope=launch_cumulative logic_fps=unavailable frequency=query_only underrun=unknown serial_pending=%zu repaint=%d canvas=%s lifecycle=%d",
        (unsigned long long)VMS_DIAG(g_p1).session_id,(unsigned long long)VMS_DIAG(g_p1).launch_id,(long long)paint_clock_us(),(unsigned long long)VMS_DIAG(g_p1).events,(unsigned long long)VMS_DIAG(g_p1).dropped,
        (unsigned long long)VMS_DIAG(g_p1).request_id,(unsigned long long)VMS_DIAG(g_p1).start_id,(unsigned long long)VMS_DIAG(g_p1).completed_id,(unsigned long long)VMS_DIAG(g_p1).present_id,(unsigned long long)VMS_DIAG(g_p1).present_count,(unsigned long long)VMS_DIAG(g_p1).duplicate_present,
        (unsigned long long)VMS_DIAG(g_p1).paint_count,(unsigned long long)VMS_DIAG(g_p1).paint_sum,(unsigned long long)VMS_DIAG(g_p1).paint_max,
        (unsigned long long)VMS_DIAG(g_p1).hist[0],(unsigned long long)VMS_DIAG(g_p1).hist[1],(unsigned long long)VMS_DIAG(g_p1).hist[2],(unsigned long long)VMS_DIAG(g_p1).hist[3],(unsigned long long)VMS_DIAG(g_p1).hist[4],(unsigned long long)VMS_DIAG(g_p1).hist[5],(unsigned long long)VMS_DIAG(g_p1).hist[6],(unsigned long long)VMS_DIAG(g_p1).hist[7],(unsigned long long)VMS_DIAG(g_p1).hist[8],(unsigned long long)VMS_DIAG(g_p1).hist[9],(unsigned long long)VMS_DIAG(g_p1).hist[10],(unsigned long long)VMS_DIAG(g_p1).hist[11],(unsigned long long)VMS_DIAG(g_p1).hist[12],VMS_EXEC(g_serial_calls).size(),int(VMS_RS(g_repaint)),p1_canvas(),int(VMS_EXEC(g_midlet_state)));
    log_line(line);
    log_line("P1FRAME scope=launch_cumulative draw_completed_id="+std::to_string(VMS_DIAG(g_p1).draw_id)+" draw_presented_id="+std::to_string(VMS_DIAG(g_p1).present_draw_id)+" canvas_request_id="+std::to_string(VMS_DIAG(g_p1).request_id)+" canvas_started_request_id="+std::to_string(VMS_DIAG(g_p1).start_id)+" canvas_completed_request_id="+std::to_string(VMS_DIAG(g_p1).completed_id)+" canvas_presented_request_id="+std::to_string(VMS_DIAG(g_p1).present_id)+" canvas_started_first_request_id="+std::to_string(VMS_DIAG(g_p1).started_first_id)+" canvas_completed_first_request_id="+std::to_string(VMS_DIAG(g_p1).completed_first_id)+" canvas_presented_first_request_id="+std::to_string(VMS_DIAG(g_p1).presented_first_id)+" pending_first_request_id="+std::to_string(VMS_DIAG(g_p1).pending_first_id)+" coalesced_requests="+std::to_string(VMS_DIAG(g_p1).coalesced_requests)+" request_semantics=inclusive_first_to_latest_repaint_or_implicit_sync logic_fps=unavailable");
    if(VMS_DIAG(g_p1).last_end[0])log_line(VMS_DIAG(g_p1).last_end.data());
    log_line("P1END scope=launch_cumulative normal_return="+std::to_string(VMS_DIAG(g_p1).reason_counts[0])+" uncaught_exception="+std::to_string(VMS_DIAG(g_p1).reason_counts[1])+" reset="+std::to_string(VMS_DIAG(g_p1).reason_counts[2])+" destroy="+std::to_string(VMS_DIAG(g_p1).reason_counts[3])+" caught="+std::to_string(VMS_DIAG(g_p1).caught_count)+" live_to_zero="+std::to_string(VMS_DIAG(g_p1).zero_transitions)+" startApp_outcome="+std::to_string(VMS_DIAG(g_p1).start_outcome));
    log_line("P1CAUGHTSTAT scope=launch_cumulative captured="+std::to_string(VMS_DIAG(g_p1).caught_count)+" retained="+std::to_string(VMS_DIAG(g_p1).caught_retained)+" overwritten="+std::to_string(VMS_DIAG(g_p1).caught_overwritten)+" dumps="+std::to_string(VMS_DIAG(g_p1).caught_dumps));
    p1_report_m3g_epoch();
    log_line("P1QUEUE scope=launch_cumulative launch="+std::to_string(VMS_DIAG(g_p1).launch_id)+" enqueued="+std::to_string(VMS_DIAG(g_p1).serial_enqueues)+" dispatched="+std::to_string(VMS_DIAG(g_p1).serial_dispatches)+" pending="+std::to_string(VMS_EXEC(g_serial_calls).size())+" bio_main_task="+std::to_string(p1_bio_field("m_MainTask"))+" bio_load_task="+std::to_string(p1_bio_field("m_LoadTask"))+" bio_task_change="+std::to_string(p1_bio_field("m_TskChangeFlg")));
    log_line("P1HOST scope=launch_cumulative ready="+std::to_string(VMS_DIAG(g_p1).host_counts[0])+" advance="+std::to_string(VMS_DIAG(g_p1).host_counts[1])+" worker="+std::to_string(VMS_DIAG(g_p1).host_counts[2])+" idle="+std::to_string(VMS_DIAG(g_p1).host_counts[3])+" gpu_scheduled="+std::to_string(VMS_DIAG(g_p1).host_counts[4])+" cpu_scheduled="+std::to_string(VMS_DIAG(g_p1).host_counts[5]));
}
static void p1_new_launch(const std::string& identity,const std::string& main){
    const auto session=VMS_DIAG(g_p1).session_id,launch=VMS_DIAG(g_p1).launch_id+1;const bool enabled=VMS_DIAG(g_p1).enabled;
    VMS_DIAG(g_p1)=P1Diagnostics{};VMS_DIAG(g_p1).session_id=session;VMS_DIAG(g_p1).launch_id=launch;VMS_DIAG(g_p1).enabled=enabled;
    city_diag_launch(identity);
    diag_launch(identity,main);
    p1_begin_m3g_epoch();
    p1_event("launch begin build=%s suite=%s main=%s profile=%s speed=%d renderer=cpu jit=%d",kVer,identity.c_str(),main.c_str(),VMS_DIAG(g_p1_profile).active?VMS_DIAG(g_p1_profile).id.c_str():"legacy",VMS_EXEC(g_speed_cap),int(VMS_NATIVE(g_native_jit).enabled));
}
static void p1_configure(); // defined after native accelerators, before configuration
