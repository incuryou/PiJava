// Cooperative Java threads. Included after VM class/heap helpers, before natives.
// There is one host executor; Java stacks yield at explicit blocking operations
// or instruction/time budgets. Native-to-Java continuations remain synchronous.
static Value thread_field(int ref, const char* name, const char* desc) {
    Field* f = find_field(VMS_HEAP(g_heap)[ref].cls, name, desc);
    return f ? VMS_HEAP(g_heap)[ref].fields[f->slot] : Value{};
}
static void set_thread_field(int ref, const char* name, const char* desc, Value value) {
    if (Field* f = find_field(VMS_HEAP(g_heap)[ref].cls,name,desc)) VMS_HEAP(g_heap)[ref].fields[f->slot] = value;
}
static bool thread_alive(int ref) { return ref && thread_field(ref,"vmState","I").i == 1; }
static bool thread_interrupted(int ref) { return ref && thread_field(ref,"vmInterrupted","Z").i != 0; }
static void set_interrupted(int ref, bool value) { set_thread_field(ref,"vmInterrupted","Z",Value::of_i(value)); }
static int current_thread_ref() {
    auto& t = current_context();
    if (!t.ref) {
        t.ref = alloc_obj(ensure_class("java/lang/Thread"));
        set_thread_field(t.ref,"vmState","I",Value::of_i(1));
        set_thread_field(t.ref,"vmPriority","I",Value::of_i(5));
        set_thread_field(t.ref,"name","Ljava/lang/String;",Value::of_ref(make_string("main")));
    }
    return t.ref;
}
static JavaThreadContext* thread_context(int ref) {
    if (VMS_EXEC(g_main_thread).ref == ref) return &VMS_EXEC(g_main_thread);
    for (auto& t : VMS_EXEC(g_threads)) if (t.ref == ref && t.state != ThreadState::Terminated) return &t;
    return nullptr;
}
static int class_object(const std::string& name) {
    auto found = VMS_CL(g_class_objects).find(name);
    if (found != VMS_CL(g_class_objects).end()) return found->second;
    int ref = alloc_obj(ensure_class("java/lang/Class"));
    VMS_HEAP(g_heap)[ref].text = name;
    VMS_CL(g_class_objects)[name] = ref;
    return ref;
}
struct RunScope {
    std::vector<Frame>* prev;
    bool previous_can_suspend;
    JavaThreadContext* owner;
    RunScope(std::vector<Frame>* stack, bool resumable): prev(VMS_EXEC(g_active_stack)), previous_can_suspend(VMS_EXEC(g_can_suspend)), owner(&current_context()) {
        ++VMS_EXEC(g_save_clock).exec_epoch;
        ++VMS_EXEC(g_run_depth); VMS_EXEC(g_active_stack) = stack; VMS_EXEC(g_can_suspend) = resumable;
        VMS_HEAP(g_root_stacks).push_back({stack,owner});
    }
    bool has_parent() const {
        for (size_t i=0;i+1<VMS_HEAP(g_root_stacks).size();++i) if (VMS_HEAP(g_root_stacks)[i].second == owner) return true;
        return false;
    }
    ~RunScope() { VMS_HEAP(g_root_stacks).pop_back(); VMS_EXEC(g_active_stack) = prev; VMS_EXEC(g_can_suspend) = previous_can_suspend; --VMS_EXEC(g_run_depth); }
};
static int64_t thread_deadline(int64_t timeout) {
    int64_t now=monotonic_ms();
    return timeout > INT64_MAX-now ? INT64_MAX : now+timeout;
}
static void remove_waiter(JavaThreadContext& t) {
    auto found=VMS_EXEC(g_monitors).find(t.monitor);
    if(found==VMS_EXEC(g_monitors).end())return;
    auto& q=found->second.waiters;
    q.erase(std::remove(q.begin(),q.end(),&t),q.end());
}
static void wait_to_blocked(JavaThreadContext& t) {
    remove_waiter(t); t.state=ThreadState::Blocked; t.wake=0; t.wake_us=0;
}
static void refresh_thread(JavaThreadContext& t) {
    const bool was_sleeping=t.state==ThreadState::Sleeping;
    const bool diagnostic_wait=was_sleeping||t.state==ThreadState::Joining;
    int64_t now=monotonic_ms();
    if(t.state==ThreadState::Initializing && VMS_CL(g_classes)[t.init_wait].init_state!=1) {t.init_wait=-1;t.state=ThreadState::Runnable;}
    if(t.state==ThreadState::Sleeping && t.wake_us) {
        if(paint_clock_us()>=t.wake_us) { t.sleep_sample_due=t.wake; t.sleep_request=0; t.wake_us=0; t.state=ThreadState::Runnable; t.wake=0; }
    } else if(t.state==ThreadState::Sleeping && now>=t.wake) { t.sleep_sample_due=t.wake; t.sleep_request=0; t.state=ThreadState::Runnable; t.wake=0; }
    if(t.state==ThreadState::Joining && (!thread_alive(t.join_target) || (t.wake && now>=t.wake))) {
        t.state=ThreadState::Runnable; t.join_target=0; t.wake=0;
    }
    if(t.state==ThreadState::Waiting && (t.wake_us ? paint_clock_us()>=t.wake_us : t.wake && now>=t.wake)) {
        t.wait_frame.timed_out=t.wait_frame.paced;
        if(t.wait_frame.awaiting_paint)speed_frame_wait_cancel_request(t);
        wait_to_blocked(t);
    }
    if(t.state==ThreadState::Blocked) {
        auto& m=VMS_EXEC(g_monitors)[t.monitor];
        if(!m.owner || m.owner==&t) {
            if(t.reacquire) { m.owner=&t; m.depth=t.restore_depth; }
            t.monitor=0; t.restore_depth=0; t.reacquire=false; t.state=ThreadState::Runnable;
            if(t.wait_frame.paced&&t.wait_frame.timed_out)t.frame_prep.woke(paint_clock_us());
            t.wait_frame.paced=t.wait_frame.timed_out=false;
        } else if(t.wait_frame.paced) {
            speed_frame_wait_cancel_request(t);t.frame_prep.reset();
        }
    }
    if(was_sleeping && t.state==ThreadState::Runnable)t.frame_prep.woke(paint_clock_us());
    if(diagnostic_wait&&t.state==ThreadState::Runnable)diag_sleep_ready(t);
}
static bool monitor_enter(int ref) {
    auto& t=current_context(); auto& m=VMS_EXEC(g_monitors)[ref];
    if(!m.owner || m.owner==&t) { m.owner=&t; ++m.depth; return true; }
    t.state=ThreadState::Blocked; t.monitor=ref; t.restore_depth=1; t.reacquire=false;
    return false;
}
// A worker can be pumped while a native event callback is sleeping or at an
// instruction budget. Entering its synchronous paint here can wait for a
// monitor owned by that suspended event (including locks inside collections).
// Leave the original serviceRepaints invoke pending until event delivery has
// returned or the request has been serviced. The event's own serviceRepaints
// keeps the MIDP synchronous exception.
static bool service_repaints_pending(int target){
    return target>0&&target==VMS_RS(g_current)&&VMS_RS(g_repaint);
}
static bool service_repaints_wait_event(const Method* native,int target){
    auto* event=VMS_EXEC(g_exec_ctx).event_callback_owner;
    if(!event||event==&current_context()||!VMS_EXEC(g_can_suspend)||VMS_RS(g_paint_depth)||
       !service_repaints_pending(target)||!native||
       native->name!="serviceRepaints"||native->desc!="()V"||
       VMS_CL(g_classes)[native->class_id].name!="javax/microedition/lcdui/Canvas")return false;
    current_context().service_repaints_deferred=true;
    return true;
}
// A resumable worker must wait BEFORE entering a nonserializable native paint
// continuation when paint's first action necessarily acquires this Canvas.
// Otherwise a key callback sleeping with that lock pumps the worker, whose
// synchronous paint parks inside C++ waiting for the suspended ancestor.
// This is only admission: no monitor is acquired here, no repaint is consumed,
// and serviceRepaints still completes its paint before its caller proceeds.
static int service_repaints_wait_monitor(const Method* native,int target){
    if(!VMS_EXEC(g_can_suspend)||VMS_RS(g_paint_depth)||!VMS_RS(g_repaint)||target!=VMS_RS(g_current)||target<=0||
       !native||native->name!="serviceRepaints"||native->desc!="()V"||
       VMS_CL(g_classes)[native->class_id].name!="javax/microedition/lcdui/Canvas")return 0;
    auto locked=VMS_EXEC(g_monitors).find(target);
    if(locked==VMS_EXEC(g_monitors).end()||!locked->second.owner||locked->second.owner==&current_context())return 0;
    Method* paint=find_method(VMS_HEAP(g_heap)[target].cls,"paint","(Ljavax/microedition/lcdui/Graphics;)V",true);
    if(!paint||(paint->flags&0x0008))return 0;
    bool entry_lock=(paint->flags&0x0020)!=0;
    // javac's exact entry synchronized(this) prefix: aload_0, dup,
    // astore_<n> or astore n, monitorenter. Do not infer arbitrary locks.
    const auto& code=paint->code;
    if(!entry_lock&&code.size()>=4&&code[0]==42&&code[1]==89){
        if(code[2]>=75&&code[2]<=78)entry_lock=code[3]==194;
        else if(code[2]==58&&code.size()>=5)entry_lock=code[4]==194;
    }
    return entry_lock?target:0;
}
static void monitor_exit(int ref) {
    auto it=VMS_EXEC(g_monitors).find(ref);
    if(it==VMS_EXEC(g_monitors).end() || it->second.owner!=&current_context()) {
        throw_new("java/lang/IllegalMonitorStateException","monitor not owned"); return;
    }
    if(--it->second.depth==0) it->second.owner=nullptr;
}
static void complete_class_initialization(Frame& fr, bool success, int exception) {
    if(fr.init_class<0)return;
    auto& c=VMS_CL(g_classes)[fr.init_class];c.init_state=success?2:3;c.init_owner=nullptr;c.init_error=exception;
    fr.init_class=-1;
}
static void release_frame_monitor(Frame& fr) {
    auto& t=current_context();
    if(t.wait_frame.method==fr.method)speed_frame_wait_cancel_request(t);
    if(t.compensated_sleep.method==fr.method)t.compensated_sleep.cancel();
    if(t.event_frame_sleep.method==fr.method)t.event_frame_sleep.cancel();
    if(t.cached_elapsed.method==fr.method)t.cached_elapsed.cancel();
    if(t.residual_sleep.method==fr.method)t.residual_sleep.cancel();
    if(t.rechecked_deadline.method==fr.method)t.rechecked_deadline.cancel();
    if(t.field_gate_anchor.method==fr.method)t.field_gate_anchor.reset();
    if(t.auto_fps.verified_gate_seen && t.auto_fps.gate_method==fr.method) {
        speed_invalidate_group(t);
    }
    if(fr.sync_acquired) {
        auto it=VMS_EXEC(g_monitors).find(fr.sync_monitor);
        if(it!=VMS_EXEC(g_monitors).end() && it->second.owner==&current_context()) {
            if(--it->second.depth==0)it->second.owner=nullptr;
        }
        fr.sync_acquired=false;
    }
}
static void update_thread_live() {
    VMS_EXEC(g_thread_live)=false;
    for(auto& t:VMS_EXEC(g_threads)) if(t.state!=ThreadState::Terminated) { VMS_EXEC(g_thread_live)=true; break; }
}
static void finish_thread(JavaThreadContext& t) {
    t.service_repaints_deferred=false;
    diag_sleep_cancel(t,runtime_cause::CancelReason::Terminated);
    speed_timer_block(t);
    t.residual_sleep.reset();t.rechecked_deadline.reset();
    t.compensated_sleep.reset();t.cached_elapsed.reset();
    t.event_frame_sleep.reset();
    t.field_gate_anchor.reset();
    speed_frame_wait_cancel_request(t);t.wait_frame.reset();
    p1_thread_end(t);
    remove_waiter(t);
    for(auto& kv:VMS_EXEC(g_monitors)) if(kv.second.owner==&t) { kv.second.owner=nullptr;kv.second.depth=0; }
    speed_invalidate_group(t);
    t.frames.clear(); t.state=ThreadState::Terminated; t.monitor=t.join_target=0; t.wake=0;
    t.reacquire=t.interrupt_exception=false;
    set_thread_field(t.ref,"vmState","I",Value::of_i(2));
    update_thread_live();
}
static void deliver_interrupt() {
    auto& t=current_context();
    if(t.interrupt_exception && t.state==ThreadState::Runnable) {
        t.interrupt_exception=false; set_interrupted(current_thread_ref(),false);
        throw_new("java/lang/InterruptedException","interrupted");
    }
}
static bool pump_one_thread() {
    if(r4::state.active&&r4::state.worker_phase&&r4::state.worker_left<=0)return false;
    refresh_thread(VMS_EXEC(g_main_thread));
    for(auto& t:VMS_EXEC(g_threads)) refresh_thread(t);
    if(VMS_EXEC(g_threads).empty()) {update_thread_live(); return false;}
    // Iterate stable list nodes directly. Nested pumps may append workers;
    // this invocation returns after running one, so no shared scratch list or
    // per-yield vector allocation is needed.
    const size_t count=VMS_EXEC(g_threads).size(), first=VMS_EXEC(g_round_robin)%count;
    auto cursor=VMS_EXEC(g_threads).begin();
    for(size_t n=0;n<first;++n)++cursor;
    for(size_t i=0;i<count;++i) {
        const size_t index=(first+i)%count;
        auto& t=*cursor;
        if(++cursor==VMS_EXEC(g_threads).end())cursor=VMS_EXEC(g_threads).begin();
        if(t.executing || t.state!=ThreadState::Runnable)continue;
        if(t.service_repaints_deferred){
            // A request may have been consumed by the event or the host. It
            // is complete only after paint returns; unrelated producers can
            // still run while that paint explicitly yields.
            const int target=t.frames.empty()||t.frames.back().stack.empty()?0:t.frames.back().stack.back().ref;
            if(VMS_RS(g_paint_depth)&&(target==VMS_EXEC(g_exec_ctx).active_paint_canvas||
               service_repaints_pending(target)))continue;
            auto* event=VMS_EXEC(g_exec_ctx).event_callback_owner;
            if(event&&event!=&t&&service_repaints_pending(target))continue;
            t.service_repaints_deferred=false;
        }
        diag_sleep_dispatch(t);
        if(t.sleep_sample_due) {VMS_EXEC(g_wake_stats).dispatched(t.sleep_sample_due,monotonic_ms());t.sleep_sample_due=0;}
        VMS_EXEC(g_round_robin)=(index+1)%count;
        const ExecutionContext::ThreadSlice saved_slice=VMS_EXEC(g_exec_ctx).save_slice();  // whole per-thread slice
        CityHostSpan city_host("worker",t.ref);
        ++VMS_EXEC(g_runtime_profile).pending.worker_turns;
        VMS_EXEC(g_exec_ctx).enter_thread(&t); t.executing=true; ++VMS_EXEC(g_pump_depth);
        Method* entry=nullptr;
        if(!t.begun) { entry=find_method(VMS_HEAP(g_heap)[t.ref].cls,"run","()V",true); if(!entry)die("thread has no run");t.begun=true;p1_event("worker run_enter ref=%d class=%s method=%s%s",t.ref,VMS_CL(g_classes)[entry->class_id].name.c_str(),entry->name.c_str(),entry->desc.c_str()); }
        if(entry){const int target=thread_field(t.ref,"target","Ljava/lang/Runnable;").ref;
            p1_event("worker target ref=%d target=%d target_class=%s",t.ref,target,target>0&&VMS_HEAP(g_heap)[target].cls>=0?VMS_CL(g_classes)[VMS_HEAP(g_heap)[target].cls].name.c_str():"-");}
        VMS_EXEC(g_resume_request)=true;
        run(entry,entry?std::vector<Value>{Value::of_ref(t.ref)}:std::vector<Value>{});
        if(t.frames.empty() && t.state!=ThreadState::Terminated)finish_thread(t);
        if(VMS_EXEC(g_yield))++VMS_EXEC(g_runtime_profile).pending.worker_yields;
        --VMS_EXEC(g_pump_depth); t.executing=false; VMS_EXEC(g_exec_ctx).restore_slice(saved_slice);
        update_thread_live();return true;
    }
    return false;
}
static void suspend_current(bool run_others) {
    CityWaitSpan city_wait;
    ++VMS_EXEC(g_runtime_profile).pending.worker_suspends;
    auto& t=current_context();
    const bool timer_wait=t.state==ThreadState::Waiting&&t.timer.queue_wait&&t.monitor==t.timer.draw.queue;
    if(t.state!=ThreadState::Runnable)t.residual_sleep.cancel();
    if(t.state!=ThreadState::Runnable)t.event_frame_sleep.cancel();
    t.timer.queue_wait=false;
    if(t.state!=ThreadState::Runnable&&!(t.state==ThreadState::Waiting&&(t.wait_frame.paced||t.wait_frame.awaiting_paint)))speed_frame_wait_cancel_request(t);
    if(t.state!=ThreadState::Runnable&&!timer_wait)speed_timer_block(t);
    if(t.state!=ThreadState::Runnable && !(t.state==ThreadState::Sleeping && t.frame_sleep_bridge)) {speed_invalidate_group(t);}
    if(VMS_EXEC(g_can_suspend)) {VMS_EXEC(g_yield)=true;city_wait.end("yield");return;}
    if(r4::state.active)r4_abort("UNSUPPORTED_CALLBACK_BLOCK");
    // Native callbacks cannot serialize their C++ continuation. Keep that stack
    // parked while pumping other Java threads, and suppress GC until it returns.
    if(run_others)pump_one_thread();
    while(t.state!=ThreadState::Runnable && t.state!=ThreadState::Terminated) {
        city_wait.progress();
        refresh_thread(t);
        if(t.state==ThreadState::Runnable)break;
        if(!pump_one_thread()) {
            RuntimeProfileScope wait(VMS_EXEC(g_runtime_profile),RuntimeProfile::Kind::NativeWait);
            { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        }
    }
    if(t.state==ThreadState::Runnable)diag_sleep_dispatch(t);
    deliver_interrupt();
    city_wait.end(t.state==ThreadState::Terminated?"terminated":"resumed");
}
static int64_t next_thread_delay() {
    int64_t now=monotonic_ms(),delay=0;
    for(auto& t:VMS_EXEC(g_threads)) if(t.wake>now && (!delay || t.wake-now<delay))delay=t.wake-now;
    return delay;
}
static void thread_native(Method* method,Frame& fr) {
    auto arg=[&](int i){return i<(int)fr.locals.size()?fr.locals[i]:Value{};};
    auto ret=[&](Value v){fr.stack.push_back(v);};
    const auto& name=method->name;
    if(name=="currentThread") {ret(Value::of_ref(current_thread_ref()));return;}
    if(name=="activeCount") {int count=1;for(auto& t:VMS_EXEC(g_threads))if(t.state!=ThreadState::Terminated)++count;ret(Value::of_i(count));return;}
    if(name=="holdsLock") {
        int ref=arg(0).ref;if(!ref){throw_new("java/lang/NullPointerException","holdsLock");return;}
        auto it=VMS_EXEC(g_monitors).find(ref);ret(Value::of_i(it!=VMS_EXEC(g_monitors).end()&&it->second.owner==&current_context()));return;
    }
    if(name=="start") {
        int ref=arg(0).ref;
        if(thread_field(ref,"vmState","I").i!=0){throw_new("java/lang/IllegalThreadStateException","thread already started");return;}
        set_thread_field(ref,"vmState","I",Value::of_i(1));
        VMS_EXEC(g_threads).emplace_back();VMS_EXEC(g_threads).back().ref=ref;VMS_EXEC(g_thread_live)=true;
        p1_event("worker start ref=%d class=%s live_after=1",ref,VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name.c_str());return;
    }
    if(name=="isAlive") {ret(Value::of_i(thread_alive(arg(0).ref)));return;}
    if(name=="isInterrupted"||name=="interrupted") {
        int ref=name=="interrupted"?current_thread_ref():arg(0).ref;
        bool value=thread_interrupted(ref);if(name=="interrupted")set_interrupted(ref,false);ret(Value::of_i(value));return;
    }
    if(name=="interrupt") {
        int ref=arg(0).ref;auto* t=thread_context(ref);
        if(!t)return;
        diag_sleep_cancel(*t,runtime_cause::CancelReason::Interrupted);
        t->frame_prep.reset();speed_frame_wait_cancel_request(*t);t->residual_sleep.cancel();t->compensated_sleep.cancel();speed_timer_block(*t);
        t->event_frame_sleep.cancel();
        if(t->frame_sleep_bridge)speed_invalidate_group(*t);
        set_interrupted(ref,true);
        if(t->state==ThreadState::Waiting || (t->state==ThreadState::Blocked&&t->reacquire)) {
            t->interrupt_exception=true;if(t->state==ThreadState::Waiting)wait_to_blocked(*t);
        } else if(t->state==ThreadState::Sleeping||t->state==ThreadState::Joining) {
            t->interrupt_exception=true;t->state=ThreadState::Runnable;t->wake=0;t->wake_us=0;t->join_target=0;t->sleep_request=0;
        }
        return;
    }
    if(name=="yield") {
        auto& t=current_context();const auto& s=t.wait_frame;
        t.event_frame_sleep.cancel();
        const auto* stack=VMS_EXEC(g_active_stack);
        // This exact common-tail yield follows remainder wait or overtime;
        // it is not a repeated busy-poll yield. Keep all existing learner
        // evidence and all other methods/sites, including early-abort paths.
        const bool remainder_tail=s.pattern.branched&&s.pattern.yield_pc>=0&&stack&&stack->size()==s.depth&&!stack->empty()&&
            stack->back().method==s.method&&stack->back().op_pc==s.pattern.yield_pc&&
            !stack->back().locals.empty()&&stack->back().locals[0].ref==s.canvas&&
            s.canvas==VMS_RS(g_current)&&s.canvas==VMS_RS(g_shown)&&!VMS_RS(g_paint_depth);
        if(speed_period_us()&&!remainder_tail)t.auto_fps.cooperate();
        suspend_current(true);return;
    }
    if(name=="sleep" || name=="join") {
        bool join=name=="join";int64_t ms=as_long(arg(join?1:0));const int64_t p84_req_ms=ms;
        if(join||ms<=0)current_context().event_frame_sleep.cancel();
        auto diagnostic_route=join?runtime_cause::Route::Join:ms>120?runtime_cause::Route::LongUnpaced:runtime_cause::Route::Unpaced;
        if(ms<0){throw_new("java/lang/IllegalArgumentException","negative timeout");return;}
        if(join&&!thread_alive(arg(0).ref))return;
        int ref=current_thread_ref();
        if(thread_interrupted(ref)){current_context().event_frame_sleep.cancel();current_context().frame_prep.reset();set_interrupted(ref,false);throw_new("java/lang/InterruptedException","interrupted");return;}
        auto& t=current_context();
        const bool diagnostic_owned=t.frame_due_us!=0; // Same earned-deadline test used by speed_frame_wake_us.
        speed_timer_block(t);
        if(join)t.frame_prep.reset();
        if(!join) {
            // A sleep within the current drawing owner's bounded frame cycle
            // closes a drawn group, not its timer proof. The next drawing
            // still needs fresh polling/ready evidence to retain the boost.
            const int period=speed_period_us();
            const bool bridge=period && ms>0 && ms<=120 && &t==VMS_EXEC(g_speed_budget_owner) &&
                (t.frame_due_us || t.frame_sleep_bridge) && t.auto_fps.verified_gate_seen &&
                t.frame_group_started_us>0 && paint_clock_us()-t.frame_group_started_us<=std::max<int64_t>(120000,int64_t(period)*4);
            if(bridge) {
                t.frame_sleep_bridge=true;t.gate_recovery.reset();
                if(t.auto_fps.group_drawn){t.auto_fps.group_open=false;t.auto_fps.group_complete=true;}
            } else speed_invalidate_group(t);
        }
        if(!join && speed_period_us() && ms>=0 && ms<=7)t.auto_fps.cooperate();
        t.wake_us = 0;
        t.sleep_request = 0;
        if (!r4::state.active && !join && ms > 0) {
            speed_compensated_sleep_begin(ms);
            const int64_t event=speed_event_frame_sleep_wake_us(ms);
            const int64_t checked=event?0:speed_rechecked_sleep_wake_us(ms);
            const int64_t cached=(event||checked)?0:speed_cached_elapsed_sleep_wake_us(ms);
            const int64_t residual=(event||checked||cached)?0:(ms<8?speed_residual_sleep_wake_us(ms):0);
            const int64_t due = event?event:checked?checked:cached?cached:residual?residual:speed_frame_wake_us(ms);
            if(event)diagnostic_route=runtime_cause::Route::EventFrame;
            else if(checked)diagnostic_route=runtime_cause::Route::Checked;
            else if(cached)diagnostic_route=runtime_cause::Route::Cached;
            else if(residual)diagnostic_route=runtime_cause::Route::Residual;
            else if(due)diagnostic_route=runtime_cause::Route::Frame;
            if (due < 0) ms = 0;
            else if (due > 0) {
                t.wake_us = due;
                const int64_t left = due - paint_clock_us();
                ms = left > 0 ? (left + 999) / 1000 : 0;
                if (ms < 1 && left > 0) ms = 1;
            } else if (!speed_period_us() && ms >= 8 && ms <= 20) t.sleep_request = (int)ms;
        }
        if(!join)speed_compensated_sleep_retimed(p84_req_ms,ms);
        if(!join)p84_note_sleep(p84_req_ms,ms); // P8.4: post-restore sleep log (log only)
        t.wake=join&&ms==0?0:thread_deadline(ms);
        t.state=join?ThreadState::Joining:(ms?ThreadState::Sleeping:ThreadState::Runnable);
        t.join_target=join?arg(0).ref:0;
        diag_sleep_start(t,p84_req_ms,ms,r4::state.active?runtime_cause::Route::Virtual:diagnostic_route,diagnostic_owned);
        suspend_current(ms==0);return;
    }
    die("native Thread."+name);
}
static void object_monitor_native(Method* method,Frame& fr) {
    int ref=fr.locals[0].ref;auto& t=current_context();
    int64_t timeout=method->name=="wait"?as_long(fr.locals[1]):0;
    if(timeout<0){throw_new("java/lang/IllegalArgumentException","negative wait");return;}
    auto it=VMS_EXEC(g_monitors).find(ref);
    if(it==VMS_EXEC(g_monitors).end()||it->second.owner!=&t){throw_new("java/lang/IllegalMonitorStateException","monitor not owned");return;}
    auto& m=it->second;
    if(method->name!="wait") {
        while(!m.waiters.empty()) {
            auto* waiter=m.waiters.front();m.waiters.pop_front();
            if(waiter->state!=ThreadState::Waiting||waiter->monitor!=ref)continue;
            speed_frame_wait_cancel_request(*waiter);waiter->frame_prep.reset();
            waiter->state=ThreadState::Blocked;waiter->wake=0;waiter->wake_us=0;
            if(method->name=="notify")break;
        }
        return;
    }
    int self=current_thread_ref();
    t.event_frame_sleep.cancel();
    if(thread_interrupted(self)){t.frame_prep.reset();speed_frame_wait_cancel_request(t);set_interrupted(self,false);throw_new("java/lang/InterruptedException","interrupted");return;}
    t.wake_us=0;
    if(timeout)timeout=speed_timer_wait_ms(ref,timeout,t);
    else speed_timer_block(t);
    if(timeout&&!t.timer.queue_wait) {
        const int64_t due=speed_frame_wait_wake_us(ref,timeout,t);
        if(due>0) {
            t.wake_us=due;
            timeout=std::max<int64_t>(1,(due-paint_clock_us()+999)/1000);
        }
    }
    if(!t.wait_frame.paced){t.frame_prep.reset();if(!t.wait_frame.awaiting_paint)speed_frame_wait_cancel_request(t);}
    t.monitor=ref;t.restore_depth=m.depth;t.reacquire=true;
    t.state=ThreadState::Waiting;t.wake=timeout?thread_deadline(timeout):0;
    m.owner=nullptr;m.depth=0;m.waiters.push_back(&t);
    suspend_current();
}
