static bool residual_field(const Frame& fr,int cp,int64_t& value) {
    const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
    if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[cp].tag!=9||fr.locals.empty())return false;
    const int ref=fr.locals[0].ref;
    if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return false;
    std::string c,n,d;if(!pacing_resolve_member(cls,cp,c,n,d))return false;
    const auto declared=VMS_CL(g_class_index).find(c);if(declared==VMS_CL(g_class_index).end()||!type_compatible(ref,c))return false;
    const auto& obj=VMS_HEAP(g_heap)[ref];const auto* field=find_field(declared->second,n,d);
    if(!field||field->is_static||field->slot<0||size_t(field->slot)>=obj.fields.size())return false;
    const auto v=obj.fields[field->slot];value=d=="J"?as_long(v):v.i;return true;
}
static bool speed_residual_clock(const Frame& fr,int64_t now) {
    auto& t=current_context();auto& s=t.residual_sleep;
    if(fr.locals.empty()||fr.locals[0].ref!=VMS_RS(g_current)||VMS_RS(g_current)!=VMS_RS(g_shown))return false;
    if(s.method!=fr.method||!s.pattern.valid()||
       (fr.op_pc!=s.pattern.start_pc&&fr.op_pc!=s.pattern.end_pc)) {
        const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
        auto member=[&](int cp,const char* c,const char* n,const char* d) {
            if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[cp].tag!=10)return false;
            std::string rc,rn,rd;return pacing_resolve_member(cls,cp,rc,rn,rd)&&rc==c&&rn==n&&rd==d;
        };
        auto pattern_at=[&](int pc) {
            return recognize_residual_sleep(fr.method->code,pc,jvm_op_length,
                [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
                [&](int cp){return member(cp,"java/lang/Thread","sleep","(J)V");},
                [&](int cp){if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[cp].tag!=9)return 0;
                    std::string c,n,d;if(!pacing_resolve_member(cls,cp,c,n,d))return 0;return d=="J"?1:(d=="I"||d=="S"||d=="B"||d=="C"?2:0);});
        };
        auto found=pattern_at(fr.op_pc);
        if(!found.valid()) {
            for(int pc=0;pc<fr.op_pc;) {
                const int len=jvm_op_length(fr.method->code,pc);if(len<=0)break;
                const auto candidate=pattern_at(pc);
                if(candidate.valid()&&candidate.end_pc==fr.op_pc){found=candidate;break;}
                pc+=len;
            }
        }
        if(!found.valid())return false;
        s.reset();s.method=fr.method;s.pattern=found;
    }
    if(!speed_period_us()||r4::state.active||VMS_RS(g_paint_depth)||!VMS_EXEC(g_can_suspend))return true;
    const size_t depth=VMS_EXEC(g_active_stack)?VMS_EXEC(g_active_stack)->size():0;
    if(fr.op_pc==s.pattern.start_pc) {
        s.cancel();s.depth=depth;s.draws=t.owned_draws;s.start_ms=now;s.start_us=paint_clock_us();s.active=true;
    } else if(s.active&&s.depth==depth&&fr.op_pc==s.pattern.end_pc) {
        int64_t start=0;
        s.complete=residual_field(fr,s.pattern.start_field,start)&&start==s.start_ms&&
            now>=start&&now-start<=120&&t.owned_draws==s.draws+1;
        s.end_ms=now;
    }
    return true;
}
static int64_t speed_residual_sleep_wake_us(int64_t requested_ms) {
    auto& t=current_context();auto& s=t.residual_sleep;
    const auto* stack=VMS_EXEC(g_active_stack);const Frame* caller=stack&&!stack->empty()?&stack->back():nullptr;
    int64_t start=0,end=0,target=0;TimerFrameTask task;
    const int period=speed_period_us();
    const bool eligible=period&&requested_ms>0&&requested_ms<8&&!r4::state.active&&s.active&&s.complete&&
        caller&&caller->method==s.method&&caller->op_pc==s.pattern.sleep_pc&&stack->size()==s.depth&&
        !caller->locals.empty()&&caller->locals[0].ref==VMS_RS(g_current)&&VMS_RS(g_current)==VMS_RS(g_shown)&&
        t.owned_draws==s.draws+1&&t.frame_due_us>0&&&t==VMS_EXEC(g_speed_budget_owner)&&
        paint_clock_us()-s.start_us<=120000&&VMS_EXEC(g_game_clock).scale<=1.01&&
        !t.auto_fps.verified_gate_seen&&!timer_drawing_task(t,task)&&
        residual_field(*caller,s.pattern.start_field,start)&&residual_field(*caller,s.pattern.end_field,end)&&
        residual_field(*caller,s.pattern.period_field,target)&&target>=8&&target<=120&&
        start==s.start_ms&&end==s.end_ms&&end>=start&&requested_ms==target-(end-start);
    s.cancel();if(!eligible)return 0;
    const int64_t due=t.frame_due_us;t.frame_due_us=0;t.sleep_paced=true;
    t.frame_prep.arm(VMS_RS(g_current),period,caller->method,caller->op_pc);
    const int64_t now=paint_clock_us(),wake=t.frame_prep.wake_deadline(due,now,period);
    return speed_owned_frame_deadline(wake,now,requested_ms,VMS_EXEC(g_speed_cut_us),VMS_EXEC(g_speed_add_us));
}
