// Optional, exact-suite P1 diagnostics. No Java allocation, timeout or scheduler
// policy change. Strings/scalars are copied before native I/O or worker pumps.
static constexpr const char* CityDiagnosticSuite="563f753243eefeeef25f392f1cb13c22725ad3cdf019a232c6314064cf8897ab";
struct CityDiagnostics {
    bool requested=false,enabled=false,key_active=false;
    uint64_t trace=0,key_spans=0,key_dropped=0,rms_spans=0,rms_dropped=0;
    uint64_t wait_spans=0,wait_dropped=0,host_spans=0,host_dropped=0;
    uint64_t methods=0,method_dropped=0,sync_steps=0,sync_dropped=0,wait_steps=0,wait_step_dropped=0;
    uint64_t phase_spans=0,phase_dropped=0,snapshots=0,snapshot_records=0,flushes=0,flush_us=0,flush_max_us=0;
    unsigned host_this_trace=0;
    uint64_t yield_trace=0;
    int64_t key_started=0,recent_until=0,last_sync=0,last_wait=0;
    uint64_t base_ops=0;
} ; VMS_DIAG_ACCESSOR(g_city_diag, CityDiagnostics)
static uint64_t city_trace(){
    return VMS_DIAG(g_city_diag).enabled&&(VMS_DIAG(g_city_diag).key_active||paint_clock_us()<=VMS_DIAG(g_city_diag).recent_until)?VMS_DIAG(g_city_diag).trace:0;
}
static void city_line(const char* tag,uint64_t trace,const std::string& fields){
    const int saved_errno=errno;
    log_line(std::string(tag)+" schema=1 session="+std::to_string(VMS_DIAG(g_p1).session_id)+" launch="+std::to_string(VMS_DIAG(g_p1).launch_id)+
        " trace_id="+std::to_string(trace)+" mono_us="+std::to_string(paint_clock_us())+" "+fields);
    const int64_t start=paint_clock_us();
    log_drain();
    const uint64_t duration=uint64_t(std::max<int64_t>(0,paint_clock_us()-start));
    ++VMS_DIAG(g_city_diag).flushes;VMS_DIAG(g_city_diag).flush_us+=duration;VMS_DIAG(g_city_diag).flush_max_us=std::max(VMS_DIAG(g_city_diag).flush_max_us,duration);
    errno=saved_errno;
}
static std::string city_method(const Method* m){
    if(!m||m->class_id<0||m->class_id>=int(VMS_CL(g_classes).size()))return "unknown";
    return VMS_CL(g_classes)[m->class_id].name+"."+m->name+m->desc;
}
static std::string city_context(){
    return "worker="+std::to_string(current_context().ref)+" lifecycle="+std::to_string(int(VMS_EXEC(g_midlet_state)))+
        " live="+std::to_string(int(VMS_EXEC(g_thread_live)))+" run_depth="+std::to_string(VMS_EXEC(g_run_depth))+
        " pump_depth="+std::to_string(VMS_EXEC(g_pump_depth))+" paint_depth="+std::to_string(VMS_RS(g_paint_depth))+
        " can_suspend="+std::to_string(int(VMS_EXEC(g_can_suspend)));
}
// A missing class/object/descriptor is unknown, never resolved by loading Java.
static bool city_value(const char* cls,int ref,const char* name,const char* desc,Value& out){
    auto it=VMS_CL(g_class_index).find(cls);if(it==VMS_CL(g_class_index).end())return false;
    const int cid=it->second;if(cid<0||cid>=int(VMS_CL(g_classes).size()))return false;
    const bool stat=ref==0;
    if(!stat&&(ref<1||ref>=int(VMS_HEAP(g_heap).size())||VMS_HEAP(g_heap)[ref].cls!=cid))return false;
    for(const auto& f:VMS_CL(g_classes)[cid].fields)if(f.name==name&&f.desc==desc&&f.is_static==stat&&f.slot>=0){
        const auto& values=stat?VMS_CL(g_classes)[cid].statics:VMS_HEAP(g_heap)[ref].fields;
        if(size_t(f.slot)>=values.size())return false;out=values[f.slot];return true;
    }
    return false;
}
static std::string city_int(const char* cls,int ref,const char* name,const char* desc){
    Value v;return city_value(cls,ref,name,desc,v)?std::to_string(v.i):"unknown";
}
static std::string city_state(int canvas){
    Value i,ac,mode,slot,titles;int ir=-1,ar=-1;
    if(city_value("ca",canvas,"a","Li;",i))ir=i.ref;
    if(city_value("ca",canvas,"a","Lac;",ac))ar=ac.ref;
    std::string title="unknown";
    if(city_value("j",0,"a","B",mode)&&city_value("j",0,"d","B",slot)&&city_value("j",0,"b","[[Ljava/lang/String;",titles)){
        const int outer=titles.ref;
        if(outer>0&&outer<int(VMS_HEAP(g_heap).size())&&VMS_HEAP(g_heap)[outer].is_array&&mode.i>=0&&size_t(mode.i)<VMS_HEAP(g_heap)[outer].elems.size()){
            const int row=VMS_HEAP(g_heap)[outer].elems.get(mode.i).ref;
            if(row>0&&row<int(VMS_HEAP(g_heap).size())&&VMS_HEAP(g_heap)[row].is_array&&slot.i>=0&&size_t(slot.i)<VMS_HEAP(g_heap)[row].elems.size()){
                const int str=VMS_HEAP(g_heap)[row].elems.get(slot.i).ref;
                if(str>0&&str<int(VMS_HEAP(g_heap).size()))title=VMS_HEAP(g_heap)[str].text.empty()?"0":"1";
            }
        }
    }
    return "ca.a:B="+city_int("ca",0,"a","B")+" i.ref="+std::to_string(ir)+" i.a:B="+city_int("i",ir,"a","B")+
        " i.a:Z="+city_int("i",ir,"a","Z")+" ac.ref="+std::to_string(ar)+" ac.a:B="+city_int("ac",ar,"a","B")+
        " ac.b:B="+city_int("ac",ar,"b","B")+" j.k:Z="+city_int("j",0,"k","Z")+" j.a:Z="+city_int("j",0,"a","Z")+
        " j.l:Z="+city_int("j",0,"l","Z")+" j.a:B="+city_int("j",0,"a","B")+" j.d:B="+city_int("j",0,"d","B")+" selected_title_nonempty="+title;
}
static void city_diag_configure(){
    const int saved_errno=errno;
#ifdef __SWITCH__
    struct stat st{};const char* source="sdmc:/switch/j2me/p1-city-diag.enable";
    VMS_DIAG(g_city_diag).requested=stat(source,&st)==0&&S_ISREG(st.st_mode);
#else
    const char* source="J2ME_CITY_DIAG";const char* value=std::getenv(source);
    VMS_DIAG(g_city_diag).requested=value&&std::strcmp(value,"1")==0;
#endif
    city_line("CITYCONFIG",0,"build="+std::string(kVer)+" requested="+std::to_string(VMS_DIAG(g_city_diag).requested)+" source="+source+
        " target_suite="+CityDiagnosticSuite+" key_span_cap=64 rms_op_cap=128 rms_phase_cap_per_op=64 wait_span_cap=64 host_span_cap=512 host_per_trace=8 method_cap=128 sync_cap=16 wait_progress_cap=16 slow_after_us=250000 progress_period_us=1000000 caught_snapshot_cap=1 post_key_us=2000000 flush=ofstream fsync=0");
    errno=saved_errno;
}
static void city_diag_launch(const std::string& identity){
    const bool requested=VMS_DIAG(g_city_diag).requested;VMS_DIAG(g_city_diag)=CityDiagnostics{};
    VMS_DIAG(g_city_diag).requested=requested;VMS_DIAG(g_city_diag).enabled=requested&&identity==CityDiagnosticSuite;
    if(requested)city_line("CITYLAUNCH",0,"enabled="+std::to_string(VMS_DIAG(g_city_diag).enabled)+" suite="+identity+" budgets=reset");
}
struct CityKeySpan {
    bool active=false;uint64_t id=0,caught=0;int64_t began=0;int canvas=0;
    std::string target;
    CityKeySpan(int code,int effective,bool release,int ref,const Method* method,uint64_t ops):canvas(ref){
        if(!VMS_DIAG(g_city_diag).enabled)return;
        if(VMS_DIAG(g_city_diag).key_spans>=64){if(!VMS_DIAG(g_city_diag).key_dropped++)city_line("CITYCAP",0,"budget=key_spans exhausted=1 cap=64");return;}
        active=true;id=++VMS_DIAG(g_city_diag).trace;++VMS_DIAG(g_city_diag).key_spans;
        caught=VMS_DIAG(g_p1).caught_count;began=paint_clock_us();target=city_method(method);
        VMS_DIAG(g_city_diag).key_active=true;VMS_DIAG(g_city_diag).key_started=began;VMS_DIAG(g_city_diag).recent_until=began+2000000;
        VMS_DIAG(g_city_diag).last_sync=0;VMS_DIAG(g_city_diag).last_wait=0;VMS_DIAG(g_city_diag).base_ops=ops;VMS_DIAG(g_city_diag).host_this_trace=0;
        const std::string cn=ref>0&&ref<int(VMS_HEAP(g_heap).size())&&VMS_HEAP(g_heap)[ref].cls>=0?VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name:"unknown";
        city_line("KEYCALL",id,"phase=begin code="+std::to_string(code)+" effective="+std::to_string(effective)+" release="+std::to_string(release)+
            " canvas_ref="+std::to_string(ref)+" canvas_class="+cn+" method="+target+" "+city_context()+" "+city_state(ref));
        // Separate budget/counter from the existing two caught-dump contract.
        if(!release&&!VMS_DIAG(g_city_diag).snapshots&&VMS_DIAG(g_p1).caught_retained){
            ++VMS_DIAG(g_city_diag).snapshots;VMS_DIAG(g_city_diag).snapshot_records=VMS_DIAG(g_p1).caught_retained;
            city_line("CITYCAUGHTSET",id,"boundary=pre_key retained="+std::to_string(VMS_DIAG(g_p1).caught_retained)+" overwritten="+std::to_string(VMS_DIAG(g_p1).caught_overwritten)+" snapshot=1 budget=1 original_caught_dumps_unchanged="+std::to_string(VMS_DIAG(g_p1).caught_dumps));
            const size_t first=(VMS_DIAG(g_p1).caught_next+P1Diagnostics::CaughtRing-VMS_DIAG(g_p1).caught_retained)%P1Diagnostics::CaughtRing;
            for(size_t n=0;n<VMS_DIAG(g_p1).caught_retained;++n)city_line("CITYCAUGHT",id,VMS_DIAG(g_p1).caught_lines[(first+n)%P1Diagnostics::CaughtRing].data());
        }
    }
    void end(){
        if(!active)return;
        city_line("KEYCALL",id,"phase=end result=returned method="+target+" canvas_ref_at_begin="+std::to_string(canvas)+
            " canvas_ref_now="+std::to_string(VMS_RS(g_current))+" elapsed_us="+std::to_string(paint_clock_us()-began)+
            " caught_delta="+std::to_string(VMS_DIAG(g_p1).caught_count-caught)+" thrown_now="+std::to_string(VMS_EXEC(g_thrown))+" "+city_context());
        VMS_DIAG(g_city_diag).key_active=false;VMS_DIAG(g_city_diag).recent_until=paint_clock_us()+2000000;active=false;
    }
};
static void city_method_enter(const Method* method){
    const uint64_t id=city_trace();if(!id||!method||method->class_id<0)return;
    const std::string& owner=VMS_CL(g_classes)[method->class_id].name;
    const bool match=(owner=="ca"&&((method->name=="c"&&method->desc=="(I)V")||(method->name=="d"&&method->desc=="()V")))||
        ((owner=="i"||owner=="ac")&&method->name=="a"&&method->desc=="(I)V")||
        (owner=="ac"&&method->name=="<init>"&&method->desc=="()V")||
        (owner=="j"&&((method->name=="c"&&method->desc=="(I)V")||(method->name=="g"&&method->desc=="()V")));
    if(!match)return;
    if(VMS_DIAG(g_city_diag).methods>=128){if(!VMS_DIAG(g_city_diag).method_dropped++)city_line("CITYCAP",id,"budget=method_enter exhausted=1 cap=128");return;}
    ++VMS_DIAG(g_city_diag).methods;city_line("CITYMETHOD",id,"phase=enter method="+city_method(method)+" "+city_context()+" "+city_state(VMS_RS(g_current)));
}
static void city_sync_step(const Frame& f,size_t depth,uint64_t ops){
    const uint64_t id=city_trace();if(!id)return;
    const int64_t now=paint_clock_us();
    if(now-VMS_DIAG(g_city_diag).key_started<250000|| (VMS_DIAG(g_city_diag).last_sync&&now-VMS_DIAG(g_city_diag).last_sync<1000000))return;
    VMS_DIAG(g_city_diag).last_sync=now;
    if(VMS_DIAG(g_city_diag).sync_steps>=16){if(!VMS_DIAG(g_city_diag).sync_dropped++)city_line("CITYCAP",id,"budget=sync_step exhausted=1 cap=16");return;}
    ++VMS_DIAG(g_city_diag).sync_steps;
    city_line("SYNCSTEP",id,"method="+city_method(f.method)+" bci="+std::to_string(f.op_pc)+" next_bci="+std::to_string(f.pc)+
        " stack_depth="+std::to_string(depth)+" ops_since_key="+std::to_string(ops-VMS_DIAG(g_city_diag).base_ops)+
        " elapsed_us="+std::to_string(now-VMS_DIAG(g_city_diag).key_started)+" origin="+(VMS_DIAG(g_city_diag).key_active?"key_active":"recent_key")+" "+city_context());
}
static std::string city_wait_state(){
    const auto& t=current_context();int owner=-1;const char* owner_context="none";
    auto it=VMS_EXEC(g_monitors).find(t.monitor);if(it!=VMS_EXEC(g_monitors).end()&&it->second.owner){owner=it->second.owner->ref;owner_context=it->second.owner==&VMS_EXEC(g_main_thread)?"main":"worker";}
    std::string rows="unknown";unsigned n=0;
    for(const auto& w:VMS_EXEC(g_threads)){if(n++==8)break; if(rows=="unknown")rows.clear();else rows+=",";
        rows+=std::to_string(w.ref)+":"+std::to_string(int(w.state))+":"+std::to_string(w.monitor)+":"+std::to_string(w.join_target)+":"+std::to_string(w.wake);}
    const Frame* f=VMS_EXEC(g_active_stack)&&!VMS_EXEC(g_active_stack)->empty()?&VMS_EXEC(g_active_stack)->back():nullptr;
    std::string roots;unsigned shown=0;
    for(const auto& root:VMS_HEAP(g_root_stacks)){
        if(shown++==8)break;if(!roots.empty())roots+=",";
        const Frame* top=root.first&&!root.first->empty()?&root.first->back():nullptr;
        roots+=(root.second==&VMS_EXEC(g_main_thread)?"main":"worker")+std::string(":")+std::to_string(root.second?root.second->ref:-1)+":"+
            city_method(top?top->method:nullptr)+":"+std::to_string(top?top->op_pc:-1);
    }
    return "state="+std::to_string(int(t.state))+" monitor="+std::to_string(t.monitor)+" monitor_owner_worker="+std::to_string(owner)+
        " monitor_owner_context="+owner_context+" parked_root_stacks="+(roots.empty()?"unknown":roots)+
        " join_target="+std::to_string(t.join_target)+" wake_ms="+std::to_string(t.wake)+" init_wait="+std::to_string(t.init_wait)+
        " method="+city_method(f?f->method:nullptr)+" bci="+std::to_string(f?f->op_pc:-1)+
        " workers_scalar="+rows+" workers_total="+std::to_string(VMS_EXEC(g_threads).size())+" workers_shown_cap=8 "+city_context();
}
struct CityWaitSpan {
    bool active=false;uint64_t id=0;int64_t began=0;
    CityWaitSpan(){
        id=city_trace();if(!id)return;
        // Repeated ordinary worker yields must not exhaust the synchronous
        // wait budget before the failing key. One yield span per input trace.
        if(VMS_EXEC(g_can_suspend)&&VMS_DIAG(g_city_diag).yield_trace==id)return;
        if(VMS_EXEC(g_can_suspend))VMS_DIAG(g_city_diag).yield_trace=id;
        if(VMS_DIAG(g_city_diag).wait_spans>=64){if(!VMS_DIAG(g_city_diag).wait_dropped++)city_line("CITYCAP",id,"budget=wait_span exhausted=1 cap=64");return;}
        active=true;++VMS_DIAG(g_city_diag).wait_spans;began=paint_clock_us();
        city_line("WAIT",id,"phase=begin reason=suspend_current "+city_wait_state());
    }
    void progress(){
        if(!active)return;const int64_t now=paint_clock_us();
        if(now-began<250000||(VMS_DIAG(g_city_diag).last_wait&&now-VMS_DIAG(g_city_diag).last_wait<1000000))return;
        VMS_DIAG(g_city_diag).last_wait=now;
        if(VMS_DIAG(g_city_diag).wait_steps>=16){if(!VMS_DIAG(g_city_diag).wait_step_dropped++)city_line("CITYCAP",id,"budget=wait_progress exhausted=1 cap=16");return;}
        ++VMS_DIAG(g_city_diag).wait_steps;city_line("WAIT",id,"phase=progress elapsed_us="+std::to_string(now-began)+" "+city_wait_state());
    }
    void end(const char* result){if(active){city_line("WAIT",id,"phase=end result="+std::string(result)+" elapsed_us="+std::to_string(paint_clock_us()-began)+" "+city_wait_state());active=false;}}
};
struct CityHostSpan {
    uint64_t id=0;bool active=false;int64_t began=0;std::string phase;int target_worker=-1;
    CityHostSpan(const char* p,int target=-1):phase(p),target_worker(target){
        id=city_trace();if(!id)return;
        if(VMS_DIAG(g_city_diag).host_spans>=512||VMS_DIAG(g_city_diag).host_this_trace>=8){++VMS_DIAG(g_city_diag).host_dropped;return;}
        ++VMS_DIAG(g_city_diag).host_spans;++VMS_DIAG(g_city_diag).host_this_trace;active=true;began=paint_clock_us();
        city_line("HOSTPHASE",id,"phase=begin scope="+phase+" origin="+(VMS_DIAG(g_city_diag).key_active?"key_active":"recent_key")+
            " target_worker="+std::to_string(target_worker)+" draw_id="+std::to_string(VMS_DIAG(g_p1).draw_id)+" consumed_draw_id="+std::to_string(VMS_DIAG(g_p1).present_draw_id)+" "+city_context());
    }
    ~CityHostSpan(){if(active)city_line("HOSTPHASE",id,"phase=end scope="+phase+" elapsed_us="+std::to_string(paint_clock_us()-began)+
        " target_worker="+std::to_string(target_worker)+" draw_id="+std::to_string(VMS_DIAG(g_p1).draw_id)+" consumed_draw_id="+std::to_string(VMS_DIAG(g_p1).present_draw_id)+" "+city_context());}
};
struct CityRmsSpan {
    bool active=false;uint64_t id=0,operation_id=0;unsigned phases=0;int64_t began=0;std::string operation,encoded,caller;int bci=-1;
    CityRmsSpan(const std::string& op,int name_ref):operation(op){
        if(!VMS_DIAG(g_city_diag).enabled)return;
        if(VMS_DIAG(g_city_diag).rms_spans>=128){if(!VMS_DIAG(g_city_diag).rms_dropped++)city_line("CITYCAP",city_trace(),"budget=rms_op exhausted=1 cap=128");return;}
        ++VMS_DIAG(g_city_diag).rms_spans;operation_id=VMS_DIAG(g_city_diag).rms_spans;active=true;id=city_trace();began=paint_clock_us();
        static const char* hex="0123456789abcdef";
        if(name_ref>0&&name_ref<int(VMS_HEAP(g_heap).size())){
            const auto& text=VMS_HEAP(g_heap)[name_ref].text;
            for(size_t n=0;n<std::min<size_t>(text.size(),128);++n){unsigned char c=text[n];encoded.push_back(hex[c>>4]);encoded.push_back(hex[c&15]);}
            if(text.size()>128)encoded+="_truncated";
        }else encoded="unknown";
        const Frame* f=VMS_EXEC(g_active_stack)&&!VMS_EXEC(g_active_stack)->empty()?&VMS_EXEC(g_active_stack)->back():nullptr;
        caller=city_method(f?f->method:nullptr);bci=f?f->op_pc:-1;
        city_line("RMSIO",id,"phase=begin scope=operation operation="+operation+" operation_id="+std::to_string(operation_id)+
            " encoded_name="+encoded+" suite="+VMS_PLAT(g_rms_suite)+" caller="+caller+" bci="+std::to_string(bci)+" "+city_context());
    }
    bool reserve(){
        if(!active)return false;
        if(phases>=64){if(phases==64){++phases;++VMS_DIAG(g_city_diag).phase_dropped;city_line("CITYCAP",id,"budget=rms_phases exhausted=1 cap_per_op=64 operation_id="+std::to_string(operation_id));}return false;}
        ++phases;++VMS_DIAG(g_city_diag).phase_spans;return true;
    }
    ~CityRmsSpan(){if(active)city_line("RMSIO",id,"phase=end scope=operation operation="+operation+" operation_id="+std::to_string(operation_id)+
        " encoded_name="+encoded+" result="+(VMS_EXEC(g_thrown)?"java_exception":"returned")+" elapsed_us="+std::to_string(paint_clock_us()-began));}
};
struct CityRmsPhase {
    CityRmsSpan& span;bool active;int64_t began=0;std::string phase;int64_t expected=-1;unsigned phase_id=0;
    CityRmsPhase(CityRmsSpan& s,const char* p,int64_t bytes=-1):span(s),active(s.reserve()),phase(p),expected(bytes){
        if(active){phase_id=s.phases;began=paint_clock_us();city_line("RMSIO",s.id,"phase=begin scope="+phase+" operation="+s.operation+
            " operation_id="+std::to_string(s.operation_id)+" phase_id="+std::to_string(phase_id)+" encoded_name="+s.encoded+
            " expected_bytes="+std::to_string(expected)+" actual_bytes=unknown caller="+s.caller+" bci="+std::to_string(s.bci));}
    }
    void end(int64_t result,int error,int64_t actual=-1){
        if(active){city_line("RMSIO",span.id,"phase=end scope="+phase+" operation="+span.operation+" operation_id="+std::to_string(span.operation_id)+
            " phase_id="+std::to_string(phase_id)+" result="+std::to_string(result)+" errno="+std::to_string(error)+
            " expected_bytes="+std::to_string(expected)+" actual_bytes="+std::to_string(actual)+" elapsed_us="+std::to_string(paint_clock_us()-began));active=false;}
    }
    ~CityRmsPhase(){if(active)end(-1,errno);}
};
// The stream's original scope-exit close is observed at the same exit point.
// No close failure changes the Java result (the prior destructor ignored it).
struct CityRmsCloseProbe {
    CityRmsSpan& span;std::ifstream& stream;
    CityRmsCloseProbe(CityRmsSpan& s,std::ifstream& in):span(s),stream(in){}
    ~CityRmsCloseProbe(){
        if(!span.active||!stream.is_open())return;
        CityRmsPhase phase(span,"close_stream");const int saved_errno=errno;
        const auto prior=stream.rdstate();
        stream.close();const int error=errno;
        // A previous short read/EOF owns failbit. Do not claim close failed
        // merely because that flag remains set; result=2 means unknown.
        phase.end(prior==std::ios::goodbit?(stream.fail()?-1:0):2,prior==std::ios::goodbit?error:0);
        errno=saved_errno;
    }
};
static void city_diag_report(){
    if(!VMS_DIAG(g_city_diag).enabled)return;
    city_line("CITYSTAT",city_trace(),"scope=launch_cumulative key_spans="+std::to_string(VMS_DIAG(g_city_diag).key_spans)+" key_dropped="+std::to_string(VMS_DIAG(g_city_diag).key_dropped)+
        " rms_ops="+std::to_string(VMS_DIAG(g_city_diag).rms_spans)+" rms_dropped="+std::to_string(VMS_DIAG(g_city_diag).rms_dropped)+
        " rms_phases="+std::to_string(VMS_DIAG(g_city_diag).phase_spans)+" phase_caps_hit="+std::to_string(VMS_DIAG(g_city_diag).phase_dropped)+
        " wait_spans="+std::to_string(VMS_DIAG(g_city_diag).wait_spans)+" wait_dropped="+std::to_string(VMS_DIAG(g_city_diag).wait_dropped)+
        " host_spans="+std::to_string(VMS_DIAG(g_city_diag).host_spans)+" host_dropped="+std::to_string(VMS_DIAG(g_city_diag).host_dropped)+
        " method_records="+std::to_string(VMS_DIAG(g_city_diag).methods)+" method_dropped="+std::to_string(VMS_DIAG(g_city_diag).method_dropped)+
        " sync_steps="+std::to_string(VMS_DIAG(g_city_diag).sync_steps)+" sync_dropped="+std::to_string(VMS_DIAG(g_city_diag).sync_dropped)+
        " wait_progress="+std::to_string(VMS_DIAG(g_city_diag).wait_steps)+" wait_progress_dropped="+std::to_string(VMS_DIAG(g_city_diag).wait_step_dropped)+
        " caught_snapshots="+std::to_string(VMS_DIAG(g_city_diag).snapshots)+" caught_snapshot_records="+std::to_string(VMS_DIAG(g_city_diag).snapshot_records)+
        " flushes="+std::to_string(VMS_DIAG(g_city_diag).flushes)+" explicit_flush_us="+std::to_string(VMS_DIAG(g_city_diag).flush_us)+" explicit_flush_max_us="+std::to_string(VMS_DIAG(g_city_diag).flush_max_us)+
        " persistence=ofstream_flush_not_fsync sd_hard_block=unobservable_no_watchdog");
}
