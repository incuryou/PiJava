// Read-only waiting in proven clock/yield loops. Keep every original Java
// instruction and branch. These naps cannot certify a faster Java clock.
static CooperativePollPattern cooperative_poll_resolve(const Frame& fr,bool* retry=nullptr) {
    const auto& owner=VMS_CL(g_classes)[fr.method->class_id];
    auto call=[&](int cp,int kind) {
        if(cp<=0||size_t(cp)>=owner.cp.size()||owner.cp[cp].tag!=10)return false;
        std::string cls,name,desc;
        if(!pacing_resolve_member(owner,cp,cls,name,desc))return false;
        if(kind==0)return cls=="java/lang/System"&&name=="currentTimeMillis"&&desc=="()J";
        if(kind==1)return cls=="java/lang/Thread"&&name=="yield"&&desc=="()V";
        if(kind==2)return cls=="java/lang/Thread"&&name=="sleep"&&desc=="(J)V";
        return cls=="java/lang/Math"&&name=="min"&&desc=="(JJ)J";
    };
    auto v=recognize_cooperative_poll(fr.method->code,fr.op_pc,call,
        [&](int cp)->int64_t{return cp>0&&size_t(cp)<owner.cp.size()&&owner.cp[cp].tag==5?owner.cp[cp].l:0;},
        [&](int pc){return jvm_op_length(fr.method->code,pc);},
        [&](int pc){for(const auto& h:fr.method->handlers)if(h.pc==pc)return true;return false;});
    if(!v.valid())return {};
    if(v.kind==1) {
        const auto loaded=VMS_CL(g_class_index).find("java/lang/Math");
        if(loaded==VMS_CL(g_class_index).end()||VMS_CL(g_classes)[loaded->second].init_state<2) {
            // The first original min call may still need to initialize Math.
            // Do not initialize it speculatively or permanently reject the
            // already-valid shape merely because its dependency is not ready.
            if(retry)*retry=true;return {};
        }
        if(VMS_CL(g_classes)[loaded->second].init_state!=2)return {};
        const auto* m=find_method(loaded->second,"min","(JJ)J",false);
        static const uint8_t exact[]={30,32,148,156,0,7,30,167,0,4,32,173};
        if(!m||m->native||(m->flags&0x0528)!=0x0008||!m->handlers.empty()||
           m->code.size()!=sizeof(exact)||!std::equal(m->code.begin(),m->code.end(),exact))return {};
    }
    auto field=[&](int cp,bool stat,int& decl,int& index) {
        if(cp<=0||size_t(cp)>=owner.cp.size()||owner.cp[cp].tag!=9)return false;
        std::string cls,name,desc;
        if(!pacing_resolve_member(owner,cp,cls,name,desc))return false;
        if(!stat&&desc!="J")return false;
        if(stat&&desc!="I"&&desc!="B"&&desc!="S"&&desc!="C"&&desc!="Z")return false;
        const auto loaded=VMS_CL(g_class_index).find(cls);
        if(loaded==VMS_CL(g_class_index).end()){if(retry)*retry=true;return false;}
        const auto* f=find_field(loaded->second,name,desc);
        if(!f||f->is_static!=stat||(f->flags&0x0040)||f->owner<0||f->slot<0)return false;
        decl=f->owner;
        const auto& fields=VMS_CL(g_classes)[decl].fields;
        for(size_t i=0;i<fields.size();++i)if(&fields[i]==f){index=int(i);return true;}
        return false;
    };
    if(v.anchor_cp>=0&&!field(v.anchor_cp,false,v.anchor_owner,v.anchor_field))return {};
    if(v.period_cp>=0&&!field(v.period_cp,true,v.period_owner,v.period_field))return {};
    return v;
}

// An audio/request worker can remain Runnable while doing nothing except
// testing flags and yielding. Inspect its NEXT saved pc, never its stale
// op_pc. Follow only the currently selected read-only path back to that pc;
// any request, work, unknown instruction, volatile read or lock refuses it.
// This reads metadata/values only: no class initialization, Java execution,
// queue removal, frame mutation, or synthetic blocked-thread state.
static bool cooperative_poll_idle_peer(const JavaThreadContext& peer) {
    if(peer.state!=ThreadState::Runnable||peer.executing||peer.interrupt_exception||peer.frames.empty())return false;
    if(peer.ref>0&&thread_interrupted(peer.ref))return false;
    for(const auto& m:VMS_EXEC(g_monitors))if(m.second.owner==&peer&&m.second.depth>0)return false;
    const auto& fr=peer.frames.back();
    if(!fr.method||fr.method->native||!fr.entry_checked||!fr.stack.empty()||fr.init_super_pending)return false;
    const auto& code=fr.method->code;const auto& owner=VMS_CL(g_classes)[fr.method->class_id];
    const int start=fr.pc,n=int(code.size());int pc=start,sp=0,reads=0;int32_t stack[2]={};bool yielded=false;
    for(int steps=0;steps<32;++steps) {
        if(steps&&pc==start)return yielded&&reads>0&&sp==0;
        if(pc<0||pc>=n)return false;
        const int op=code[pc];
        if(op==178) {
            if(pc+2>=n||sp==2)return false;
            const int cp=(int(code[pc+1])<<8)|code[pc+2];
            const auto* field=pacing_loaded_static_integral(owner,cp);
            if(!field||VMS_CL(g_classes)[field->owner].init_state!=2)return false;
            stack[sp++]=VMS_CL(g_classes)[field->owner].statics[field->slot].i;
            ++reads;pc+=3;
        } else if(op>=153&&op<=158) {
            if(pc+2>=n||sp!=1)return false;const int32_t v=stack[--sp];
            const bool take=op==153?v==0:op==154?v!=0:op==155?v<0:op==156?v>=0:op==157?v>0:v<=0;
            pc=take?pc+int16_t((int(code[pc+1])<<8)|code[pc+2]):pc+3;
        } else if(op==167) {
            if(pc+2>=n||sp)return false;pc+=int16_t((int(code[pc+1])<<8)|code[pc+2]);
        } else if(op==184) {
            if(pc+2>=n||sp)return false;const int cp=(int(code[pc+1])<<8)|code[pc+2];
            if(cp<=0||size_t(cp)>=owner.cp.size()||owner.cp[cp].tag!=10)return false;
            std::string cls,name,desc;
            if(!pacing_resolve_member(owner,cp,cls,name,desc)||cls!="java/lang/Thread"||name!="yield"||desc!="()V")return false;
            const auto loaded=VMS_CL(g_class_index).find(cls);
            if(loaded==VMS_CL(g_class_index).end()||VMS_CL(g_classes)[loaded->second].init_state!=2)return false;
            const auto* method=find_method(loaded->second,name,desc,false);
            if(!method||!method->native||(method->flags&0x0028)!=0x0008)return false;
            yielded=true;pc+=3;
        } else return false;
    }
    return false;
}

static void cooperative_poll_clock(Frame& fr,int64_t& now,QuickSite& site,bool worker,int64_t slice_deadline_ms) {
    auto& obs=fr.cooperative_poll;
    if(site.cooperative_poll<0) {
        bool retry=false;const auto v=cooperative_poll_resolve(fr,&retry);site.cooperative_poll=retry?-1:0;
        if(v.valid()){fr.method->cooperative_polls.push_back(v);site.cooperative_poll=int(fr.method->cooperative_polls.size());}
    }
    if(site.cooperative_poll<=0)return;
    const auto& v=fr.method->cooperative_polls[size_t(site.cooperative_poll-1)];
    auto& ctx=current_context();
    auto quiet=[&]() {
        if(r4::state.active||VMS_RS(g_paint_depth)||VMS_PLAT(g_host_paused)||
           VMS_EXEC(g_midlet_state)!=MidletState::Active||VMS_EXEC(g_notify_paused)||VMS_EXEC(g_notify_destroyed)||
           VMS_RS(g_current)<=0||VMS_RS(g_current)!=VMS_RS(g_shown)||!ctx.owned_draws||
           VMS_EXEC(g_game_clock).paused||ctx.interrupt_exception||
           (ctx.ref>0&&thread_interrupted(ctx.ref))||p7_pending_ends())return false;
        // A synchronous callSerially callback already owns run-to-completion.
        // Its pending repaint remains queued until its ORIGINAL branch exits.
        if(worker&&(VMS_RS(g_repaint)||VMS_RS(g_fb_dirty)||!VMS_EXEC(g_serial_calls).empty()))return false;
        for(const auto& peer:VMS_EXEC(g_threads))
            if(&peer!=&ctx&&peer.state!=ThreadState::Terminated&&!cooperative_poll_idle_peer(peer))return false;
        const auto& main=VMS_EXEC(g_main_thread);
        if(&main!=&ctx&&!main.frames.empty()&&main.state!=ThreadState::Terminated&&!cooperative_poll_idle_peer(main))return false;
        for(const auto& m:VMS_EXEC(g_monitors))if(m.second.owner==&ctx&&m.second.depth>0)return false;
        if(worker) {
            const int64_t us=paint_clock_us(),host=VMS_EXEC(g_exec_ctx).native_poll_host_deadline_us;
            if(slice_deadline_ms<=us/1000+2||(host>0&&host-us<=2000))return false;
        }
        return true;
    };
    auto values=[&](int64_t& anchor,int64_t& period,int& receiver,uint32_t& generation) {
        if(!fr.stack.empty())return false;
        period=v.period_ms;receiver=0;generation=0;
        if(v.anchor_local>=0) {
            if(size_t(v.anchor_local)>=fr.locals.size()||!fr.locals[v.anchor_local].is_long)return false;
            anchor=fr.locals[v.anchor_local].l;
        } else {
            if(v.receiver<0||size_t(v.receiver)>=fr.locals.size())return false;
            receiver=fr.locals[v.receiver].ref;
            if(receiver<=0||size_t(receiver)>=VMS_HEAP(g_heap).size())return false;
            const auto& obj=VMS_HEAP(g_heap)[receiver];
            const auto& field=VMS_CL(g_classes)[v.anchor_owner].fields[v.anchor_field];
            if((field.flags&0x0040)||field.slot<0||size_t(field.slot)>=obj.fields.size()||
               !class_assignable(obj.cls,v.anchor_owner)||!obj.fields[field.slot].is_long)return false;
            anchor=obj.fields[field.slot].l;generation=obj.generation;
        }
        if(v.period_cp>=0) {
            const auto& cls=VMS_CL(g_classes)[v.period_owner];const auto& field=cls.fields[v.period_field];
            if(cls.init_state!=2||(field.flags&0x0040)||field.slot<0||size_t(field.slot)>=cls.statics.size())return false;
            period=cls.statics[field.slot].i;
        }
        return anchor>=0&&period>=2&&period<=120&&anchor<=INT64_MAX-period&&
            now>=anchor&&now<anchor+period&&(v.kind!=3||now>anchor);
    };
    int64_t anchor=0,period=0;int receiver=0;uint32_t generation=0;
    const auto& clock=VMS_EXEC(g_game_clock);
    if(!quiet()||!values(anchor,period,receiver,generation)||!std::isfinite(clock.scale)||clock.scale<=0) {obs.reset();return;}
    ++VMS_EXEC(g_poll_gate_reads);
    const bool same=obs.site==fr.op_pc&&obs.canvas==VMS_RS(g_current)&&obs.cap==VMS_EXEC(g_speed_cap)&&
        obs.receiver==receiver&&obs.generation==generation&&obs.draws==ctx.owned_draws&&
        obs.anchor==anchor&&obs.period==period&&obs.epoch==clock.real_us&&obs.scale==clock.scale&&now>=obs.last_ms;
    if(!same) {
        obs.reset();obs.site=fr.op_pc;obs.canvas=VMS_RS(g_current);obs.cap=VMS_EXEC(g_speed_cap);
        obs.receiver=receiver;obs.generation=generation;obs.draws=ctx.owned_draws;
        obs.anchor=anchor;obs.period=period;obs.epoch=clock.real_us;obs.scale=clock.scale;
    }
    obs.last_ms=now;if(obs.reads<4)++obs.reads;if(obs.reads<4)return;
    auto requested=[&]()->int64_t {
        // Keep at least a real millisecond for the original Java comparison.
        const double real_left=double(anchor+period-now)*1000.0/clock.scale;
        return real_left>1000.0?int64_t(std::min(500.0,real_left-1000.0)):0;
    };
    if(!requested())return;
    audio_pump();now=game_current_ms();
    if(!quiet()||!values(anchor,period,receiver,generation)||anchor!=obs.anchor||period!=obs.period||
       receiver!=obs.receiver||generation!=obs.generation||clock.real_us!=obs.epoch||clock.scale!=obs.scale||
       ctx.owned_draws!=obs.draws||VMS_RS(g_current)!=obs.canvas||VMS_EXEC(g_speed_cap)!=obs.cap||now<obs.last_ms) {obs.reset();return;}
    const int64_t nap=requested();if(nap<=0)return;
    const int64_t before=paint_clock_us();
    {RuntimeProfileScope wait(VMS_EXEC(g_runtime_profile),RuntimeProfile::Kind::PollWait);
     P7_SCOPE(Idle);std::this_thread::sleep_for(std::chrono::microseconds(nap));}
    VMS_EXEC(g_poll_wait_us)+=paint_clock_us()-before;++VMS_EXEC(g_poll_waits);
    now=game_current_ms();obs.last_ms=now;
}
