// gc_roots.h — P2.1-c3: per-module GC root enumeration (C04). Included right before gc_collect,
// (P2.1 final: always). Every module states its roots in one place; gc_collect calls them in turn.
// A visitor V provides: void ref(int) and void val(const Value&). Storage of non-Heap modules is NOT moved here;
// these functions only read the module's existing globals (Exec/Render/Platform storage moves in later batches).
#pragma once
namespace j2me_gc {
// Heap/RootScope: frames of Java/native activations currently registered by RunScope.
template <class V> static inline void frame_roots(V& v, const std::vector<Frame>* st) {
    if (!st) return;
    for (auto& fr : *st) { v.ref(fr.sync_monitor); v.val(fr.constructed); for (auto& x : fr.locals) v.val(x); for (auto& x : fr.stack) v.val(x); }
}
template <class V> static inline void heap_roots(V& v) {
    for (auto& active : VMS_HEAP(g_root_stacks)) frame_roots(v, active.first);
    // P2.2-S5 RootScope: native temporaries held across a Java re-entry (empty at every production GC).
    for (auto& e : g_native_roots.entries) { if (e.ref) v.ref(*e.ref); if (e.vals) for (auto& x : *e.vals) v.val(x); }
}
// Diagnostics: none. g_exception_traces is keyed by ref but WEAK (entry erased when the object is freed).
template <class V> static inline void diag_roots(V&) {}
// NativeRegistry: none. g_library_indexes is a WEAK derived index, swept after marking.
template <class V> static inline void natives_roots(V&) {}
// ClassLoader/Resolver: Class objects, class statics, recorded class-init errors.
template <class V> static inline void classes_roots(V& v) {
    for (const auto& kv : VMS_CL(g_class_objects)) v.ref(kv.second);
    for (auto& c : VMS_CL(g_classes)) { v.ref(c.init_error); for (auto& s : c.statics) v.val(s); }
}
// ExecutionContext/Scheduler: MIDlet, serial callbacks, main thread, contended monitors, live threads,
// plus the repaint owner/yield identities (P2.1-a suspects R2, verified missing in P2.1-c3 and added here).
template <class V> static inline void exec_roots(V& v) {
    for (int r : VMS_EXEC(g_serial_calls)) v.ref(r);
    v.ref(VMS_EXEC(g_midlet));
    v.ref(VMS_EXEC(g_main_thread).ref); v.ref(VMS_EXEC(g_main_thread).unhandled); v.ref(VMS_EXEC(g_main_thread).monitor);
    for (const auto& kv : VMS_EXEC(g_monitors)) if (kv.second.owner || !kv.second.waiters.empty()) v.ref(kv.first);
    for (auto& t : VMS_EXEC(g_threads)) if (t.state != ThreadState::Terminated || t.executing) {
        v.ref(t.ref); v.ref(t.monitor); v.ref(t.join_target); v.ref(t.unhandled); frame_roots(v, &t.frames);
    }
    v.ref(VMS_EXEC(g_repaint_owner_ref));      // NEW (c3): Thread object identity held across GC until frame end
    v.ref(VMS_EXEC(g_repaint_yield_canvas));   // NEW (c3): Canvas identity compared against VMS_RS(g_current) at Thread.yield
}
// PlatformHost: none. VMS_PLAT(g_tones) holds audio-engine player handles, not Java heap references.
template <class V> static inline void platform_roots(V&) {}
// RenderSurface: displayed/current/alert-return objects. (Graphics->Image and GameCanvas->buffer associations are
// owned edges traced from their Java object in gc_collect, not independent roots.)
template <class V> static inline void render_roots(V& v) {
    v.ref(VMS_RS(g_shown)); v.ref(VMS_RS(g_alert_return)); v.ref(VMS_RS(g_current)); v.ref(VMS_RS(g_display));
}
template <class V> static inline void all_roots(V& v) {
    diag_roots(v); natives_roots(v); classes_roots(v); heap_roots(v); exec_roots(v); platform_roots(v); render_roots(v);
}
}  // namespace j2me_gc
