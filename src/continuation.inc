// P2.2-S1: Continuation structure (PX-2 doc 02 section 3) and SerialRun continuations for
// Display.callSerially tasks (doc 02 section 4 step 1).
// S1 scope (P2 rule: never change draw count or display behaviour): a SerialRun continuation runs
// to completion exactly like the old direct run(m,{ref}); it never yields mid-callback. Order is the
// unchanged g_serial_calls deque (which J2S5 and GC roots keep using); the continuation adds identity
// (taskId, suiteEpoch), phase and order/age bookkeeping on top. Suspension inside a serial task is a
// later step and needs its own ruling.
// P2.2-S2S4-fix: the old direct-run reference path (serial_cont_reference.enable /
// J2ME_SERIAL_CONT_REFERENCE) was removed per ruling S1-3; the continuation path is the only path.
enum class ContPhase : uint8_t { Ready, Running, YieldedSlice, BlockedMonitor, BlockedClassInit, WaitingTarget, Completed, Faulted };
enum class ContKind  : uint8_t { Worker, SerialRun, Paint, ServiceRepaintsCaller, Lifecycle, NativeReentry };
struct Continuation {
    uint64_t suiteEpoch=0, taskId=0, enqueueSeq=0;
    ContKind kind=ContKind::SerialRun; ContPhase phase=ContPhase::Ready;
    std::vector<Frame> frames;            // unused while SerialRun runs to completion (S1)
    Value result{}; int pendingException=0;
    int monitorRef=0, monitorDepth=0; bool reacquire=false;
    int initClass=-1;
    int targetCanvas=0; uint64_t waitForTaskId=0, callerTaskId=0;
    int ownerThreadRef=0, graphicsRef=0, runnableRef=0;
    int64_t enqueuedUs=-1, startedUs=0;
};
// GPU fence state for the snapshot safepoint: structure only (P2 ruling 4). Not wired; P5 fills it.
struct GpuFenceState { bool tracked=false; uint64_t submitId=0, waitedId=0; bool pending() const { return tracked && waitedId<submitId; } };
struct ContinuationRuntime {
    uint64_t epoch=1, next_task=1, next_enqueue=1, last_dispatched_seq=0;
    std::deque<std::pair<uint64_t,int64_t>> pending_seq; // mirrors g_serial_calls (seq, enqueue us); reset on mismatch
    std::vector<Continuation*> running;                  // nested only if a serial run re-enters host dispatch
    uint64_t created=0, completed=0, faulted=0, order_violations=0, untracked=0, epoch_resets=0;
    int64_t max_age_us=0;
    GpuFenceState gpu;
};
static ContinuationRuntime g_cont;
static void cont_configure(){
    log_line("CONT config serial_continuation=1 reference=0 mode=run_to_completion");
}
// suite switch / lifecycle reset / restore: all continuations of the old epoch are invalid.
static void cont_new_epoch(){ ++g_cont.epoch; ++g_cont.epoch_resets; g_cont.pending_seq.clear(); g_cont.last_dispatched_seq=0; g_cont.running.clear(); } // also drops pointers left by a longjmp (die)
static void cont_serial_enqueued(size_t queue_size_after){
    if(g_cont.pending_seq.size()+1!=queue_size_after){ g_cont.pending_seq.clear(); ++g_cont.untracked; return; }
    g_cont.pending_seq.push_back({g_cont.next_enqueue++,paint_clock_us()});
}
static bool cont_running_any(){ return !g_cont.running.empty(); }
// Runs one popped callSerially Runnable. Identical Java call to the old path.
static void cont_run_serial(int ref, size_t queue_size_before_pop){
    Method* m=find_method(VMS_HEAP(g_heap)[ref].cls,"run","()V",true);
    Continuation c; c.kind=ContKind::SerialRun; c.suiteEpoch=g_cont.epoch; c.taskId=g_cont.next_task++; c.runnableRef=ref;
    c.ownerThreadRef=0; c.startedUs=paint_clock_us();
    if(g_cont.pending_seq.size()==queue_size_before_pop && !g_cont.pending_seq.empty()){
        c.enqueueSeq=g_cont.pending_seq.front().first; c.enqueuedUs=g_cont.pending_seq.front().second; g_cont.pending_seq.pop_front();
        if(c.enqueueSeq<=g_cont.last_dispatched_seq)++g_cont.order_violations;
        g_cont.last_dispatched_seq=c.enqueueSeq; g_cont.max_age_us=std::max(g_cont.max_age_us,c.startedUs-c.enqueuedUs);
    } else { g_cont.pending_seq.clear(); ++g_cont.untracked; }
    ++g_cont.created;
    if(!m){ c.phase=ContPhase::Completed; ++g_cont.completed; return; }
    c.phase=ContPhase::Running; g_cont.running.push_back(&c);
    const int unhandled_before=current_context().unhandled;
    c.result=run_event_callback(m,{Value::of_ref(ref)});
    g_cont.running.pop_back();
    if(current_context().unhandled && current_context().unhandled!=unhandled_before){ c.pendingException=current_context().unhandled; c.phase=ContPhase::Faulted; ++g_cont.faulted; }
    else { c.phase=ContPhase::Completed; ++g_cont.completed; }
}
static void cont_report(){
    log_line("CONT summary serial_created="+std::to_string(g_cont.created)+" completed="+std::to_string(g_cont.completed)+
        " faulted="+std::to_string(g_cont.faulted)+" order_violations="+std::to_string(g_cont.order_violations)+
        " untracked="+std::to_string(g_cont.untracked)+" epoch="+std::to_string(g_cont.epoch)+" epoch_resets="+std::to_string(g_cont.epoch_resets)+
        " max_queue_age_us="+std::to_string(g_cont.max_age_us)+" running_now="+std::to_string(g_cont.running.size())+
        " reference=0");
}
static bool s0_audio_unfrozen(bool& pending_ends){ pending_ends=p7_pending_ends(); return !p7_audio_paused(); } // P7.3 exact fast path
