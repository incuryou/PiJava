// This protocol changes one certified waiting branch, never Java time, fields,
// callbacks or arbitrary sleeps. Structural results live in QuickSite.
static void speed_rechecked_clock(const Frame& fr,int64_t now,QuickSite& site) {
    auto& t=current_context();auto& s=t.rechecked_deadline;
    if(site.rechecked_deadline<0) {
        const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
        auto member=[&](int cp,const char* c,const char* n,const char* d){return frame_wait_member(cls,cp,c,n,d);};
        const auto p=recognize_rechecked_deadline(fr.method->code,fr.op_pc,jvm_op_length,
            [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
            [&](int cp){return member(cp,"java/lang/Thread","sleep","(J)V");},
            [&](int cp){return member(cp,"java/lang/Math","max","(II)I");},
            [&](int cp){std::string c,n,d;return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==9&&pacing_resolve_member(cls,cp,c,n,d)&&d=="J";},
            [&](int cp){return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==5?cls.cp[cp].l:int64_t(0);});
        site.rechecked_deadline=0;
        if(p.valid()){fr.method->has_rechecked_deadline=true;fr.method->rechecked_deadlines.push_back(p);site.rechecked_deadline=int(fr.method->rechecked_deadlines.size());}
    }
    if(!site.rechecked_deadline)return;
    const auto p=fr.method->rechecked_deadlines[site.rechecked_deadline-1];
    if(s.method!=fr.method||s.pattern.clock_pc!=fr.op_pc){s.reset();s.method=fr.method;s.pattern=p;}
    const auto* stack=VMS_EXEC(g_active_stack);TimerFrameTask task;
    const int cap=speed_period_us();int64_t anchor=0;
    FrameWaitValue source;source.field=p.anchor_field;
    const bool valid=cap&&int64_t(cap)<p.period_ms*1000&&!r4::state.active&&!VMS_RS(g_paint_depth)&&VMS_EXEC(g_can_suspend)&&
        stack&&!fr.locals.empty()&&fr.locals[0].ref==VMS_RS(g_current)&&VMS_RS(g_current)==VMS_RS(g_shown)&&
        !t.timer.queue_wait&&!timer_drawing_task(t,task)&&!t.auto_fps.verified_gate_seen&&VMS_EXEC(g_game_clock).scale<=1.01&&
        frame_wait_value(fr,source,anchor)&&now>=anchor&&now-anchor<=120;
    if(!valid){s.cancel();return;}
    const size_t depth=stack->size();const int canvas=VMS_RS(g_current);
    if(s.canvas!=canvas||s.depth!=depth){s.cancel();s.canvas=canvas;s.depth=depth;}
    s.sample_now=now;s.sampled=true;
    if(s.active) {
        if(t.owned_draws!=s.cycle_draws||anchor!=s.anchor||&t!=VMS_EXEC(g_speed_budget_owner)||paint_clock_us()-s.started_us>120000)s.cancel();
        return;
    }
    if(s.have_ready&&anchor==s.ready_now&&t.owned_draws==s.ready_draws+1&&t.frame_due_us>0&&&t==VMS_EXEC(g_speed_budget_owner)) {
        s.active=true;s.anchor=anchor;s.cycle_draws=t.owned_draws;s.due_us=t.frame_due_us;s.started_us=paint_clock_us();
    }
}
static int speed_rechecked_branch(Frame& fr,int pc,int remainder) {
    auto& t=current_context();auto& s=t.rechecked_deadline;
    if(s.method!=fr.method||s.pattern.branch_pc!=pc)return remainder;
    const auto* stack=VMS_EXEC(g_active_stack);
    const auto& p=s.pattern;FrameWaitValue source;source.field=p.anchor_field;int64_t anchor=0;
    const bool sample=s.sampled&&stack&&stack->size()==s.depth&&!fr.locals.empty()&&fr.locals[0].ref==s.canvas&&
        p.now_local>=0&&size_t(p.now_local)<fr.locals.size()&&fr.locals[p.now_local].is_long&&fr.locals[p.now_local].l==s.sample_now&&
        p.remainder_local>=0&&size_t(p.remainder_local)<fr.locals.size()&&fr.locals[p.remainder_local].i==remainder&&
        frame_wait_value(fr,source,anchor)&&s.sample_now>=anchor&&s.sample_now-anchor<=120;
    if(!sample){s.cancel();return remainder;}
    // Preserve Thread.sleep's interrupt delivery point, even after a frame's
    // cap deadline. t.ref cannot allocate Java objects inside the fast batch.
    if(thread_interrupted(t.ref)){s.cancel();return remainder;}
    const bool usable=s.active&&remainder==p.period_ms-(s.sample_now-anchor)&&anchor==s.anchor&&speed_period_us()&&!r4::state.active&&!VMS_RS(g_paint_depth)&&
        s.canvas==VMS_RS(g_current)&&VMS_RS(g_current)==VMS_RS(g_shown)&&s.cycle_draws==t.owned_draws&&
        &t==VMS_EXEC(g_speed_budget_owner)&&VMS_EXEC(g_game_clock).scale<=1.01;
    if(usable&&remainder>0&&paint_clock_us()>=s.due_us){remainder=0;t.frame_due_us=0;}
    if(remainder<=0){s.active=false;s.sampled=false;s.have_ready=true;s.ready_now=s.sample_now;s.ready_draws=t.owned_draws;s.due_us=0;}
    return remainder;
}
static int64_t speed_rechecked_sleep_wake_us(int64_t requested_ms) {
    auto& t=current_context();auto& s=t.rechecked_deadline;
    const auto* stack=VMS_EXEC(g_active_stack);const Frame* caller=stack&&!stack->empty()?&stack->back():nullptr;
    const int cap=speed_period_us();TimerFrameTask task;
    if(!s.active||!cap||!caller||caller->method!=s.method||caller->op_pc!=s.pattern.sleep_pc||stack->size()!=s.depth||
       requested_ms<=0||requested_ms>120||requested_ms!=s.pattern.period_ms-(s.sample_now-s.anchor)||
       s.canvas!=VMS_RS(g_current)||VMS_RS(g_current)!=VMS_RS(g_shown)||t.owned_draws!=s.cycle_draws||
       &t!=VMS_EXEC(g_speed_budget_owner)||r4::state.active||VMS_RS(g_paint_depth)||VMS_EXEC(g_game_clock).scale>1.01||
       timer_drawing_task(t,task)||paint_clock_us()-s.started_us>120000)return 0;
    if(t.frame_due_us) {
        t.frame_prep.arm(s.canvas,cap,caller->method,caller->op_pc);
        s.due_us=t.frame_prep.wake_deadline(t.frame_due_us,paint_clock_us(),cap);t.frame_due_us=0;
    }
    t.sleep_paced=true;
    return speed_owned_frame_deadline(s.due_us,paint_clock_us(),requested_ms,VMS_EXEC(g_speed_cut_us),VMS_EXEC(g_speed_add_us));
}
