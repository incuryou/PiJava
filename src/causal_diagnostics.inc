// All helpers run on the existing VM owner. Stored handles are weak, checked
// against the current heap generation; observations do not influence scheduling.
static uint32_t diag_thread_generation(const JavaThreadContext& t){
    return t.ref>0&&size_t(t.ref)<VMS_HEAP(g_heap).size()?VMS_HEAP(g_heap)[t.ref].generation:UINT32_MAX;
}
static void diag_alloc(const std::string& descriptor,size_t length,uint64_t bytes){
    Method* method=nullptr;int bci=-1;
    if(auto* stack=VMS_EXEC(g_active_stack))if(!stack->empty()){method=stack->back().method;bci=stack->back().op_pc;}
    g_device_diag.allocations.note(diag_method(method),bci,descriptor.size()>1?descriptor[1]:'?',uint64_t(length),bytes);
}
static void diag_sleep_start(JavaThreadContext& t,int64_t requested,int64_t effective,runtime_cause::Route route,bool owned){
    runtime_cause::SleepRequest r;r.thread=t.ref;r.generation=diag_thread_generation(t);r.started_us=paint_clock_us();r.requested_ms=requested;r.effective_ms=effective;r.deadline_ms=t.wake;r.deadline_us=t.wake_us;r.route=route;r.cap=VMS_EXEC(g_speed_cap);r.owned_before=owned;r.paced_after=t.sleep_paced;r.virtual_clock=r4::state.active;r.clock_scale=VMS_EXEC(g_game_clock).scale;r.draw=uint64_t(VMS_RS(g_game_frames));
    if(auto* stack=VMS_EXEC(g_active_stack))for(size_t n=0;n<std::min<size_t>(4,stack->size());++n){const auto& frame=(*stack)[stack->size()-1-n];r.callers[n]=diag_method(frame.method);r.caller_bcis[n]=frame.op_pc;}
    r.method=r.callers[0];r.bci=r.caller_bcis[0];
    r.session=g_device_diag.owner.session;r.launch=g_device_diag.owner.launch;
    const auto id=g_device_diag.sleeps.begin(r);
    if(requested>120||effective>=50){char line[640];std::snprintf(line,sizeof(line),"id=%llu thread=%d generation=%u method=%s bci=%d requested_ms=%lld effective_ms=%lld route=%s cap=%d owned_before=%d paced_after=%d deadline_ms=%lld deadline_us=%lld scale=%.6f",(unsigned long long)id,r.thread,r.generation,g_device_diag.labels.get(r.method),r.bci,(long long)requested,(long long)effective,runtime_cause::route_name(route),r.cap,int(owned),int(r.paced_after),(long long)r.deadline_ms,(long long)r.deadline_us,r.clock_scale);g_device_diag.flight.note(r.started_us,"sleep_request",line);}
}
static void diag_sleep_ready(JavaThreadContext& t){g_device_diag.sleeps.ready(t.ref,diag_thread_generation(t),paint_clock_us());}
static void diag_sleep_dispatch(JavaThreadContext& t){g_device_diag.sleeps.dispatch(t.ref,diag_thread_generation(t),paint_clock_us());}
static void diag_sleep_cancel(JavaThreadContext& t,runtime_cause::CancelReason reason){g_device_diag.sleeps.cancel(t.ref,diag_thread_generation(t),paint_clock_us(),reason);}
static void diag_causal_reset(){
    auto& d=g_device_diag;d.sleeps.cancel_all(paint_clock_us());d.sleeps.clear_window();d.allocations.clear();
    const auto& pool=jit236::code_pool_stats();d.jit_creates_seen=pool.create_calls;d.jit_failures_seen=pool.create_failures;d.jit_create_us_seen=pool.create_wall_us;
}
static void diag_sleep_record(const runtime_cause::SleepCompletion& c){
    const auto& r=c.request;char line[1536];
    const bool normal=c.status==runtime_cause::Status::Completed;
    const bool clock_valid=c.dispatched_us>=r.started_us&&c.ready_us>=r.started_us&&c.dispatched_us>=c.ready_us;
    char identity[256];std::snprintf(identity,sizeof(identity),"DIAGSLEEPCTX id=%llu session=%llu launch=%llu cancel_reason=%s",(unsigned long long)c.id,(unsigned long long)r.session,(unsigned long long)r.launch,runtime_cause::cancel_name(c.cancel_reason));log_line(identity);
    std::snprintf(line,sizeof(line),"DIAGSLEEP id=%llu completion_seq=%llu status=%s thread=%d generation=%u start_us=%lld ready_us=%lld dispatch_us=%lld observed_resume_us=%lld ready_to_dispatch_us=%lld clock_valid=%d method=%s bci=%d caller=%s caller_bci=%d outer=%s outer_bci=%d requested_ms=%lld effective_ms=%lld route=%s cap=%d owned_before=%d paced_after=%d deadline_ms=%lld deadline_us=%lld game_scale=%.6f draw_at_request=%llu virtual_test_clock=%d dispatch_scope=first_admitted_to_run not_exclusive_main_sleep=1",(unsigned long long)c.id,(unsigned long long)c.sequence,runtime_cause::status_name(c.status),r.thread,r.generation,(long long)r.started_us,(long long)c.ready_us,(long long)c.dispatched_us,(long long)(normal&&clock_valid?c.dispatched_us-r.started_us:-1),(long long)(normal&&clock_valid?c.dispatched_us-c.ready_us:-1),int(clock_valid),g_device_diag.labels.get(r.method),r.bci,g_device_diag.labels.get(r.callers[1]),r.caller_bcis[1],g_device_diag.labels.get(r.callers[2]),r.caller_bcis[2],(long long)r.requested_ms,(long long)r.effective_ms,runtime_cause::route_name(r.route),r.cap,int(r.owned_before),int(r.paced_after),(long long)r.deadline_ms,(long long)r.deadline_us,r.clock_scale,(unsigned long long)r.draw,int(r.virtual_clock));log_line(line);
}
static void diag_causal_snapshot(){
    // Four largest allocation sites from this live window, not a claim of
    // exclusive method CPU time. No sorting/mutation of the VM's data.
    const auto& bank=g_device_diag.allocations;std::array<const runtime_cause::AllocationSite*,4> top{};
    for(const auto& site:bank.rows)if(site.used)for(size_t n=0;n<top.size();++n)if(!top[n]||site.calls>top[n]->calls){for(size_t k=top.size()-1;k>n;--k)top[k]=top[k-1];top[n]=&site;break;}
    for(size_t n=0;n<top.size()&&top[n];++n){const auto& r=*top[n];char line[768];std::snprintf(line,sizeof(line),"DIAGALLOC window=%llu rank=%zu method=%s bci=%d element_kind=%c calls=%llu payload_bytes=%llu min_length=%llu max_length=%llu scope=successful_arrays_since_window_reset not_CPU_time=1",(unsigned long long)g_device_diag.windows,n+1,g_device_diag.labels.get(r.method),r.bci,r.kind,(unsigned long long)r.calls,(unsigned long long)r.bytes,(unsigned long long)r.min_length,(unsigned long long)r.max_length);log_line(line);}
    auto& sleeps=g_device_diag.sleeps;
    const auto first=std::max<uint64_t>(g_device_diag.sleep_reported,sleeps.completions> sleeps.history.size()?sleeps.completions-sleeps.history.size():0);
    // Retain the four longest NEW completed/canceled spans. Aggregates count
    // all spans, including short or overwritten details. No per-sleep disk I/O.
    std::array<const runtime_cause::SleepCompletion*,4> longest{};
    for(uint64_t index=first;index<sleeps.completions;++index){const auto& row=sleeps.history[size_t(index%sleeps.history.size())];const auto duration=row.dispatched_us-row.request.started_us;
        for(size_t n=0;n<longest.size();++n)if(!longest[n]||duration>longest[n]->dispatched_us-longest[n]->request.started_us){for(size_t k=longest.size()-1;k>n;--k)longest[k]=longest[k-1];longest[n]=&row;break;}}
    for(const auto* row:longest)if(row)diag_sleep_record(*row);
    g_device_diag.sleep_reported=sleeps.completions;
}
static uint64_t diag_counter_delta(uint64_t now,uint64_t& prior){const auto value=now>=prior?now-prior:now;prior=now;return value;}
static void diag_causal_window(int64_t now){
    auto& d=g_device_diag;const auto& s=g_frame_sleep_stats;const auto& pool=jit236::code_pool_stats();char line[1536];
    std::snprintf(line,sizeof(line),"DIAGCAUSE window=%llu end_us=%lld arrays=%llu array_payload_bytes=%llu array_site_overflow_calls=%llu array_site_overflow_bytes=%llu sleep_requests=%llu requested_ms=%llu effective_ms=%llu frame_owned_8_120=%llu unowned_8_120=%llu outside_8_120=%llu short_1_7=%llu spans_started=%llu spans_completed_in_window=%llu summed_completed_resume_us=%llu pending_spans=%zu canceled_spans_total=%llu span_overflow_total=%llu span_history_overwritten_total=%llu jit_create_calls=%llu jit_create_failures=%llu jit_create_wall_us=%llu jit_create_max_us_total=%llu wait_scope=cooperative_requests_and_completed_spans_can_overlap not_exclusive_wall=1",
        (unsigned long long)d.windows,(long long)now,(unsigned long long)d.allocations.calls,(unsigned long long)d.allocations.bytes,(unsigned long long)d.allocations.overflow_calls,(unsigned long long)d.allocations.overflow_bytes,(unsigned long long)d.sleeps.window_requests,(unsigned long long)s.requested_ms,(unsigned long long)s.effective_ms,(unsigned long long)s.owned,(unsigned long long)s.unowned,(unsigned long long)s.long_calls,(unsigned long long)s.short_calls,(unsigned long long)d.sleeps.window_requests,(unsigned long long)d.sleeps.window_completed,(unsigned long long)d.sleeps.window_resume_us,d.sleeps.pending(),(unsigned long long)d.sleeps.canceled,(unsigned long long)d.sleeps.overflow,(unsigned long long)d.sleeps.history_overwritten(),(unsigned long long)diag_counter_delta(pool.create_calls,d.jit_creates_seen),(unsigned long long)diag_counter_delta(pool.create_failures,d.jit_failures_seen),(unsigned long long)diag_counter_delta(pool.create_wall_us,d.jit_create_us_seen),(unsigned long long)pool.create_max_us);log_line(line);
    diag_causal_snapshot();d.allocations.clear();d.sleeps.clear_window();
}
