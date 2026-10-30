// Separate from both the Timer proof and the existing clock-polling learner.
static void speed_frame_wait_cancel_request(JavaThreadContext& t) {
    if(VMS_EXEC(g_exec_ctx).frame_wait_request_owner==t.ref)VMS_EXEC(g_exec_ctx).frame_wait_request_owner=0;
    t.wait_frame.cancel();
}
static bool frame_wait_member(const Class& owner,int cp,const char* cls,const char* name,const char* desc) {
    if(cp<=0||size_t(cp)>=owner.cp.size()||owner.cp[size_t(cp)].tag!=10)return false;
    std::string c,n,d;return pacing_resolve_member(owner,cp,c,n,d)&&c==cls&&n==name&&d==desc;
}
static bool frame_wait_value(const Frame& fr,const FrameWaitValue& source,int64_t& value) {
    if(source.local>=0) {
        if(size_t(source.local)>=fr.locals.size()||!fr.locals[size_t(source.local)].is_long)return false;
        value=fr.locals[size_t(source.local)].l;return true;
    }
    if(source.static_field) {
        const auto* field=pacing_loaded_static_long(VMS_CL(g_classes)[fr.method->class_id],source.static_field);
        if(!field||(field->flags&0x0040))return false;
        const auto stored=VMS_CL(g_classes)[field->owner].statics[size_t(field->slot)];
        if(!stored.is_long)return false;
        value=stored.l;return true;
    }
    if(!source.field||fr.locals.empty())return false;
    const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
    std::string c,n,d;if(!pacing_resolve_member(cls,source.field,c,n,d))return false;
    const int ref=fr.locals[0].ref;
    if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return false;
    const auto declared=VMS_CL(g_class_index).find(c);
    if(declared==VMS_CL(g_class_index).end()||!type_compatible(ref,c))return false;
    const auto& obj=VMS_HEAP(g_heap)[ref];const auto* field=find_field(declared->second,n,d);
    if(!field||field->is_static||field->slot<0||size_t(field->slot)>=obj.fields.size())return false;
    value=as_long(obj.fields[size_t(field->slot)]);return true;
}
static bool speed_frame_wait_clock(const Frame& fr,int64_t now_ms) {
    auto& t=current_context();auto& s=t.wait_frame;
    if(s.method!=fr.method||!s.pattern.valid()||
       (fr.op_pc!=s.pattern.start_pc&&fr.op_pc!=s.pattern.end_pc)) {
        const auto& cls=VMS_CL(g_classes)[fr.method->class_id];
        auto member=[&](int cp,const char* c,const char* n,const char* d){return frame_wait_member(cls,cp,c,n,d);};
        auto recognize=[&](int pc){auto pattern=recognize_frame_wait(fr.method->code,pc,jvm_op_length,
            [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
            [&](int cp){return member(cp,"java/lang/Object","wait","(J)V");},
            [&](int cp){return member(cp,"java/lang/Thread","yield","()V");},
            [&](int cp){if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[size_t(cp)].tag!=9)return false;
                std::string c,n,d;return pacing_resolve_member(cls,cp,c,n,d)&&d=="J";},
            [&](int cp){return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[size_t(cp)].tag==5?cls.cp[size_t(cp)].l:int64_t(0);});
            if(pattern.valid())return pattern;
            if(!(fr.method->flags&0x0028))pattern=recognize_static_frame_wait(fr.method->code,pc,jvm_op_length,
                [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
                [&](int cp){return member(cp,"java/lang/Object","wait","(J)V");},
                [&](int cp)->int64_t {
                    std::string c,n,d;
                    if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[cp].tag!=9||!pacing_resolve_member(cls,cp,c,n,d))return -1;
                    if(d!="J")return 0;
                    const auto* field=pacing_loaded_static_long(cls,cp);
                    if(!field||(field->flags&0x0040))return -1;
                    return (int64_t(field->owner+1)<<32)+field->slot+1;
                },
                [&](int cp){return cp>0&&size_t(cp)<cls.cp.size()&&cls.cp[cp].tag==5?cls.cp[cp].l:int64_t(0);},
                [&](int begin,int end,int enter,int leave,int lock) {
                    const auto& code=fr.method->code;bool cleanup=false;
                    auto local=[&](int& at,int generic,int compact) {
                        if(at<0||size_t(at)>=code.size())return -1;
                        const int op=code[size_t(at++)];
                        if(op>=compact&&op<compact+4)return op-compact;
                        return op==generic&&size_t(at)<code.size()?int(code[size_t(at++)]):-1;
                    };
                    for(const auto& h:fr.method->handlers) {
                        const bool releases=h.start==enter+1&&h.end==leave+1;
                        if(!releases&&!(h.pc>begin&&h.pc<end))continue;
                        int at=h.pc;const int exception=local(at,0x3a,0x4b);
                        if(h.catch_cp||exception<0||exception==lock||local(at,0x19,0x2a)!=lock||
                           size_t(at)>=code.size()||code[size_t(at++)]!=0xc3)return false;
                        const int released=at;
                        if(local(at,0x19,0x2a)!=exception||size_t(at)>=code.size()||code[size_t(at)]!=0xbf)return false;
                        if(releases)cleanup=true;
                        else if(h.start!=h.pc||h.end!=released)return false;
                    }
                    return cleanup;
                });
            if(pattern.valid())return pattern;
            pattern=recognize_branched_frame_wait(fr.method->code,pc,jvm_op_length,
                [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
                [&](int cp){return member(cp,"java/lang/Object","wait","(J)V");},
                [&](int cp){return member(cp,"java/lang/Thread","yield","()V");});
            if(pattern.valid())return pattern;
            return recognize_async_frame_wait(fr.method->code,pc,jvm_op_length,
                [&](int cp){return member(cp,"java/lang/System","currentTimeMillis","()J");},
                [&](int cp){return member(cp,"java/lang/Object","wait","(J)V");},
                [&](int cp){return member(cp,"java/lang/Thread","yield","()V");},
                [&](int cp){if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[size_t(cp)].tag!=9)return false;
                    std::string c,n,d;return pacing_resolve_member(cls,cp,c,n,d)&&d=="I";},
                [&](int cp){
                    if(cp<=0||size_t(cp)>=cls.cp.size()||cls.cp[size_t(cp)].tag!=10)return false;
                    std::string c,n,d;if(!pacing_resolve_member(cls,cp,c,n,d)||n!="repaint"||d!="()V")return false;
                    const auto declared=VMS_CL(g_class_index).find(c);
                    const auto* m=declared==VMS_CL(g_class_index).end()?nullptr:find_method(declared->second,n,d,true);
                    return m&&VMS_CL(g_classes)[m->class_id].name=="javax/microedition/lcdui/Canvas";
                });};
        auto pattern=recognize(fr.op_pc);
        // A reset may happen between the two clock reads. Recognize an end
        // structurally as well, so its site's negative cache stays correct.
        if(!pattern.valid())for(int pc=0;pc<fr.op_pc;) {
            const int len=jvm_op_length(fr.method->code,size_t(pc));
            if(len<=0||pc+len>int(fr.method->code.size()))break;
            if(fr.method->code[size_t(pc)]==0xb8) {
                const auto candidate=recognize(pc);
                if(candidate.valid()&&candidate.end_pc==fr.op_pc){pattern=candidate;break;}
            }
            pc+=len;
        }
        if(!pattern.valid())return false;
        speed_frame_wait_cancel_request(t);s.reset();s.method=fr.method;s.pattern=pattern;
    }
    // Only structural rejection is cached in QuickSite.frame_wait. Native
    // mode and transient callback/visibility conditions may change later.
    if(!speed_period_us()||r4::state.active||VMS_RS(g_paint_depth)||!VMS_EXEC(g_can_suspend)||
       fr.locals.empty()||fr.locals[0].ref!=VMS_RS(g_current)||VMS_RS(g_current)!=VMS_RS(g_shown))return true;
    const size_t depth=VMS_EXEC(g_active_stack)?VMS_EXEC(g_active_stack)->size():0;
    if(fr.op_pc==s.pattern.start_pc) {
        speed_frame_wait_cancel_request(t);
        if(s.pattern.start.static_field&&!fr.stack.empty())return true;
        if(s.pattern.period.field) {
            int64_t period=0;
            if(!frame_wait_value(fr,s.pattern.period,period)||period<8||period>120) {s.stable_samples=0;return true;}
            s.observed_period=period;s.pattern.period_ms=period;
            if(s.stable_period!=period){s.stable_period=period;s.stable_samples=0;}
        }
        s.canvas=VMS_RS(g_current);s.cap_period=speed_period_us();
        s.depth=depth;s.draws=t.owned_draws;s.started_ms=now_ms;s.started_us=paint_clock_us();s.active=true;
    } else if(s.active&&s.depth==depth&&fr.op_pc==s.pattern.end_pc) {
        int64_t previous=0;
        s.complete=frame_wait_value(fr,s.pattern.start,previous)&&previous==s.started_ms&&now_ms>=previous&&
            now_ms-previous<s.pattern.period_ms&&
            (!s.pattern.start.static_field||(fr.stack.empty()&&t.owned_draws==s.draws+1))&&
            (s.pattern.repaint_pc>=0?s.requested:(t.owned_draws>s.draws&&t.frame_due_us>0));
        s.elapsed_ms=now_ms-s.started_ms;
    }
    return true;
}
static int64_t speed_frame_wait_wake_us(int ref,int64_t requested_ms,JavaThreadContext& t) {
    auto& s=t.wait_frame;
    const auto* stack=VMS_EXEC(g_active_stack);
    const Frame* caller=stack&&!stack->empty()?&stack->back():nullptr;
    int64_t elapsed=0;TimerFrameTask task;
    const int period=speed_period_us();
    const bool async=s.pattern.repaint_pc>=0;
    int64_t field_period=0;
    int64_t static_anchor=0;
    const bool static_values=!s.pattern.start.static_field||(caller&&frame_wait_value(*caller,s.pattern.start,static_anchor)&&
        static_anchor==s.started_ms&&t.owned_draws==s.draws+1);
    const bool local_period=s.pattern.period_local<0||(caller&&
        size_t(s.pattern.period_local)<caller->locals.size()&&
        !caller->locals[size_t(s.pattern.period_local)].is_long&&
        caller->locals[size_t(s.pattern.period_local)].i==s.pattern.period_ms);
    const bool stable=local_period&&(!async||(caller&&frame_wait_value(*caller,s.pattern.period,field_period)&&field_period==s.observed_period));
    const bool eligible=period&&requested_ms>0&&requested_ms<=120&&!r4::state.active&&
        s.active&&s.complete&&caller&&caller->method==s.method&&caller->op_pc==s.pattern.wait_pc&&stack->size()==s.depth&&
        !caller->locals.empty()&&caller->locals[0].ref==ref&&ref==VMS_RS(g_current)&&ref==VMS_RS(g_shown)&&
        (async?s.requested:(t.owned_draws>s.draws&&t.frame_due_us>0&&&t==VMS_EXEC(g_speed_budget_owner)))&&
        stable&&static_values&&s.canvas==VMS_RS(g_current)&&s.cap_period==period&&
        paint_clock_us()-s.started_us<=std::max<int64_t>(120000,int64_t(period)*4)&&
        !t.auto_fps.verified_gate_seen&&!t.timer.queue_wait&&!timer_drawing_task(t,task)&&
        VMS_EXEC(g_game_clock).scale<=1.01&&frame_wait_value(*caller,s.pattern.elapsed,elapsed)&&
        elapsed==s.elapsed_ms&&requested_ms==s.pattern.period_ms-elapsed;
    if(async&&eligible) {
        s.stable_samples=std::min(3u,s.stable_samples+1);
        if(s.stable_samples>=2){s.awaiting_paint=true;return 0;}
    }
    speed_frame_wait_cancel_request(t);
    if(!eligible||async)return 0;
    const int64_t due=t.frame_due_us;t.frame_due_us=0;t.sleep_paced=true;
    t.frame_prep.arm(VMS_RS(g_current),period,caller->method,caller->op_pc);
    const int64_t now=paint_clock_us(),wake=t.frame_prep.wake_deadline(due,now,period);
    s.paced=true;
    return speed_owned_frame_deadline(wake,now,requested_ms,VMS_EXEC(g_speed_cut_us),VMS_EXEC(g_speed_add_us));
}

// A pending MIDP repaint is coalesced globally. Replacing it revokes the old
// request certificate, even when another Canvas or the same thread requested
// the replacement. No wait deadline is changed at this point.
static void speed_frame_wait_repaint(int canvas) {
    const int previous=VMS_EXEC(g_exec_ctx).frame_wait_request_owner;
    if(previous) {
        VMS_EXEC(g_exec_ctx).frame_wait_request_owner=0;
        if(auto* owner=thread_context(previous))owner->wait_frame.cancel();
    }
    auto& t=current_context();auto& s=t.wait_frame;
    const auto* stack=VMS_EXEC(g_active_stack);
    if(!speed_period_us()||r4::state.active||VMS_RS(g_paint_depth)||!VMS_EXEC(g_can_suspend)||
       !s.active||s.pattern.repaint_pc<0||!stack||stack->size()!=s.depth||stack->empty()||
       stack->back().method!=s.method||stack->back().op_pc!=s.pattern.repaint_pc||
       canvas!=s.canvas||canvas!=VMS_RS(g_current)||canvas!=VMS_RS(g_shown)||
       stack->back().locals.empty()||stack->back().locals[0].ref!=canvas||t.timer.queue_wait)return;
    TimerFrameTask task;if(timer_drawing_task(t,task))return;
    ++s.request;s.requested=true;VMS_EXEC(g_exec_ctx).frame_wait_request_owner=t.ref;
}
static uint64_t speed_frame_wait_paint_token(JavaThreadContext* owner,int canvas) {
    if(!owner)return 0;
    const auto& s=owner->wait_frame;
    return VMS_EXEC(g_exec_ctx).frame_wait_request_owner==owner->ref&&s.requested&&s.awaiting_paint&&s.canvas==canvas?s.request:0;
}
static void speed_frame_wait_painted(JavaThreadContext* owner,int canvas,uint64_t request) {
    if(!owner||!request)return;
    auto& t=*owner;auto& s=t.wait_frame;
    const int period=speed_period_us();const int64_t now=paint_clock_us();
    TimerFrameTask task;int64_t field_period=0;
    const bool valid=period&&period==s.cap_period&&!r4::state.active&&s.active&&s.complete&&s.requested&&
        s.awaiting_paint&&s.request==request&&s.canvas==canvas&&canvas==VMS_RS(g_current)&&canvas==VMS_RS(g_shown)&&
        t.state==ThreadState::Waiting&&t.monitor==canvas&&t.reacquire&&t.wake>monotonic_ms()&&!t.wake_us&&
        t.frames.size()==s.depth&&!t.frames.empty()&&t.frames.back().method==s.method&&t.frames.back().op_pc==s.pattern.wait_pc&&
        frame_wait_value(t.frames.back(),s.pattern.period,field_period)&&field_period==s.observed_period&&
        t.owned_draws==s.draws+1&&t.frame_due_us>0&&&t==VMS_EXEC(g_speed_budget_owner)&&
        now-s.started_us<=std::max<int64_t>(120000,int64_t(period)*4)&&
        !t.auto_fps.verified_gate_seen&&!t.timer.queue_wait&&!timer_drawing_task(t,task)&&VMS_EXEC(g_game_clock).scale<=1.01;
    speed_frame_wait_cancel_request(t);if(!valid)return;
    const int64_t due=t.frame_due_us;t.frame_due_us=0;t.sleep_paced=true;
    t.frame_prep.arm(canvas,period,s.method,s.pattern.wait_pc);
    const int64_t wake=t.frame_prep.wake_deadline(due,now,period);
    t.wake_us=speed_owned_frame_deadline(wake,now,s.pattern.period_ms-s.elapsed_ms,
        VMS_EXEC(g_speed_cut_us),VMS_EXEC(g_speed_add_us));
    t.wake=thread_deadline(std::max<int64_t>(1,(t.wake_us-now+999)/1000));
    s.paced=true;
}
