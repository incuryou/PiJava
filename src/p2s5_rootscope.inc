// P2.2-S5 (PX-2 doc 04 row S5, roadmap P2.4): RootScope for native temporary heap references that are
// held in C++ while the native re-enters Java (run()) or calls code that can run Java (ensure_class -> <clinit>).
// Behaviour: production GC still only runs at the host safepoint (g_run_depth==0, see gc_safepoint), where every
// RootScope is already closed, so these roots are empty whenever a production GC runs. RootScope therefore changes
// no GC decision, no draw and no timing; it closes the coverage gap so a GC at depth>0 (tests, later steps) is safe.
// Re-entry conversion to native step state machines (doc 02 section 4 step 5) would change scheduling: per the P2
// rule it is NOT done; re-entry sites are counted only (P22OBS S5).
enum class ReentrySite : uint8_t { RunArgs, StreamFrom, NewInstance, ShowHide, ReleaseKeys, Paint, LaunchInit, DispatchKey,
    Serial, Lifecycle, HostEvent, ThreadEntry, ThrowWrap, Count };
static const char* reentry_site_name(ReentrySite s){
    static const char* n[]={"run_args","stream_from","newInstance","show_hide_notify","release_keys","paint","launch_init",
        "dispatch_key","serial","lifecycle","host_event","thread_entry","throw_wrap"};
    return n[size_t(s)<size_t(ReentrySite::Count)?size_t(s):0];
}
struct NativeRootRegistry {
    struct Entry { const int* ref; const std::vector<Value>* vals; int cls; bool is_array; int orig; uint8_t site=0; };
    std::vector<Entry> entries;
    uint64_t scopes=0, max_entries=0, residue_cleared=0;
    uint64_t opened_at_depth[size_t(ReentrySite::Count)]{}, reentries[size_t(ReentrySite::Count)]{};
    uint64_t max_reentry_depth=0, stale_at_close=0; // stale_at_close: test verification only
    bool test_disable_registration=false; // test-only negative control (never set by production code)
};
static NativeRootRegistry g_native_roots;
static void native_roots_snapshot_sig(NativeRootRegistry::Entry& e);
struct RootScope {
    size_t mark;
    RootScope(ReentrySite site, std::initializer_list<const int*> refs, const std::vector<Value>* vals=nullptr): mark(g_native_roots.entries.size()) {
        ++g_native_roots.scopes; ++g_native_roots.opened_at_depth[size_t(site)];
        if (VMS_EXEC(g_run_depth) > 0) { ++g_native_roots.reentries[size_t(site)];
            g_native_roots.max_reentry_depth = std::max<uint64_t>(g_native_roots.max_reentry_depth, uint64_t(VMS_EXEC(g_run_depth))); }
        if (g_native_roots.test_disable_registration) return;
        for (const int* r : refs) { NativeRootRegistry::Entry e{r,nullptr,-1,false,*r,uint8_t(site)}; native_roots_snapshot_sig(e); g_native_roots.entries.push_back(e); }
        if (vals) g_native_roots.entries.push_back({nullptr,vals,-1,false,0,uint8_t(site)});
        g_native_roots.max_entries = std::max<uint64_t>(g_native_roots.max_entries, g_native_roots.entries.size());
    }
    ~RootScope() {
#ifdef J2ME_TEST_ROOTSCOPE_VERIFY
        for (size_t i = mark; i < g_native_roots.entries.size(); ++i) { const auto& e = g_native_roots.entries[i];
            if (e.ref && e.orig > 0 && *e.ref == e.orig && (size_t(e.orig) >= VMS_HEAP(g_heap).size() ||
                VMS_HEAP(g_heap)[e.orig].cls != e.cls || VMS_HEAP(g_heap)[e.orig].is_array != e.is_array)) ++g_native_roots.stale_at_close; }
#endif
        if (g_native_roots.entries.size() >= mark) g_native_roots.entries.resize(mark);
    }
    RootScope(const RootScope&) = delete; RootScope& operator=(const RootScope&) = delete;
};
// Called where g_root_stacks is reset (lifecycle reset / restore): scopes abandoned by a longjmp (die) are dropped.
static std::string native_roots_open_sites() { std::string s; for (auto& e : g_native_roots.entries) { if (!s.empty()) s += ","; s += reentry_site_name(ReentrySite(e.site)); } return s.empty() ? "-" : s; }
static void native_roots_reset() { if (!g_native_roots.entries.empty()) { ++g_native_roots.residue_cleared; g_native_roots.entries.clear(); } }
static void native_roots_report() {
    std::string s = "P22OBS S5 rootscopes=" + std::to_string(g_native_roots.scopes) + " max_entries=" + std::to_string(g_native_roots.max_entries) +
        " residue_cleared=" + std::to_string(g_native_roots.residue_cleared) + " max_reentry_depth=" + std::to_string(g_native_roots.max_reentry_depth) + " reentry";
    for (size_t i = 0; i < size_t(ReentrySite::Count); ++i)
        if (g_native_roots.opened_at_depth[i]) s += std::string(" ") + reentry_site_name(ReentrySite(i)) + "=" + std::to_string(g_native_roots.reentries[i]) + "/" + std::to_string(g_native_roots.opened_at_depth[i]);
    log_line(s + " (nested/total) mode=roots_registered_reentry_observe_only");
}
static void native_roots_snapshot_sig(NativeRootRegistry::Entry& e) {
    const int r = *e.ref;
    if (r > 0 && size_t(r) < VMS_HEAP(g_heap).size()) { e.cls = VMS_HEAP(g_heap)[r].cls; e.is_array = VMS_HEAP(g_heap)[r].is_array; }
}

// ---- P2.2-S7: snapshot freezability check (used by save_instant_state). OBSERVE ONLY: never refuses. ----
struct SnapshotCheckStats { uint64_t checks=0, freezable=0, would_refuse=0, by_reason[10]{}; uint64_t gpu_untracked=0; };
static SnapshotCheckStats g_snap_check;
static const char* const kSnapReason[10]={"run_depth","pump_depth","paint_depth","thread_executing","root_owner","rootscope_open","continuation_running","audio_running","audio_pending_ends","serial_mid_run"};
static bool snapshot_freezable(std::string& reasons) {
    bool r[10]{};
    r[0]=VMS_EXEC(g_run_depth)!=0; r[1]=VMS_EXEC(g_pump_depth)!=0; r[2]=VMS_RS(g_paint_depth)!=0;
    for (auto& t : VMS_EXEC(g_threads)) if (t.executing) { r[3]=true; break; }
    for (auto& p : VMS_HEAP(g_root_stacks)) if (!p.second) { r[4]=true; break; }
    r[5]=!g_native_roots.entries.empty(); r[6]=cont_running_any();
    bool ends=false; r[7]=s0_audio_unfrozen(ends); r[8]=ends; r[9]=VMS_EXEC(g_run_depth)!=0 && cont_running_any();
    bool ok=true; reasons.clear();
    for (int i=0;i<10;++i) if (r[i]) { ok=false; ++g_snap_check.by_reason[i]; reasons += (reasons.empty()?"":",") + std::string(kSnapReason[i]); }
    ++g_snap_check.gpu_untracked; // GPU fences: GpuFenceState structure only, wired in P5
    return ok;
}
static void snapshot_check_report() {
    std::string s="P22OBS S7 snapshot_checks="+std::to_string(g_snap_check.checks)+" freezable="+std::to_string(g_snap_check.freezable)+
        " would_refuse="+std::to_string(g_snap_check.would_refuse)+" reasons";
    for (int i=0;i<10;++i) s+=std::string(" ")+kSnapReason[i]+"="+std::to_string(g_snap_check.by_reason[i]);
    log_line(s+" gpu_fence=untracked mode=observe_only_save_not_refused");
}
