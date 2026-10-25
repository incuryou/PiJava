// P2.3 (roadmap P2.3, PX-2): presentable safe boundary by completed draw/surface version. OBSERVATION ONLY.
// Moving the present boundary or deferring/merging paints would change display timing and draw counts, which the
// P2 rule forbids; so this file only measures, at every present (C08 trace on: p2-s0-trace.enable / J2ME_S0_TRACE=1):
//  - boundary class: new completed surface at a safe point / duplicate (no new surface) / unsafe (inside paint or run),
//  - consumed id (surface version presented) and completion->present latency, superseded (never-presented) versions,
//  - what was kept pending across the present: serial queue length + oldest age, runnable workers, repaint request,
//  - host service opportunities: gap between host input/lifecycle services and between audio pumps (upper bounds).
// Nothing here is in J2S5 state, nothing reorders or drops serial/worker/repaint work.
#if J2ME_S0_COMPILED
struct P23Obs {
    uint64_t presents=0, safe_new=0, dup=0, unsafe=0, last_consumed=0, superseded_versions=0;
    int64_t surf_done_us=0; uint64_t lat_n=0, lat_sum=0; int64_t lat_max=0;
    uint64_t with_serial=0, with_worker=0, with_repaint=0, serial_max=0, worker_max=0; int64_t serial_age_max_us=0;
    int64_t last_service_us[2]{0,0}, gap_max_us[2]{0,0}; uint64_t gap_hist[2][6]{}, services[2]{};
};
static P23Obs p23;
static void p23_surface(){ p23.surf_done_us=paint_clock_us(); }
static int p23_bucket(int64_t us){ const int64_t ms=us/1000; return ms<=16?0:ms<=33?1:ms<=50?2:ms<=100?3:ms<=250?4:5; }
static void p23_service(int kind){ // 0 = host input/lifecycle service (host_events), 1 = audio pump
    const int64_t now=paint_clock_us(); ++p23.services[kind];
    if(p23.last_service_us[kind]){ const int64_t g=now-p23.last_service_us[kind]; p23.gap_max_us[kind]=std::max(p23.gap_max_us[kind],g); ++p23.gap_hist[kind][p23_bucket(g)]; }
    p23.last_service_us[kind]=now;
}
static void p23_reset_service_clock(){ p23.last_service_us[0]=p23.last_service_us[1]=0; } // host pause/sheet: not a service gap of the game
static void p23_present(){ // called right after s0_present (s0.present_id/presented_ver already updated)
    ++p23.presents;
    const bool unsafe=VMS_RS(g_paint_depth)!=0||VMS_EXEC(g_run_depth)!=0;
    const uint64_t ver=s0.presented_ver;
    if(unsafe)++p23.unsafe;
    else if(ver==p23.last_consumed)++p23.dup;
    else { ++p23.safe_new; if(ver>p23.last_consumed+1&&p23.last_consumed)p23.superseded_versions+=ver-p23.last_consumed-1;
        const int64_t lat=paint_clock_us()-p23.surf_done_us; if(lat>=0){++p23.lat_n;p23.lat_sum+=uint64_t(lat);p23.lat_max=std::max(p23.lat_max,lat);} }
    p23.last_consumed=ver;
    const size_t serial=VMS_EXEC(g_serial_calls).size(); size_t workers=0;
    for(auto& t:VMS_EXEC(g_threads)) if(t.state==ThreadState::Runnable)++workers;
    if(serial){++p23.with_serial;p23.serial_max=std::max<uint64_t>(p23.serial_max,serial);
        if(!g_cont.pending_seq.empty())p23.serial_age_max_us=std::max(p23.serial_age_max_us,paint_clock_us()-g_cont.pending_seq.front().second);}
    if(workers){++p23.with_worker;p23.worker_max=std::max<uint64_t>(p23.worker_max,workers);}
    if(VMS_RS(g_repaint))++p23.with_repaint;
}
static void p23_report(){
    if(!s0.on)return;
    auto h=[](int k){std::string s;for(int i=0;i<6;++i)s+=(i?"/":"")+std::to_string(p23.gap_hist[k][i]);return s;};
    log_line("P23OBS present presents="+std::to_string(p23.presents)+" safe_new="+std::to_string(p23.safe_new)+" dup="+std::to_string(p23.dup)+
        " unsafe="+std::to_string(p23.unsafe)+" consumed_last="+std::to_string(p23.last_consumed)+" superseded_versions="+std::to_string(p23.superseded_versions)+
        " complete_to_present_us avg="+std::to_string(p23.lat_n?p23.lat_sum/p23.lat_n:0)+" max="+std::to_string(p23.lat_max)+" mode=observe_only");
    log_line("P23OBS kept_pending_at_present serial="+std::to_string(p23.with_serial)+"(max_len="+std::to_string(p23.serial_max)+" oldest_age_max_us="+std::to_string(p23.serial_age_max_us)+
        ") runnable_worker="+std::to_string(p23.with_worker)+"(max="+std::to_string(p23.worker_max)+") repaint_requested="+std::to_string(p23.with_repaint)+" dropped=0(no drop path exists)");
    log_line("P23OBS host_service input_lifecycle n="+std::to_string(p23.services[0])+" gap_max_us="+std::to_string(p23.gap_max_us[0])+" hist_ms<=16/33/50/100/250/>250="+h(0)+
        " audio_pump n="+std::to_string(p23.services[1])+" gap_max_us="+std::to_string(p23.gap_max_us[1])+" hist="+h(1)+" mode=observe_only");
}
#define P23_HOOK(x) S0_HOOK(x)
#else
#define P23_HOOK(x) do{}while(0)
static inline void p23_report(){}
#endif
