// Owner-thread VM integration; the watchdog below reads only atomic values
// and immutable label cells, never the heap, class tables, frames or audio.
static uint32_t diag_method(Method* method){
    if(!method)return 0;
    if(method->diagnostic_id)return method->diagnostic_id;
    char label[256];const char* owner="unknown";
    if(method->class_id>=0&&size_t(method->class_id)<VMS_CL(g_classes).size())owner=VMS_CL(g_classes)[method->class_id].name.c_str();
    std::snprintf(label,sizeof(label),"%s.%s%s",owner,method->name.c_str(),method->desc.c_str());
    method->diagnostic_id=g_device_diag.labels.add(label);
    if(!method->diagnostic_id)method->diagnostic_id=UINT32_MAX; // Cache bounded registry exhaustion, too.
    return method->diagnostic_id;
}
static void diag_watchdog_tick(){
    static runtime_diag::Watchdog watchdog;
    watchdog.poll(paint_clock_us(),g_device_diag.live,g_device_diag.labels,[](const char* line){j2melog::write_raw(line,true);});
}
static void diag_host(bool active,bool paused){
    auto& o=g_device_diag.owner;o.host_us=paint_clock_us();o.active=active;o.paused=paused;o.phase=runtime_diag::Phase::Host;g_device_diag.live.publish(o);
}
static void diag_launch(const std::string& identity,const std::string& main){
    diag_causal_reset();g_device_diag.sleep_reported=g_device_diag.sleeps.completions;
    auto& d=g_device_diag;d.flight.reset();d.detector.reset();d.throw_count=d.caught_count=d.windows=0;
    ++d.owner.launch;d.owner.host_us=d.owner.point_us=paint_clock_us();d.owner.active=true;d.owner.paused=false;d.owner.method=d.owner.caller=0;d.owner.input_us=0;d.owner.phase=runtime_diag::Phase::Launch;d.live.publish(d.owner);
    char line[1024];std::snprintf(line,sizeof(line),"DIAGSESSION schema=1 build=%s session=%llu launch=%llu mono_us=%lld suite=%s main=%s history_records=128 fault_dump_records=32 method_labels=4096 heartbeat=logger_atomic_only watchdog_us=2000000 window_period_us=1000000 persistence=stream_flush_not_power_loss_guarantee",kVer,(unsigned long long)d.owner.session,(unsigned long long)d.owner.launch,(long long)d.owner.point_us,identity.c_str(),main.c_str());log_line(line);d.flight.note(d.owner.point_us,"launch",line);
}
static void diag_checkpoint(const std::vector<Frame>& stack){
    if(stack.empty())return;
    auto& d=g_device_diag;auto& o=d.owner;const auto now=paint_clock_us();
    o.point_us=now;o.phase=runtime_diag::Phase::Java;o.method=diag_method(stack.back().method);o.caller=stack.size()>1?diag_method(stack[stack.size()-2].method):0;o.bci=stack.back().op_pc;o.depth=int(stack.size());d.live.publish(o);
    if(now-d.last_point_note<250000)return;d.last_point_note=now;
    char line[600];std::snprintf(line,sizeof(line),"method=%s caller=%s bci=%d depth=%d source=existing_safepoint not_CPU_profile=1",d.labels.get(o.method),d.labels.get(o.caller),o.bci,o.depth);d.flight.note(now,"vm_point",line);
}
static void diag_stack(const char* kind,bool emit){
    auto* stack=VMS_EXEC(g_active_stack);if(!stack)return;
    const auto now=paint_clock_us();size_t seen=0;
    for(auto f=stack->rbegin();f!=stack->rend()&&seen<16;++f,++seen){char line[640];
        std::snprintf(line,sizeof(line),"%s throw_seq=%llu depth=%zu method=%s bci=%d next_pc=%d native=%d",kind,(unsigned long long)g_device_diag.throw_count,stack->size()-seen,g_device_diag.labels.get(diag_method(f->method)),f->op_pc,f->pc,f->method?int(f->method->native):0);
        g_device_diag.flight.note(now,"stack",line);if(emit)log_line(std::string("DIAGSTACK ")+line);
    }
    if(stack->size()>16)g_device_diag.flight.note(now,"stack","truncated=1 limit=16");
}
static void diag_exception(int ref){
    auto& d=g_device_diag;++d.throw_count;char line[640];const char* cls="unknown";const char* detail="";
    if(ref>0&&size_t(ref)<VMS_HEAP(g_heap).size()){
        const auto& o=VMS_HEAP(g_heap)[ref];if(o.cls>=0&&size_t(o.cls)<VMS_CL(g_classes).size()){
            cls=VMS_CL(g_classes)[o.cls].name.c_str();
            if(auto* f=find_field(o.cls,"detail","Ljava/lang/String;"))if(f->slot>=0&&size_t(f->slot)<o.fields.size())detail=read_string(o.fields[f->slot].ref).c_str();
        }
    }
    std::snprintf(line,sizeof(line),"throw_seq=%llu class=%s detail=%s thread=%d outcome=pending",(unsigned long long)d.throw_count,cls,detail,current_context().ref);d.flight.note(paint_clock_us(),"throw",line);diag_stack("throw_origin",false);
}
static void diag_caught(const Frame& frame,int handler){
    auto& d=g_device_diag;++d.caught_count;char line[512];std::snprintf(line,sizeof(line),"throw_seq=%llu method=%s origin_bci=%d handler_bci=%d outcome=caught",(unsigned long long)d.throw_count,d.labels.get(diag_method(frame.method)),frame.op_pc,handler);d.flight.note(paint_clock_us(),"catch",line);
}
static void diag_fault(const char* reason,const char* detail,bool force){
    auto& d=g_device_diag;if(d.reporting)return;
    const auto now=paint_clock_us();size_t kind=0;
    if(!force){
        kind=!std::strcmp(reason,"AUDIO_OUTPUT_FAILURE")?7:!std::strcmp(reason,"DRAW_NO_PROGRESS")?8:!std::strcmp(reason,"LONG_DRAW_GAP")?1:!std::strcmp(reason,"CAP_DRAW_SHORTFALL")?2:
            d.owner.phase==runtime_diag::Phase::GC?4:d.owner.phase==runtime_diag::Phase::Present?5:d.owner.phase==runtime_diag::Phase::Audio?6:3;
    }
    if(!d.gate.allow(kind,now,force))return;
    d.reporting=true;const auto id=++d.incidents;char line[1200],clean[512];runtime_diag::text(clean,sizeof(clean),detail);
    std::snprintf(line,sizeof(line),"DIAGFAULT schema=1 id=%llu session=%llu launch=%llu throw_seq=%llu mono_us=%lld reason=%s detail=%s repeat_count=%llu root_cause=unattributed",(unsigned long long)id,(unsigned long long)d.owner.session,(unsigned long long)d.owner.launch,(unsigned long long)d.throw_count,(long long)now,reason,clean,(unsigned long long)d.gate.count(kind));log_line(line);
    std::snprintf(line,sizeof(line),"DIAGCTX id=%llu phase=%s thread=%d lifecycle=%d active_frames=%zu run_depth=%d pump_depth=%d paint_depth=%d canvas=%s serial_pending=%zu jit_enabled=%d jit_paused=%d jit_reopens=%llu logger_io_uncertain=%d logger_failures=%llu key_lost=%llu fault_lost=%llu label_overflow=%llu origin_samples=not_CPU_profile",(unsigned long long)id,runtime_diag::phase_name(d.owner.phase),current_context().ref,int(VMS_EXEC(g_midlet_state)),VMS_EXEC(g_active_stack)?VMS_EXEC(g_active_stack)->size():0,VMS_EXEC(g_run_depth),VMS_EXEC(g_pump_depth),VMS_RS(g_paint_depth),p1_canvas(),VMS_EXEC(g_serial_calls).size(),int(VMS_NATIVE(g_native_jit).enabled),int(VMS_NATIVE(g_native_jit).admission_closed),(unsigned long long)VMS_NATIVE(g_native_jit).stats.resource_reopens,int(j2melog::g_io_uncertain.load()),(unsigned long long)j2melog::g_failures.load(),(unsigned long long)j2melog::g_key_drops.load(),(unsigned long long)j2melog::g_fault_drops.load(),(unsigned long long)d.labels.overflow());log_line(line);
    std::snprintf(line,sizeof(line),"DIAGCTX id=%llu method=%s caller=%s bci=%d depth=%d point_age_us=%lld point_scope=last_published_boundary",(unsigned long long)id,d.labels.get(d.owner.method),d.labels.get(d.owner.caller),d.owner.bci,d.owner.depth,(long long)(d.owner.point_us?std::max<int64_t>(0,now-d.owner.point_us):-1));log_line(line);
    d.flight.dump([](const char* s){log_line(s);},32);diag_stack("fault_active",true);
    size_t n=0;for(const auto& thread:VMS_EXEC(g_threads)){if(n++==16)break;
        Method* last=thread.frames.empty()?thread.p1_last_method:thread.frames.back().method;
        std::snprintf(line,sizeof(line),"DIAGSTACK thread=%d state=%d frames=%zu method=%s bci=%d wake=%lld",thread.ref,int(thread.state),thread.frames.size(),d.labels.get(diag_method(last)),thread.frames.empty()?thread.p1_last_bci:thread.frames.back().op_pc,(long long)thread.wake);log_line(line);
    }
    std::snprintf(line,sizeof(line),"DIAGEND id=%llu history_overwritten=%llu throws=%llu caught=%llu critical_flush_requested=%d",(unsigned long long)id,(unsigned long long)d.flight.overwritten(),(unsigned long long)d.throw_count,(unsigned long long)d.caught_count,int(force));log_line(line);
    diag_causal_snapshot();
    // Fault-only bounded barrier, never a wait/join on audio or worker jobs.
    if(force&&!j2melog::drain(std::chrono::milliseconds(250)))log_line("DIAGIO fault_flush=timeout_or_uncertain evidence_may_be_incomplete=1");
    d.reporting=false;
}
static DiagnosticNativeToken diag_native_enter(Method* method){
    auto& d=g_device_diag;DiagnosticNativeToken token{d.owner,paint_clock_us(),diag_method(method)};
    auto& o=d.owner;o.phase=runtime_diag::Phase::Native;o.point_us=token.began;o.method=token.method;
    if(auto* stack=VMS_EXEC(g_active_stack)){o.depth=int(stack->size());if(!stack->empty()){o.caller=diag_method(stack->back().method);o.bci=stack->back().op_pc;}}
    d.live.publish(o);return token;
}
static DiagnosticNativeToken diag_phase_enter(runtime_diag::Phase phase){
    auto& d=g_device_diag;DiagnosticNativeToken token{d.owner,paint_clock_us(),d.owner.method,phase};
    d.owner.phase=phase;d.owner.point_us=token.began;d.live.publish(d.owner);return token;
}
static void diag_native_leave(const DiagnosticNativeToken& token){
    auto& d=g_device_diag;const auto now=paint_clock_us();
    if(now-token.began>=100000){char line[384];std::snprintf(line,sizeof(line),"phase=%s method=%s inclusive_wall_us=%lld not_CPU_time=1",runtime_diag::phase_name(token.phase),d.labels.get(token.method),(long long)(now-token.began));d.flight.note(now,"slow_boundary",line);diag_fault("LONG_HOST_BOUNDARY",line,false);}
    // Preserve current input/host timestamps when a native callback reentered.
    auto restored=token.previous;
    if(d.owner.session!=restored.session||d.owner.launch!=restored.launch)restored=d.owner;
    restored.host_us=d.owner.host_us;restored.active=d.owner.active;restored.paused=d.owner.paused;restored.input_us=d.owner.input_us;restored.key=d.owner.key;restored.release=d.owner.release;restored.point_us=now;d.owner=restored;d.live.publish(d.owner);
}
static void diag_window(int64_t now,const PaintStatsWindow& stats,const RuntimeProfileWindow& work,uint64_t presents){
    auto& d=g_device_diag;++d.windows;const auto& frames=VMS_RS(g_frame_metrics).pending;const auto& pr=VMS_PLAT(g_present_stats);const auto& jit=VMS_NATIVE(g_native_jit);
    const auto cpu=j2me_cpu_clock::difference(VMS_PLAT(g_cpu_window_start_ns),j2me_cpu_clock::now_ns());
    char line[2048];std::snprintf(line,sizeof(line),"DIAGWIN schema=1 session=%llu launch=%llu window=%llu start_us=%lld end_us=%lld window_us=%lld mode=%d lifecycle=%d paused=%d raw_draws=%llu changed=%llu presents=%llu draw_p95_upper_us=%llu draw_max_us=%llu gc_max_us=%llu host_cpu_ns=%lld worker_cpu_ns=%lld worker_wall_us=%lld gc_us=%lld capwait_us=%lld pollwait_us=%lld nativewait_us=%lld present_us=%llu acquire_us=%llu prepare_us=%llu submit_us=%llu gpu_wait_us=%llu fast_ops=%llu slow_ops=%llu jit_failures=%llu jit_paused=%d jit_reopens=%llu audio_snapshot_valid=%d audio_cached_idle=%d audio_cached_paused=%d throws=%llu caught=%llu logger_us=%llu io_uncertain=%d key_lost=%llu fault_lost=%llu rate_scope=raw_draw_and_host_present logic_fps=unavailable root_cause=unattributed",
        (unsigned long long)d.owner.session,(unsigned long long)d.owner.launch,(unsigned long long)d.windows,(long long)(now-stats.elapsed_us),(long long)now,(long long)stats.elapsed_us,VMS_EXEC(g_speed_cap),int(VMS_EXEC(g_midlet_state)),int(VMS_PLAT(g_host_paused)),(unsigned long long)frames.completed_draws,(unsigned long long)frames.content_frames,(unsigned long long)presents,(unsigned long long)frames.draw_intervals.quantile_upper(95),(unsigned long long)frames.draw_intervals.max_us,(unsigned long long)frames.gc_pauses.max_us,(long long)cpu,(long long)work.worker_cpu_ns,(long long)work.worker_gross_us,(long long)work.gc_us,(long long)work.worker_capwait_us,(long long)work.worker_pollwait_us,(long long)work.worker_nativewait_us,(unsigned long long)pr.total_us,(unsigned long long)pr.acquire_us,(unsigned long long)pr.prepare_us,(unsigned long long)pr.submit_us,(unsigned long long)pr.gpu_wait_us,(unsigned long long)VMS_EXEC(g_fast_ops),(unsigned long long)VMS_EXEC(g_slow_ops),(unsigned long long)jit.stats.permission_failures,int(jit.admission_closed),(unsigned long long)jit.stats.resource_reopens,int(p7audio::q_valid),int(p7audio::q_idle),int(p7audio::q_paused),(unsigned long long)d.throw_count,(unsigned long long)d.caught_count,(unsigned long long)j2melog::main_us_snapshot(),int(j2melog::g_io_uncertain.load()),(unsigned long long)j2melog::g_key_drops.load(),(unsigned long long)j2melog::g_fault_drops.load());
    log_line(line);d.flight.note(now,"window",line);
    const uint64_t jit_ops=jit.stats.ops>=g_stat_jit_ops_seen?jit.stats.ops-g_stat_jit_ops_seen:jit.stats.ops;
    g_stat_jit_ops_seen=jit.stats.ops;
    std::snprintf(line,sizeof(line),"DIAGEXEC schema=1 window=%llu end_us=%lld paint_calls=%llu gamecanvas_flushes=%llu paint_wall_sum_us=%lld paint_wall_p95_upper_us=%llu paint_wall_max_us=%lld paint_cpu_ns=%lld worker_paint_cpu_ns=%lld gc_cpu_ns=%lld jit_ops=%llu fast_ops=%llu slow_ops=%llu cpu_unavailable=%llu paint_observed_capwait_us=%lld paint_observed_capwait_count=%llu paint_observed_pollwait_us=%lld paint_observed_pollwait_count=%llu paint_observed_nativewait_us=%lld paint_observed_nativewait_count=%llu main_paint_observed_capwait_us=%lld main_paint_observed_capwait_count=%llu main_paint_observed_pollwait_us=%lld main_paint_observed_pollwait_count=%llu main_paint_observed_nativewait_us=%lld main_paint_observed_nativewait_count=%llu observed_waits_exclusive=0 observed_waits_cover_OS_IO=0 counters_scope=window jit_ops_includes_regions=1 worker_is_Java_scope_on_host_thread=1 paint_worker_cpu_overlap=1 draw_max_is_interval=1",
        (unsigned long long)d.windows,(long long)now,(unsigned long long)stats.canvas_paints,(unsigned long long)stats.gamecanvas_flushes,(long long)stats.paint_us,(unsigned long long)stats.paint_durations.quantile_upper(95),(long long)stats.paint_max_us,(long long)work.paint_cpu_ns,(long long)work.worker_paint_cpu_ns,(long long)work.gc_cpu_ns,(unsigned long long)jit_ops,(unsigned long long)VMS_EXEC(g_fast_ops),(unsigned long long)VMS_EXEC(g_slow_ops),(unsigned long long)work.cpu_unavailable,
        (long long)work.paint_waits.cap_us,(unsigned long long)work.paint_waits.cap_count,(long long)work.paint_waits.poll_us,(unsigned long long)work.paint_waits.poll_count,(long long)work.paint_waits.native_us,(unsigned long long)work.paint_waits.native_count,
        (long long)work.main_paint_waits.cap_us,(unsigned long long)work.main_paint_waits.cap_count,(long long)work.main_paint_waits.poll_us,(unsigned long long)work.main_paint_waits.poll_count,(long long)work.main_paint_waits.native_us,(unsigned long long)work.main_paint_waits.native_count);
    log_line(line);d.flight.note(now,"execution",line);
    const auto& gfx=VMS_RS(g_graphics_profile).global_pending;
    std::snprintf(line,sizeof(line),"DIAGDETAIL window=%llu end_us=%lld cap_budget_frames=%llu cap_budget_scope=budget_not_logic gfx_calls=%llu gfx_sampled_calls=%llu gfx_sampled_native_ns=%lld gfx_offscreen_calls=%llu gfx_sample_rate=1/64 gap_observations=%llu cap_observations=%llu native_observations=%llu gc_observations=%llu present_observations=%llu audio_observations=%llu logger_enqueue_us_total=%llu",(unsigned long long)d.windows,(long long)now,(unsigned long long)VMS_EXEC(g_speed_budget_window_frames),(unsigned long long)gfx.native_calls,(unsigned long long)gfx.sampled_calls,(long long)gfx.native_ns,(unsigned long long)gfx.offscreen_calls,(unsigned long long)d.gate.count(1),(unsigned long long)d.gate.count(2),(unsigned long long)d.gate.count(3),(unsigned long long)d.gate.count(4),(unsigned long long)d.gate.count(5),(unsigned long long)d.gate.count(6),(unsigned long long)j2melog::main_us_snapshot());log_line(line);d.flight.note(now,"details",line);
    std::snprintf(line,sizeof(line),"DIAGPROGRESS window=%llu end_us=%lld phase=%s method=%s caller=%s bci=%d depth=%d point_age_us=%lld last_key=%d release=%d input_age_us=%lld threads=%zu scope=last_published_boundary not_CPU_hotspot=1",(unsigned long long)d.windows,(long long)now,runtime_diag::phase_name(d.owner.phase),d.labels.get(d.owner.method),d.labels.get(d.owner.caller),d.owner.bci,d.owner.depth,(long long)(d.owner.point_us?std::max<int64_t>(0,now-d.owner.point_us):-1),d.owner.key,int(d.owner.release),(long long)(d.owner.input_us?std::max<int64_t>(0,now-d.owner.input_us):-1),VMS_EXEC(g_threads).size());log_line(line);
    runtime_diag::AudioSnapshot audio;
    if(d.audio.read(audio)&&audio.request_us){
        std::snprintf(line,sizeof(line),"DIAGAUDIO window=%llu end_us=%lld snapshot_age_us=%lld request_us=%llu last_service_us=%llu queued_buffers=%llu pumps=%llu queue_empty_polls=%llu render_blocks=%llu nonzero_blocks=%llu output_errors=%llu max_pump_gap_us=%llu max_mix_us=%llu counters_scope=sink_since_legacy_reset queue_empty_is_risk_not_proven_dropout=1 no_VM_audio_fence=1",(unsigned long long)d.windows,(long long)now,(long long)std::max<int64_t>(0,now-int64_t(audio.request_us)),(unsigned long long)audio.request_us,(unsigned long long)audio.service_us,(unsigned long long)audio.queued,(unsigned long long)audio.pumps,(unsigned long long)audio.empty_polls,(unsigned long long)audio.blocks,(unsigned long long)audio.nonzero_blocks,(unsigned long long)audio.errors,(unsigned long long)audio.max_gap_us,(unsigned long long)audio.max_mix_us);log_line(line);d.flight.note(now,"audio_sink",line);
    }
    runtime_diag::Window w;w.duration_us=stats.elapsed_us;w.worker_cpu_ns=work.cpu_samples?work.worker_cpu_ns:-1;w.draws=frames.completed_draws;w.changes=frames.content_frames;w.presents=presents;w.max_draw_interval_us=frames.draw_intervals.max_us;w.cap=VMS_EXEC(g_speed_cap);w.active=VMS_EXEC(g_midlet_state)==MidletState::Active;w.paused=VMS_PLAT(g_host_paused)||!w.active;
    w.budget_valid=w.cap==30||w.cap==60;w.budget_frames=VMS_EXEC(g_speed_budget_window_frames);
    const auto observation=d.detector.observe(now,w);
    if(observation){
        std::snprintf(line,sizeof(line),"window=%llu elapsed_us=%lld raw_draws=%llu cap_budget_frames=%llu cap=%d draw_max_us=%llu worker_cpu_ns=%lld gc_us=%lld selected_cap_is_not_proven_logic_FPS=1",(unsigned long long)d.windows,(long long)w.duration_us,(unsigned long long)w.draws,(unsigned long long)w.budget_frames,w.cap,(unsigned long long)w.max_draw_interval_us,(long long)w.worker_cpu_ns,(long long)work.gc_us);
        diag_fault(observation&runtime_diag::WindowDetector::NoProgress?"DRAW_NO_PROGRESS":observation&runtime_diag::WindowDetector::LongGap?"LONG_DRAW_GAP":"CAP_DRAW_SHORTFALL",line,false);
    }
    diag_causal_window(now); // Observe before the original per-window sleep reset.
    diag_host(w.active,w.paused);
}
