// Transient evidence only: clocks, Java values and original bytecode are read,
// never rewritten. A completed drawing alone does not certify a long wait.
static bool event_frame_member(const Class& cls,int cp,const char* owner,const char* name,const char* desc) {
    std::string c,n,d;
    return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==10&&pacing_resolve_member(cls,cp,c,n,d)&&
        (!owner||c==owner)&&(!name||n==name)&&d==desc;
}
static const Field* event_frame_field(const Frame& fr,int cp,bool extra) {
    const auto& cls=VMS_CL(g_classes)[fr.method->class_id];std::string c,n,d;
    if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[cp].tag!=9||!pacing_resolve_member(cls,cp,c,n,d)||d!=(extra?"I":"Z"))return nullptr;
    const auto loaded=VMS_CL(g_class_index).find(c);if(loaded==VMS_CL(g_class_index).end())return nullptr;
    const auto* field=find_field(loaded->second,n,d);
    return field&&!field->is_static&&!(field->flags&0x0040)?field:nullptr;
}
static bool event_frame_field_value(const Frame& fr,int cp,bool extra,int expected) {
    const auto* field=event_frame_field(fr,cp,extra);
    if(!field||fr.locals.empty())return false;
    const int ref=fr.locals[0].ref;
    return ref>0&&size_t(ref)<VMS_HEAP(g_heap).size()&&field->slot>=0&&
        type_compatible(ref,VMS_CL(g_classes)[field->owner].name)&&
        size_t(field->slot)<VMS_HEAP(g_heap)[ref].fields.size()&&
        !VMS_HEAP(g_heap)[ref].fields[field->slot].is_long&&VMS_HEAP(g_heap)[ref].fields[field->slot].i==expected;
}
static bool event_frame_environment(JavaThreadContext& t) {
    TimerFrameTask task;
    return speed_period_us()&&!r4::state.active&&VMS_EXEC(g_can_suspend)&&!VMS_RS(g_paint_depth)&&
        t.state==ThreadState::Runnable&&VMS_EXEC(g_midlet_state)==MidletState::Active&&
        !VMS_PLAT(g_host_paused)&&!VMS_PLAT(g_host_frozen236)&&!VMS_EXEC(g_notify_destroyed)&&!VMS_EXEC(g_notify_paused)&&
        !VMS_EXEC(g_game_clock).paused&&VMS_EXEC(g_game_clock).scale==1.0&&
        !t.auto_fps.verified_gate_seen&&!t.timer.queue_wait&&!timer_drawing_task(t,task);
}
static bool event_frame_owner(const Frame& fr,const EventFrameSleepState& s) {
    auto& t=current_context();
    auto live=[&](int ref,uint32_t generation) {return ref>0&&size_t(ref)<VMS_HEAP(g_heap).size()&&
        VMS_HEAP(g_heap)[ref].cls>=0&&VMS_HEAP(g_heap)[ref].generation==generation;};
    const int64_t age=paint_clock_us()-s.start_us;
    return s.active&&event_frame_environment(t)&&s.cap==VMS_EXEC(g_speed_cap)&&s.epoch==resver::g_epoch&&
        live(s.controller,s.controller_generation)&&live(s.canvas,s.canvas_generation)&&
        s.canvas==VMS_RS(g_current)&&s.canvas==VMS_RS(g_shown)&&fr.method==s.method&&fr.event_frame_cycle==s.cycle&&
        !fr.locals.empty()&&fr.locals[0].ref==s.controller&&age>=0&&age<=240000&&
        t.owned_draws==s.draws+1&&t.frame_due_us>0&&&t==VMS_EXEC(g_speed_budget_owner);
}
static void speed_event_frame_clock(Frame& fr,int64_t now,QuickSite& site) {
    auto& t=current_context();auto& s=t.event_frame_sleep;
    if(!speed_period_us()||r4::state.active)return;
    if(site.event_frame_sleep<0) {
        site.event_frame_sleep=0;
        if(fr.method->native||(fr.method->flags&(0x0008|0x0020)))return;
        const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
        const auto p=recognize_event_frame_sleep(fr.method->code,fr.op_pc,jvm_op_length,
            [&](int cp){return event_frame_member(cls,cp,"java/lang/System","currentTimeMillis","()J");},
            [&](int cp){return event_frame_member(cls,cp,nullptr,nullptr,"()Z");},
            [&](int cp,bool extra){return event_frame_field(fr,cp,extra)!=nullptr;},
            [&](int cp){return event_frame_member(cls,cp,"java/lang/Math","max","(JJ)J");},
            [&](int cp){return event_frame_member(cls,cp,nullptr,nullptr,"(J)V");},
            [&](int cp,bool wide)->int64_t {return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==(wide?5:3)?(wide?cls.cp[cp].l:cls.cp[cp].a):0;});
        if(!p.valid()||p.anchor+1>=fr.method->max_locals||p.extra>=fr.method->max_locals||p.request+1>=fr.method->max_locals)return;
        for(const auto& h:fr.method->handlers)if((h.start<p.loop_end&&h.end>p.start)||(h.pc>=p.start&&h.pc<p.loop_end))return;
        fr.method->event_frame_sleep_patterns.push_back(p);
        site.event_frame_sleep=int(fr.method->event_frame_sleep_patterns.size());
        fr.method->quick[p.end].event_frame_sleep=site.event_frame_sleep;
    }
    if(site.event_frame_sleep<=0)return;
    const auto p=fr.method->event_frame_sleep_patterns[size_t(site.event_frame_sleep-1)];
    const auto* stack=VMS_EXEC(g_active_stack);
    if(fr.op_pc==p.start) {
        s.reset();
        const int controller=fr.locals.empty()?0:fr.locals[0].ref,canvas=VMS_RS(g_current);
        if(!event_frame_environment(t)||!stack||stack->empty()||!fr.stack.empty()||
           controller<=0||size_t(controller)>=VMS_HEAP(g_heap).size()||canvas<=0||size_t(canvas)>=VMS_HEAP(g_heap).size()||
           canvas!=VMS_RS(g_shown)||!type_compatible(canvas,"javax/microedition/lcdui/Canvas")||
           speed_period_us()>=p.period_ms*1000)return;
        s.method=fr.method;s.pattern=p;s.controller=controller;s.canvas=canvas;s.cap=VMS_EXEC(g_speed_cap);
        s.epoch=resver::g_epoch;s.controller_generation=VMS_HEAP(g_heap)[controller].generation;s.canvas_generation=VMS_HEAP(g_heap)[canvas].generation;
        s.cycle=++t.event_frame_generation;fr.event_frame_cycle=s.cycle;s.depth=stack->size();s.draws=t.owned_draws;
        s.start_ms=now;s.start_us=paint_clock_us();s.active=true;
    } else if(fr.op_pc==p.end&&s.active) {
        const bool anchor=p.anchor>=0&&size_t(p.anchor)<fr.locals.size()&&fr.locals[p.anchor].is_long&&as_long(fr.locals[p.anchor])==s.start_ms;
        const bool extra=p.extra>=0&&size_t(p.extra)<fr.locals.size()&&!fr.locals[p.extra].is_long&&fr.locals[p.extra].i==p.period_ms;
        const bool operands=fr.stack.size()==2&&fr.stack[0].is_long&&fr.stack[1].is_long&&
            as_long(fr.stack[0])==p.floor_ms&&as_long(fr.stack[1])==2*p.period_ms;
        if(!stack||stack->size()!=s.depth||!event_frame_owner(fr,s)||s.pattern.start!=p.start||!anchor||!extra||!operands||
           now<s.start_ms||now-s.start_ms>240||!event_frame_field_value(fr,p.flag_field,false,0)||
           !event_frame_field_value(fr,p.extra_field,true,p.period_ms)){s.cancel();return;}
        s.end_ms=now;s.request_ms=std::max<int64_t>(p.floor_ms,2*p.period_ms-(now-s.start_ms));s.complete=true;
    }
}
static int event_frame_bridge_pc(Method& method) {
    if(method.event_frame_bridge>=0)return method.event_frame_bridge-1;
    method.event_frame_bridge=0;
    if(method.native||!(method.flags&0x0008)||(method.flags&0x0020)||method.desc!="(J)V"||method.max_locals<2)return -1;
    const auto& cls=VMS_CL(g_classes)[method.class_id];
    const auto p=recognize_event_frame_bridge(method.code,
        [&](int cp){return event_frame_member(cls,cp,"java/lang/Thread","sleep","(J)V");},
        [&](int cp){return event_frame_member(cls,cp,"java/lang/Throwable","printStackTrace","()V");});
    if(!p.valid())return -1;
    if(p.handler<0) {if(!method.handlers.empty())return -1;}
    else {
        if(method.handlers.size()!=1)return -1;const auto& h=method.handlers[0];
        if(h.start!=p.positive||h.end!=p.after_sleep||h.pc!=p.handler||h.catch_cp<=0||
           size_t(h.catch_cp)>=cls.cp.size()||cls.cp[h.catch_cp].tag!=7)return -1;
        const auto catch_name=cp_class(cls,h.catch_cp);
        if(catch_name!="java/lang/Exception"&&catch_name!="java/lang/InterruptedException")return -1;
    }
    method.event_frame_bridge=p.sleep+1;return p.sleep;
}
static int64_t speed_event_frame_sleep_wake_us(int64_t requested) {
    auto& t=current_context();auto& s=t.event_frame_sleep;const auto* stack=VMS_EXEC(g_active_stack);
    if(!s.active||!s.complete||!s.depth||requested<=0||requested!=s.request_ms||!stack||stack->size()!=s.depth+1){s.cancel();return 0;}
    const auto& parent=(*stack)[stack->size()-2];const auto& bridge=stack->back();const auto& p=s.pattern;
    const bool request=p.request>=0&&size_t(p.request)<parent.locals.size()&&parent.locals[p.request].is_long&&as_long(parent.locals[p.request])==requested;
    const bool original=bridge.locals.size()>=2&&bridge.locals[0].is_long&&as_long(bridge.locals[0])==requested;
    const bool member=event_frame_member(VMS_CL(g_classes)[parent.method->class_id],p.bridge_cp,
        VMS_CL(g_classes)[bridge.method->class_id].name.c_str(),bridge.method->name.c_str(),"(J)V");
    const bool valid=event_frame_owner(parent,s)&&parent.op_pc==p.bridge&&parent.pc==p.bridge+3&&
        parent.stack.empty()&&bridge.stack.empty()&&request&&original&&member&&event_frame_bridge_pc(*bridge.method)==bridge.op_pc&&
        event_frame_field_value(parent,p.flag_field,false,0)&&event_frame_field_value(parent,p.extra_field,true,p.period_ms);
    const int64_t start=s.start_us;s.cancel();if(!valid)return 0;
    const int period=speed_period_us();t.frame_due_us=0;t.sleep_paced=true;
    t.frame_prep.arm(s.canvas,period,bridge.method,bridge.op_pc);
    // Work since the first clock is already spent. Applying FramePrep's lead
    // here again would shorten the deliberate second frame a second time.
    return speed_owned_frame_deadline(start+2*int64_t(period),paint_clock_us(),requested,
        VMS_EXEC(g_speed_cut_us),VMS_EXEC(g_speed_add_us));
}
