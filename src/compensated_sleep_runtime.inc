static bool compensated_sleep_owner(const Frame& fr,const CompensatedSleepState& s) {
    auto& t=current_context();const auto* stack=VMS_EXEC(g_active_stack);TimerFrameTask task;
    return speed_period_us()&&s.cap==VMS_EXEC(g_speed_cap)&&!r4::state.active&&!VMS_RS(g_paint_depth)&&
        VMS_EXEC(g_can_suspend)&&stack&&stack->size()==s.depth&&fr.method==s.method&&
        !fr.locals.empty()&&fr.locals[0].ref==s.canvas&&s.canvas==VMS_RS(g_current)&&s.canvas==VMS_RS(g_shown)&&
        &t==VMS_EXEC(g_speed_budget_owner)&&VMS_EXEC(g_game_clock).scale==1.0&&
        !t.auto_fps.verified_gate_seen&&!timer_drawing_task(t,task);
}
static void speed_compensated_clock(const Frame& fr,int64_t now) {
    auto& t=current_context();auto& s=t.compensated_sleep;
    if(s.method!=fr.method)return;
    if(fr.op_pc==s.pattern.anchor_clock) {
        s.cancel();
        const auto* stack=VMS_EXEC(g_active_stack);
        if(!speed_period_us()||!stack||fr.locals.empty()||fr.locals[0].ref!=VMS_RS(g_current)||
           VMS_RS(g_current)!=VMS_RS(g_shown)||VMS_RS(g_paint_depth)||r4::state.active)return;
        s.cap=VMS_EXEC(g_speed_cap);s.canvas=fr.locals[0].ref;s.depth=stack->size();
        s.anchor_ms=now;s.draws=t.owned_draws;s.anchor_seen=true;
    } else if(s.pending&&fr.op_pc==s.pattern.after_clock) {
        if(!compensated_sleep_owner(fr,s)||t.owned_draws!=s.draws+1||
           paint_clock_us()-s.started_us>120000){s.cancel();return;}
        s.after_ms=now;s.after_seen=true;
    }
}
static void speed_compensated_sleep_begin(int64_t requested) {
    auto& t=current_context();auto& s=t.compensated_sleep;
    const auto* stack=VMS_EXEC(g_active_stack);const Frame* fr=stack&&!stack->empty()?&stack->back():nullptr;
    if(!speed_period_us()||!fr||requested<=0||requested>120){s.cancel();return;}
    auto& site=fr->method->quick[fr->op_pc];
    if(site.compensated_sleep<0) {
        site.compensated_sleep=0;const auto& cls=VMS_CL(g_classes)[fr->method->class_id];
        auto member=[&](int cp,const char* c,const char* n,const char* d) {
            std::string rc,rn,rd;return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==10&&
                pacing_resolve_member(cls,cp,rc,rn,rd)&&rc==c&&rn==n&&rd==d;
        };
        const auto p=recognize_compensated_sleep(fr->method->code,fr->op_pc,jvm_op_length,
            [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
            [&](int cp){return member(cp,"java/lang/Thread","sleep","(J)V");},
            [&](int cp){int64_t ignored=0;return cached_elapsed_static(*fr,cp,ignored);});
        if(p.valid()){fr->method->compensated_sleep_patterns.push_back(p);site.compensated_sleep=int(fr->method->compensated_sleep_patterns.size());}
    }
    if(!site.compensated_sleep){s.cancel();return;}
    const auto p=fr->method->compensated_sleep_patterns[site.compensated_sleep-1];
    if(s.method!=fr->method||s.pattern.sleep!=p.sleep){s.reset();s.method=fr->method;s.pattern=p;return;}
    auto value=[&](int slot,int64_t& out){if(slot<0||size_t(slot)>=fr->locals.size()||!fr->locals[slot].is_long)return false;out=as_long(fr->locals[slot]);return true;};
    int64_t now=0,anchor=0,elapsed=0,error=0,request=0,period=0;
    const bool own=s.anchor_seen&&compensated_sleep_owner(*fr,s)&&t.frame_due_us>0&&t.owned_draws==s.draws+1&&
        value(p.now,now)&&value(p.anchor,anchor)&&value(p.elapsed,elapsed)&&value(p.error,error)&&value(p.request,request)&&
        cached_elapsed_static(*fr,p.period_field,period)&&period>=8&&period<=120&&period*1000>speed_period_us()&&
        anchor==s.anchor_ms&&now>=anchor&&now-anchor<=120&&elapsed==now-anchor&&error>=-120&&error<=120&&
        request==requested&&requested==period-elapsed-error;
    s.pending=s.after_seen=false;s.cut_ms=0;
    if(!own){s.cancel();return;}
    s.before_ms=now;s.requested_ms=requested;s.started_us=paint_clock_us();s.pending=true;
}
static void speed_compensated_sleep_retimed(int64_t requested,int64_t effective) {
    auto& s=current_context().compensated_sleep;
    if(!s.pending||s.requested_ms!=requested)return;
    // Retiming changes only this private pacing ledger, never a Java clock,
    // field, local variable, or the original argument passed to Thread.sleep.
    s.cut_ms=requested>effective?requested-effective:0;
}
static int64_t speed_compensated_subtract(const Frame& fr,int64_t a,int64_t b,int64_t result) {
    auto& t=current_context();auto& s=t.compensated_sleep;
    if(!s.pending||s.method!=fr.method||s.pattern.subtract!=fr.op_pc)return result;
    const bool valid=s.after_seen&&compensated_sleep_owner(fr,s)&&t.owned_draws==s.draws+1&&
        paint_clock_us()-s.started_us<=120000&&b==s.requested_ms&&a==s.after_ms-s.before_ms;
    const int64_t cut=s.cut_ms;s.pending=s.after_seen=false;s.cut_ms=0;
    return valid?long_bits(uint64_t(result)+uint64_t(cut)):result;
}
