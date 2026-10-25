static bool cached_elapsed_static(const Frame& fr,int cp,int64_t& value) {
    const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
    if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[cp].tag!=9)return false;
    std::string c,n,d;if(!pacing_resolve_member(cls,cp,c,n,d)||d!="J")return false;
    auto found=VMS_CL(g_class_index).find(c);if(found==VMS_CL(g_class_index).end())return false;
    const auto* f=find_field(found->second,n,d);
    // Volatile time fields can be inter-thread signals, never frame evidence.
    if(!f||!f->is_static||(f->flags&0x0040)||f->slot<0)return false;
    const auto v=VMS_CL(g_classes)[f->owner].statics[f->slot];value=as_long(v);return v.is_long;
}
static bool cached_elapsed_owns(const Frame& fr,const CachedElapsedState& s) {
    const auto& t=current_context();const auto* stack=VMS_EXEC(g_active_stack);TimerFrameTask task;
    int64_t anchor=0,elapsed=0;
    return s.active&&s.method==fr.method&&speed_period_us()&&s.cap==VMS_EXEC(g_speed_cap)&&
        !r4::state.active&&!VMS_RS(g_paint_depth)&&VMS_EXEC(g_can_suspend)&&stack&&stack->size()==s.depth&&
        !fr.locals.empty()&&fr.locals[0].ref==s.canvas&&s.canvas==VMS_RS(g_current)&&s.canvas==VMS_RS(g_shown)&&
        &t==VMS_EXEC(g_speed_budget_owner)&&t.owned_draws==s.draws+1&&
        t.field_gate_anchor.real_us==s.anchor_us&&t.field_gate_anchor.method==s.method&&
        t.field_gate_anchor.clock_pc==s.pattern.anchor_clock&&t.field_gate_anchor.field==s.pattern.anchor_field&&
        paint_clock_us()>=s.anchor_us&&paint_clock_us()-s.anchor_us<=120000&&
        VMS_EXEC(g_game_clock).scale==1.0&&!t.auto_fps.verified_gate_seen&&
        !timer_drawing_task(current_context(),task)&&
        cached_elapsed_static(fr,s.pattern.anchor_field,anchor)&&anchor==s.anchor_ms&&
        cached_elapsed_static(fr,s.pattern.elapsed_field,elapsed)&&elapsed==s.latest_ms-s.anchor_ms;
}
static void speed_cached_elapsed_clock(const Frame& fr,int64_t now,QuickSite& site) {
    if(site.cached_elapsed==0)return;
    if(!speed_period_us()||r4::state.active||VMS_RS(g_paint_depth)||!VMS_EXEC(g_can_suspend))return;
    if(site.cached_elapsed<0) {
        site.cached_elapsed=0;const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
        auto member=[&](int cp,const char* c,const char* n,const char* d) {
            std::string rc,rn,rd;return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==10&&
                pacing_resolve_member(cls,cp,rc,rn,rd)&&rc==c&&rn==n&&rd==d;
        };
        const auto pattern=recognize_cached_elapsed(fr.method->code,fr.op_pc,jvm_op_length,
            [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
            [&](int cp){return member(cp,"java/lang/Thread","sleep","(J)V");},
            [&](int cp)->int64_t{return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==5?cls.cp[cp].l:-1;},
            [&](int cp){int64_t ignored=0;return cached_elapsed_static(fr,cp,ignored);});
        if(pattern.valid()){fr.method->cached_elapsed_gates.push_back(pattern);site.cached_elapsed=int(fr.method->cached_elapsed_gates.size());}
    }
    if(site.cached_elapsed<=0)return;
    auto& t=current_context();auto& s=t.cached_elapsed;const auto& a=t.field_gate_anchor;
    const auto pattern=fr.method->cached_elapsed_gates[site.cached_elapsed-1];int64_t anchor=0;
    const bool own=a.method==fr.method&&a.clock_pc==pattern.anchor_clock&&a.field==pattern.anchor_field&&
        a.real_us>0&&a.real_us!=s.rejected_anchor_us&&t.owned_draws==a.draws+1&&
        cached_elapsed_static(fr,pattern.anchor_field,anchor)&&anchor==a.value&&now>=anchor&&now-anchor<=120;
    if(!own){s.active=false;return;}
    if(s.method!=fr.method||s.anchor_us!=a.real_us||s.pattern.branch!=pattern.branch) {
        const int64_t rejected=s.rejected_anchor_us;s.reset();s.rejected_anchor_us=rejected;
        s.method=fr.method;s.pattern=pattern;s.canvas=VMS_RS(g_current);s.cap=VMS_EXEC(g_speed_cap);
        s.depth=VMS_EXEC(g_active_stack)?VMS_EXEC(g_active_stack)->size():0;s.draws=a.draws;
        s.anchor_ms=anchor;s.anchor_us=a.real_us;s.due_us=t.frame_due_us;s.active=s.due_us>0;
    }
    s.latest_ms=now;
}
static bool speed_cached_elapsed_branch(Frame& fr,int pc,int comparison) {
    auto& t=current_context();auto& s=t.cached_elapsed;
    if(s.method!=fr.method||s.pattern.branch!=pc)return comparison>=0;
    if(thread_interrupted(t.ref)||!cached_elapsed_owns(fr,s)){s.cancel();return comparison>=0;}
    if(comparison>=0||paint_clock_us()>=s.due_us) {t.frame_due_us=0;s.active=false;return true;}
    return false;
}
static int64_t speed_cached_elapsed_sleep_wake_us(int64_t requested) {
    auto& t=current_context();auto& s=t.cached_elapsed;
    const auto* stack=VMS_EXEC(g_active_stack);const Frame* fr=stack&&!stack->empty()?&stack->back():nullptr;
    if(!fr||s.method!=fr->method||s.pattern.sleep!=fr->op_pc)return 0;
    if(requested!=s.pattern.sleep_ms||!cached_elapsed_owns(*fr,s)){s.cancel();return 0;}
    t.frame_due_us=0;t.sleep_paced=true;
    return speed_owned_frame_deadline(s.due_us,paint_clock_us(),requested,VMS_EXEC(g_speed_cut_us),VMS_EXEC(g_speed_add_us));
}
