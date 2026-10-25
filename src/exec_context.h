// P2.1-c4: ExecutionContext = the "current Java thread" state, gathered in ONE object (VMStorage::exec.ctx).
// The per-thread slice (running thread + yield + thrown) is saved/entered/restored as ONE value on every
// cooperative thread switch (see pump in threading.h). The remaining members are owned here but keep their
// old (shared, not restored) semantics exactly.
#pragma once
struct ExecutionContext {
    JavaThreadContext* g_running_thread = nullptr;
    std::vector<Frame>* g_active_stack = nullptr;
    bool g_yield = false;
    bool g_thrown = false;
    int g_run_depth = 0;
    int g_pump_depth = 0;
    int g_repaint_owner_ref = 0;
    // At most one coalesced repaint has an async timed-wait certificate.
    // A Java ref, rather than a host pointer, survives scheduler reentry.
    int frame_wait_request_owner = 0;
    int g_repaint_owner_yields = 0;
    int g_repaint_yield_canvas = 0;
    bool g_repaint_yield_handoff = false;
    Method* g_repaint_yield_method = nullptr;
    int g_repaint_yield_pc = -1;
    // Per-member decision (P2.1-c4 segment 2), see SELFCHECK:
    //   per Java thread  -> switched as one slice: g_running_thread, g_yield, g_thrown, g_active_stack
    //   VM-global on purpose (C-stack nesting / frame-scoped handshake keyed by owner thread):
    //     g_run_depth, g_pump_depth, g_repaint_owner_ref, g_repaint_owner_yields, g_repaint_yield_canvas,
    //     g_repaint_yield_handoff, g_repaint_yield_method, g_repaint_yield_pc
    struct ThreadSlice { JavaThreadContext* running_thread; bool yield, thrown; std::vector<Frame>* active_stack; };
    ThreadSlice save_slice() const { return {g_running_thread, g_yield, g_thrown, g_active_stack}; }
    // active_stack is NOT reset on enter: run() installs the new thread's stack through RunScope before any
    // interpreter use, and RunScope's destructor puts the previous value back, so restore_slice() writes back
    // the value that is already there (no behaviour change, but the slice is complete and checked).
    void enter_thread(JavaThreadContext* t) { g_running_thread = t; g_yield = g_thrown = false; }
    void restore_slice(const ThreadSlice& s) { g_running_thread = s.running_thread; g_yield = s.yield; g_thrown = s.thrown; g_active_stack = s.active_stack; }
};
VMS_EXEC_ACCESSOR(g_exec_ctx, ExecutionContext)
