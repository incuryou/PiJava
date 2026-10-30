// Tiered interpreter and call-site dispatch; safepoints retained; optional verified leaf JIT at static call sites.
// Included once by the runtime composition unit; not a standalone TU.
// The normal interpreter has no per-bytecode profiling branch. The diagnostic
// copy is selected once per run entry, and keeps exact method-transition CPU
// accounting. No Java semantics or opcode implementation is duplicated by hand.
#include "../interpreter_fast.inc"
#include "../flag_spin236.h"

template<bool ProfileMethods>
static Value run_impl(Method* entry, const std::vector<Value>& args) { P61_SCOPE(5, entry);
    ExecutionProfile::Context profile_context(VMS_EXEC(g_execution_profile),nullptr,ProfileMethods,true);
    bool use_thread = VMS_EXEC(g_resume_request);
    VMS_EXEC(g_resume_request) = false;
    std::vector<Frame> local;
    std::vector<Frame>* st = use_thread ? &current_context().frames : &local;
    RunScope scope(st, use_thread);
    Value result;
    auto push_frame = [&](Method* mth, const Value* a, int an) {
        city_method_enter(mth);
        if constexpr(ProfileMethods)VMS_EXEC(g_execution_profile).called(mth->cpu_profile);
        if (mth->native) {
            ExecutionProfile::Context native_profile(VMS_EXEC(g_execution_profile),&mth->cpu_profile,ProfileMethods);
            Frame fr; fr.method = mth;
#if J2ME_OPT_FRAME_POOL
            VMS_HEAP(g_frame_buffer_pool).acquire(fr.locals,size_t(an));
            VMS_HEAP(g_frame_buffer_pool).acquire(fr.stack,1);
#endif
            if(an)fr.locals.assign(a, a + an); fr.op_pc = st->empty() ? 0 : st->back().op_pc;
            call_native(mth, fr);
            if (!VMS_EXEC(g_thrown) && !fr.stack.empty()) { result = fr.stack.back(); if (!st->empty()) st->back().stack.push_back(result); }
            return;
        }
        Frame fr; fr.method = mth;
        if (mth->flags & 0x0020) fr.sync_monitor = (mth->flags & 0x0008) ? class_object(VMS_CL(g_classes)[mth->class_id].name) : a[0].ref;
        int lc = std::max<int>(mth->max_locals, an + 2);
#if J2ME_OPT_FRAME_POOL
        VMS_HEAP(g_frame_buffer_pool).acquire(fr.locals,size_t(lc));
        VMS_HEAP(g_frame_buffer_pool).acquire(fr.stack,mth->max_stack > 0 ? mth->max_stack : 8);
#else
        fr.stack.reserve(mth->max_stack > 0 ? mth->max_stack : 8);
#endif
        fr.locals.assign(lc, Value{});
        int slot = 0;
        for (int i = 0; i < an; ++i) {
            if (slot < lc) fr.locals[slot] = a[i];
            slot += a[i].is_long ? 2 : 1;
        }
        st->push_back(std::move(fr));
    };
    std::function<bool(int)> initialize = [&](int cid) -> bool {
        Class& c=VMS_CL(g_classes)[cid];
        if(c.init_state==2 || (c.init_state==1 && c.init_owner==&current_context())) return true;
        if(c.init_state==3) { throw_new("java/lang/NoClassDefFoundError",c.name+" initialization failed");return false; }
        if(c.init_state==1) {
            current_context().state=ThreadState::Initializing; current_context().init_wait=cid;
            suspend_current();return false;
        }
        Method* cl=find_method(cid,"<clinit>","()V",false);
        if(!cl) {
            c.init_stub.class_id=cid;c.init_stub.name="<clinit>";c.init_stub.desc="()V";
            c.init_stub.code={177};cl=&c.init_stub;
        }
        c.init_state=1;c.init_owner=&current_context();
        push_frame(cl,nullptr,0);st->back().init_class=cid;st->back().entry_checked=true;st->back().init_super_pending=true;
        return false;
    };
    if (entry) push_frame(entry, args.data(), (int)args.size());
    int r4_quantum=use_thread?1024:10000000;
    int guard = 0;
    int64_t slice_deadline = monotonic_ms() + VMS_EXEC(g_slice_ms);
    while (!st->empty() && !(use_thread && VMS_EXEC(g_yield))) {
        if (current_context().interrupt_exception) { deliver_interrupt(); if (st->empty()) break; }
        if (VMS_EXEC(g_thrown)) VMS_EXEC(g_thrown) = false;
        if(st->back().init_super_pending) {
            const Class& c=VMS_CL(g_classes)[st->back().init_class];
            if(c.super_id>=0 && !(c.flags & 0x0200) && !initialize(c.super_id))continue;
            st->back().init_super_pending=false;
        }
        if(!st->back().entry_checked) {
            int cls=st->back().method->class_id;
            if(st->back().method->name!="<clinit>" && !initialize(cls))continue;
            st->back().entry_checked=true;
        }
        if (st->back().sync_monitor && !st->back().sync_acquired) {
            if (!monitor_enter(st->back().sync_monitor)) { suspend_current(); continue; }
            st->back().sync_acquired = true;
        }
        if(r4::state.active&&r4::state.available(use_thread,r4_quantum)<=0) {
            if(use_thread){VMS_EXEC(g_yield)=true;break;}
            r4_abort("CALLBACK_FUEL_LIMIT");
        }
        ++guard;
        if (!r4::state.active && (guard > (VMS_EXEC(g_slice_ms) <= 4 ? 250000 : 1000000) || ((guard & 1023) == 0 && monotonic_ms() >= slice_deadline))) {
            city_sync_step(st->back(),st->size(),VMS_EXEC(g_fast_ops)+VMS_EXEC(g_slow_ops));
            diag_checkpoint(*st);
            guard = 0;
            VMS_EXEC(g_cuts)++;
            if(VMS_RS(g_paint_depth)>0)paint_hotspots241::sample(*st);
            else if(use_thread)paint_hotspots241::sample_worker(*st);
#ifdef J2ME_TRACE_THROW
            if(VMS_EXEC(g_cuts)<8 || (VMS_EXEC(g_cuts)%200)==0)log_line("TRACE_SLICE thread="+std::to_string(use_thread)+" paint="+std::to_string(VMS_RS(g_paint_depth))+" "+VMS_CL(g_classes)[st->back().method->class_id].name+"."+st->back().method->name+" pc="+std::to_string(st->back().pc));
#endif
#ifndef __SWITCH__
            audio_pump();
#endif
            S0_HOOK(s0_assert_poll(st->back(),use_thread,VMS_EXEC(g_can_suspend),VMS_EXEC(g_active_stack)==&current_context().frames));
            if (use_thread) {
                if (VMS_EXEC(g_cuts) <= 4)
                    log_line("SLICE cut " + VMS_CL(g_classes)[st->back().method->class_id].name + "." + st->back().method->name);
                VMS_EXEC(g_yield) = true;
                break;
            } else if (VMS_RS(g_paint_depth) == 0) pump_one_thread();
            else if(pure_static_poll236(st->back().method->code.data(),st->back().method->code.size(),st->back().pc)) {
                // A Java producer is allowed to clear an explicit read-only
                // flag spin inside paint. Never preempt an arbitrary drawing
                // body and never unlock a monitor or fabricate a flag value.
                if(pump_one_thread()) ++VMS_EXEC(g_flag_spin_pumps);
            }
            slice_deadline=monotonic_ms()+VMS_EXEC(g_slice_ms);
        }
        if (st->back().method->native) {
            result = st->back().stack.empty() ? Value{} : st->back().stack.back();
            st->pop_back();
            break;
        }
        if (VMS_EXEC(g_thrown)) { VMS_EXEC(g_thrown) = false; if (st->empty()) break; }
        if (st->empty()) break;
        size_t frame_index = st->size() - 1;
        Frame& fr = st->back();
        if constexpr(ProfileMethods)VMS_EXEC(g_execution_profile).activate(&fr.method->cpu_profile);
#if J2ME_OPT_FAST_DISPATCH
        // Stop BEFORE the next original 1024-op clock check. No safepoint,
        // exception edge or Java call is hidden inside this batch.
        const int remaining=r4::state.active?r4::state.available(use_thread,r4_quantum):std::min(1024-(guard&1023),
            (VMS_EXEC(g_slice_ms)<=4?250000:1000000)-guard+1);
        // P6.2 T2: a compiled region has execute_fast_batch's exact contract (same budget, same stop points);
        // the op count it returns is charged to guard below, so slice length and game pacing are unchanged.
        const bool region_dispatch = !ProfileMethods && (!r4::state.active||r4::state.cfg.profile=="optimized-v1") &&
            VMS_NATIVE(g_native_jit).enabled && VMS_NATIVE(g_native_jit).available;
        bool overlap_deferred=false;
        if (region_dispatch && !jitr_skipped(fr)) {   // P6.8: retired pcs skip the table lookup
            const int compiled=jit_region_try(fr,remaining,&overlap_deferred);
            if(compiled){guard+=compiled-1;if(r4::state.active){r4::state.charge(compiled,use_thread);r4_quantum-=compiled;r4::state.jit_ops+=compiled;}continue;}
        }
        // Only a deferred, covered interior needs a handoff to its loop header.
        // Other failures retain batching: some valid Java loops carry values
        // the integer region cannot represent, or repeatedly fail a helper.
        const int fast=(!r4::state.active||r4::state.cfg.profile=="optimized-v1")?
            (overlap_deferred?execute_fast_batch<true>(fr,remaining):execute_fast_batch(fr,remaining)):0;
        if(fast) {guard+=fast-1;VMS_EXEC(g_fast_ops)+=uint64_t(fast);++VMS_EXEC(g_fast_blocks);if(r4::state.active){r4::state.charge(fast,use_thread);r4_quantum-=fast;r4::state.fast_ops+=fast;}continue;}
#endif
        if(r4::state.active){r4::state.charge(1,use_thread);--r4_quantum;}
        ++VMS_EXEC(g_slow_ops);
        const auto& code = fr.method->code;
        const Class& owner = VMS_CL(g_classes)[fr.method->class_id];
        if (fr.pc < 0 || fr.pc >= (int)code.size()) die("bad pc in " + owner.name + "." + fr.method->name);
        fr.op_pc = fr.pc;
        if (fr.method->quick.size() < code.size()) fr.method->quick.resize(code.size());
        uint8_t op = code[fr.pc++]; P61_OP(fr.method, fr.op_pc, op | 0x100u);
        auto u8 = [&]() { return code[fr.pc++]; };
        auto u16 = [&]() { uint16_t v = ru16(&code[fr.pc]); fr.pc += 2; return v; };
        auto s16 = [&]() { int v = (int)rs16(&code[fr.pc]); fr.pc += 2; return v; };
        auto push = [&](Value v) { fr.stack.push_back(v); };
        auto pop = [&]() {
            if (fr.stack.empty()) { underflow_log("pop", fr, 1, std::string()); die("stack underflow in " + owner.name + "." + fr.method->name); }
            Value v = fr.stack.back(); fr.stack.pop_back(); return v;
        };
        auto branch = [&](int off) { fr.pc = fr.op_pc + off; };
        auto ldc = [&](int idx) {
            const CpEntry& e = owner.cp[idx];
            if (e.tag == 3 || e.tag == 4) push(Value::of_i(e.a));
            else if (e.tag == 8) push(Value::of_ref(literal_string(cp_utf(owner, e.a))));
            else if (e.tag == 7) {
                std::string name = cp_class(owner, idx);
                if (name[0] != '[') ensure_class(name);
                push(Value::of_ref(class_object(name)));
            } else die("ldc tag " + std::to_string(e.tag));
        };
        auto do_invoke = [&](int idx, bool is_static, bool is_interface, bool virtual_dispatch) {
            if (is_interface) { u8(); u8(); }
            QuickSite& site = fr.method->quick[fr.op_pc];
            int argc = site.kind ? (int)site.argc : -1;
            bool is_void = site.flags & 1;
            std::string ocn, mn, desc;
            if (argc < 0) {
                resolve_member(owner, idx, ocn, mn, desc);
                argc = count_args(desc);
                is_void = returns_void(desc);
            }
            int n = argc + (is_static ? 0 : 1);
            if ((int)fr.stack.size() < n) {
                std::string ucn, umn, udesc; resolve_member(owner, idx, ucn, umn, udesc);   // error path only
                underflow_log("invoke", fr, n, ucn + "." + umn + udesc);
                die("invoke underflow " + owner.name + "." + fr.method->name + " argc=" + std::to_string(argc));
            }
            Value tmp[16];
            std::vector<Value> extra;
            Value* a = tmp;
            if (n > 16) { extra.resize(n); a = extra.data(); }
            for (int i = n - 1; i >= 0; --i) a[i] = pop();
            if (!is_static && a[0].ref <= 0) {
                if (mn.empty()) resolve_member(owner, idx, ocn, mn, desc);
                throw_new("java/lang/NullPointerException", ocn + "." + mn + " from " + owner.name + "." + fr.method->name);
                return;
            }
            Method* mm = nullptr;
            int runtime = -1;
            if (!virtual_dispatch && site.kind == 3) mm = method_at(((uint32_t)site.a << 16) | (uint32_t)site.b);
            if (virtual_dispatch && a[0].ref > 0) {
                runtime = VMS_HEAP(g_heap)[a[0].ref].cls;
                if (site.kind == 4 && site.vclass == runtime) mm = method_at(((uint32_t)site.va << 16) | (uint32_t)site.vb);
            }
            if (!mm) {
                if (ocn.empty()) resolve_member(owner, idx, ocn, mn, desc);
                int start = ensure_class(!is_static && !ocn.empty() && ocn[0]=='[' ? "java/lang/Object" : ocn);
                if (virtual_dispatch && runtime >= 0) start = runtime;
                mm = find_method(start, mn, desc, true);
                if (!mm) die("missing method " + ocn + "." + mn + desc);
                is_void = returns_void(desc);
                int mi = (int)(mm - &VMS_CL(g_classes)[mm->class_id].methods[0]);
                site.argc = (uint8_t)argc;
                site.flags = (is_void ? 1 : 0) | (uint8_t(stream_intrinsic_kind(mm)) << 1);
                site.intrinsic = (uint8_t)graphics_intrinsic(mm);
                site.library_intrinsic = (uint8_t)library_intrinsic_kind(mm);
                if (virtual_dispatch) {
                    site.kind = 4; site.vclass = runtime; site.va = mm->class_id; site.vb = mi;
                } else {
                    site.kind = 3; site.a = mm->class_id; site.b = mi;
                }
            }
            auto retry_after_init = [&](int cid) -> bool {
                if(initialize(cid))return false;
                if(!VMS_EXEC(g_thrown)) {
                    auto& caller=(*st)[frame_index];caller.pc=caller.op_pc;
                    for(int i=0;i<n;++i)caller.stack.push_back(a[i]);
                }
                return true;
            };
            if(is_static && retry_after_init(mm->class_id))return;
            if constexpr(!ProfileMethods) {
#if J2ME_OPT_FAST_DISPATCH
                if(!is_static && (!r4::state.active||r4::state.cfg.profile=="optimized-v1")) {
                    const unsigned budget=r4::state.active?unsigned(r4::state.available(use_thread,r4_quantum)):unsigned(std::max(0,std::min(1023-(guard&1023),
                        (VMS_EXEC(g_slice_ms)<=4?250000:1000000)-guard)));
                    Value leaf_result;unsigned executed=0;
                    if(execute_read_leaf(mm,a,n,budget,leaf_result,executed)) {
                        guard+=int(executed);if(r4::state.active){r4::state.charge(executed,use_thread);r4_quantum-=int(executed);r4::state.fast_ops+=executed;}
                        VMS_EXEC(g_fast_ops)+=executed;++VMS_EXEC(g_fast_blocks);push(leaf_result);return;
                    }
                }
#endif
                if(is_static&&VMS_NATIVE(g_native_jit).enabled&&(!r4::state.active||r4::state.cfg.profile=="optimized-v1")){
                    const unsigned budget=r4::state.active?unsigned(r4::state.available(use_thread,r4_quantum)):unsigned(std::max(0,std::min(1023-(guard&1023),
                        (VMS_EXEC(g_slice_ms)<=4?250000:1000000)-guard)));
                    Value compiled_result;unsigned executed=0;
                    if(jit_try_call(mm,a,n,budget,compiled_result,executed)){
                        guard+=int(executed);if(r4::state.active){r4::state.charge(executed,use_thread);r4_quantum-=int(executed);r4::state.jit_ops+=executed;}push(compiled_result);return;
                    }
                }
            }
            if(mm->native && VMS_CL(g_classes)[mm->class_id].name=="java/lang/Class") {
                if(mm->name=="forName") {
                    if(!a[0].ref){throw_new("java/lang/NullPointerException","forName");return;}
                    std::string name=read_string(a[0].ref);for(char& c:name)if(c=='.')c='/';
                    int id=load_class_file(name,false);
                    if(id<0){throw_new("java/lang/ClassNotFoundException",name);return;}
                    if(retry_after_init(id))return;
                } else if(mm->name=="newInstance") {
                    int id=ensure_class(VMS_HEAP(g_heap)[a[0].ref].text);
                    if(retry_after_init(id))return;
                    Method* ctor=find_method(id,"<init>","()V",false);
                    if(!ctor || (VMS_CL(g_classes)[id].flags & 0x0600)) {throw_new("java/lang/InstantiationException",VMS_CL(g_classes)[id].name);return;}
                    if(!(ctor->flags & 1)){throw_new("java/lang/IllegalAccessException",VMS_CL(g_classes)[id].name);return;}
                    int obj=alloc_obj(id);Value value=Value::of_ref(obj);
                    push_frame(ctor,&value,1);st->back().return_object=true;st->back().constructed=value;return;
                }
            }
            const bool profiled_native = mm->native || (site.flags >> 1) || site.intrinsic || site.library_intrinsic;
            ExecutionProfile::Context native_profile(VMS_EXEC(g_execution_profile),&mm->cpu_profile,ProfileMethods && profiled_native);
            if constexpr(ProfileMethods)if(mm->native)VMS_EXEC(g_execution_profile).called(mm->cpu_profile);
            unsigned r4_cost=0;Value r4_expected;
            bool shortcut_allowed=!r4::state.active;
            if(r4::state.active&&((site.flags>>1)||site.library_intrinsic)&&r4::state.cfg.profile=="optimized-v1") {
                // Synchronized library calls retain their complete Java
                // frame/monitor and exact-fuel path in deterministic mode.
                shortcut_allowed=!(mm->flags&0x0020)&&r4_shortcut_cost(mm,a,n,unsigned(r4::state.available(use_thread,r4_quantum)),r4_cost,r4_expected);
                if(!shortcut_allowed)++r4::state.uncertified_fallbacks;
            }
            auto charge_shortcut=[&](const Value& value){
                if(!r4::state.active)return;
                if(!is_void&&(value.i!=r4_expected.i||value.ref!=r4_expected.ref||value.l!=r4_expected.l||value.is_long!=r4_expected.is_long))r4_abort("SHORTCUT_RESULT_MISMATCH");
                r4::state.charge(r4_cost,use_thread);r4_quantum-=int(r4_cost);++r4::state.certified_shortcuts;
            };
            if (shortcut_allowed && (site.flags >> 1)) {
                Value result;
                if (stream_intrinsic_dispatch(site.flags >> 1, mm, a, result)) {
                    charge_shortcut(result);
                    // A rejected Java stream intrinsic enters push_frame below;
                    // count it there, not twice as probe + actual invocation.
                    if constexpr(ProfileMethods)if(!mm->native)VMS_EXEC(g_execution_profile).called(mm->cpu_profile);
                    if (!VMS_EXEC(g_thrown) && !is_void) push(result);
                    return;
                }
            }
            if (shortcut_allowed && site.library_intrinsic) {
                const unsigned original_ops=library_intrinsic_ops(site.library_intrinsic);
                const unsigned budget=unsigned(std::max(0,std::min(1023-(guard&1023),
                    (VMS_EXEC(g_slice_ms)<=4?250000:1000000)-guard)));
                Value result;
                if (original_ops<=budget && library_intrinsic_dispatch(site.library_intrinsic, mm, a, result)) {
                    guard+=int(original_ops);VMS_EXEC(g_fast_ops)+=original_ops;
                    if(original_ops)++VMS_EXEC(g_fast_blocks);
                    charge_shortcut(result);
                    if constexpr(ProfileMethods)if(!mm->native)VMS_EXEC(g_execution_profile).called(mm->cpu_profile);
                    if (!VMS_EXEC(g_thrown) && !is_void) push(result);
                    return;
                }
            }
            if (site.intrinsic) {
                if(site.intrinsic>=10&&site.intrinsic<=16)push(Value::of_i(gfx_query_intrinsic(site.intrinsic,a[0].ref,mm)));
                else gfx_intrinsic(site.intrinsic, a);
                return;
            }
            if (mm->native && !(mm->flags & 0x0020) &&
                VMS_CL(g_classes)[mm->class_id].name == "java/lang/System" &&
                mm->name == "currentTimeMillis" && mm->desc == "()J") {
                ++VMS_EXEC(g_poll_calls);
                if (site.time_gate == -1) {
                    auto constant = [&](int cp) -> int64_t {
                        return cp > 0 && cp < int(owner.cp.size()) && owner.cp[cp].tag == 5 ? owner.cp[cp].l : 0;
                    };
                    auto is_yield = [&](int cp) {
                        if (cp <= 0 || cp >= int(owner.cp.size()) || owner.cp[cp].tag != 10) return false;
                        std::string cls, name, desc;if(!pacing_resolve_member(owner,cp,cls,name,desc))return false;
                        return cls == "java/lang/Thread" && name == "yield" && desc == "()V";
                    };
                    auto is_clock = [&](int cp) {
                        if(cp<=0 || cp>=int(owner.cp.size()) || owner.cp[cp].tag!=10)return false;
                        std::string cls,name,desc;if(!pacing_resolve_member(owner,cp,cls,name,desc))return false;
                        return cls=="java/lang/System" && name=="currentTimeMillis" && desc=="()J";
                    };
                    auto is_long_static = [&](int cp) {
                        if(cp<=0 || cp>=int(owner.cp.size()) || owner.cp[cp].tag!=9)return false;
                        std::string cls,name,desc;if(!pacing_resolve_member(owner,cp,cls,name,desc))return false;return desc=="J";
                    };
                    site.bounded_deadline=1;
                    if(fr.method->desc=="(J)V" && (fr.method->flags&0x0528)==0x0008 &&
                       fr.method->handlers.empty()&&recognize_bounded_deadline_helper(code,is_clock,is_yield)) {
                        if(fr.op_pc==1&&fr.pc==4)site.bounded_deadline=2;
                        else if(fr.op_pc==7&&fr.pc==10)site.bounded_deadline=3;
                    }
                    auto gate = recognize_time_gate(code, fr.op_pc, fr.pc, constant, is_yield);
                    if(gate.local<0)gate=recognize_field_time_gate(code,fr.op_pc,fr.pc,constant,is_yield,is_clock,
                        is_long_static,[&](int pc){return jvm_op_length(code,pc);});
                    if(gate.local<0&&gate.static_field<0)gate=recognize_aliased_field_time_gate(code,fr.op_pc,fr.pc,constant,is_yield,is_clock,
                        [&](int cp){const auto* field=pacing_loaded_static_long(owner,cp);return field&&!(field->flags&0x0040);},
                        [&](int a,int b)->int {
                            std::string c,n,d;
                            if(a<=0||a>=int(owner.cp.size())||owner.cp[a].tag!=9||!pacing_resolve_member(owner,a,c,n,d))return -1;
                            if(d!="J")return 0; // A non-long write cannot alias the long anchor.
                            const auto* left=pacing_loaded_static_long(owner,a);
                            const auto* right=pacing_loaded_static_long(owner,b);
                            if(!left||!right)return -1;
                            return left->owner==right->owner&&left->slot==right->slot?1:0;
                        },[&](int pc){return jvm_op_length(code,pc);},[&](int pc){
                            for(const auto& handler:fr.method->handlers)if(handler.pc==pc)return true;return false;
                        });
                    site.time_gate = 0;
                    if (gate.local >= 0 || gate.static_field >= 0) {
                        fr.method->time_gates.push_back(gate);
                        site.time_gate = int(fr.method->time_gates.size());
                    } else {
                        const int local=recognize_native_deadline_gate(code,fr.op_pc,fr.pc,
                            [&](int cp){return pacing_loaded_static_integral(owner,cp)!=nullptr;},is_yield,is_clock,
                            [&](int start,int end){return native_deadline_flow_safe(code,start,end,
                                !fr.method->handlers.empty(),[&](int pc){return jvm_op_length(code,pc);});});
                        // Separate cache domain: this must neither certify a
                        // paced group nor ban JIT through Method::time_gates.
                        if(local>=0)site.time_gate=-2-local;
                    }
                }
                int64_t now = game_current_ms();
                if(site.cooperative_poll!=0)cooperative_poll_clock(fr,now,site,use_thread,slice_deadline);
                auto& context = current_context();
                speed_compensated_clock(fr,now);
                if(site.event_frame_sleep!=0)speed_event_frame_clock(fr,now,site);
                if(site.rechecked_deadline!=0)speed_rechecked_clock(fr,now,site);
                if(site.cached_elapsed!=0)speed_cached_elapsed_clock(fr,now,site);
                if(speed_period_us()&&!r4::state.active&&!VMS_RS(g_paint_depth)&&VMS_EXEC(g_can_suspend)&&
                   !fr.locals.empty()&&fr.locals[0].ref==VMS_RS(g_current)&&VMS_RS(g_current)==VMS_RS(g_shown)&&
                   site.residual_sleep!=1)
                    site.residual_sleep=speed_residual_clock(fr,now)?2:1;
                if(!site.frame_wait&&!speed_frame_wait_clock(fr,now))site.frame_wait=1;
                if(speed_period_us() && VMS_RS(g_paint_depth)==0) {
                    int stored=field_clock_store(code,fr.pc);
                    if(stored<0)for(const auto& gate:fr.method->time_gates)
                        if(gate.static_field>=0&&gate.anchor_clock_pc==fr.op_pc){stored=gate.static_field;break;}
                    if(stored>=0)context.field_gate_anchor.record(fr.method,fr.op_pc,stored,now,paint_clock_us(),context.owned_draws);
                }
                if(!r4::state.active&&VMS_EXEC(g_can_suspend)&&VMS_RS(g_paint_depth)==0&&
                   (VMS_EXEC(g_speed_cap)==0||site.bounded_deadline>1)) {
                    auto& evidence=context.native_deadline;
                    const int stored=VMS_EXEC(g_speed_cap)==0?clock_long_local_store(code,fr.pc):-1;
                    if(stored>=0) {
                        if(!fr.native_deadline_activation&&context.native_deadline_generation<UINT64_MAX)
                            fr.native_deadline_activation=++context.native_deadline_generation;
                        if(VMS_RS(g_current)==VMS_RS(g_shown))
                            evidence.record(fr.native_deadline_activation,stored,now,VMS_RS(g_current),
                                            context.owned_draws,VMS_EXEC(g_game_clock).scale);
                        else evidence.reset();
                    }
                    if(site.bounded_deadline==2) {
                        const bool values=!fr.locals.empty()&&fr.locals[0].is_long&&
                            fr.stack.size()==1&&fr.stack.back().is_long&&fr.stack.back().l==fr.locals[0].l;
                        if(!fr.native_deadline_activation&&context.native_deadline_generation<UINT64_MAX)
                            fr.native_deadline_activation=++context.native_deadline_generation;
                        if(values&&VMS_RS(g_current)==VMS_RS(g_shown))
                            evidence.record_bounded_delay(fr.native_deadline_activation,fr.locals[0].l,now,
                                VMS_RS(g_current),context.owned_draws,VMS_EXEC(g_game_clock).scale);
                        else evidence.reset();
                    }
                    const bool bounded=site.bounded_deadline==3;
                    if((VMS_EXEC(g_speed_cap)==0&&site.time_gate<=-2)||bounded) {
                        const int local=bounded?0:-2-site.time_gate;
                        const bool values=local<int(fr.locals.size())&&fr.locals[local].is_long&&
                            fr.stack.size()==1&&fr.stack.back().is_long;
                        const int64_t anchor=bounded?evidence.value:values?fr.locals[local].l:0;
                        const int64_t target=values?fr.stack.back().l:0;
                        const bool owned=values&&VMS_RS(g_current)==VMS_RS(g_shown)&&
                            (bounded?(fr.locals[local].l==target&&evidence.owns_bounded_delay(fr.native_deadline_activation,
                                target,VMS_RS(g_current),context.owned_draws,VMS_EXEC(g_game_clock).scale,now)):
                                evidence.owns(fr.native_deadline_activation,local,anchor,VMS_RS(g_current),
                                    context.owned_draws,VMS_EXEC(g_game_clock).scale));
                        if(!owned) {if(bounded)evidence.reset();else evidence.poll.reset();}
                        else {
                            if(bounded)evidence.last_ms=now;
                            ++VMS_EXEC(g_poll_gate_reads);
                            evidence.poll.observe_deadline(fr.method,fr.op_pc,anchor,target,now,true);
                            const int64_t nap=evidence.poll.wait_us(now,VMS_EXEC(g_game_clock).scale,false);
                            bool quiet=nap>0&&!VMS_RS(g_repaint)&&!VMS_RS(g_fb_dirty)&&
                                VMS_EXEC(g_serial_calls).empty()&&!VMS_EXEC(g_notify_paused)&&!VMS_EXEC(g_notify_destroyed);
                            if(quiet) {
                                // Even sleeping/blocked Java peers may soon
                                // become runnable; the new path rejects all.
                                for(const auto& peer:VMS_EXEC(g_threads))
                                    if(&peer!=&context&&peer.state!=ThreadState::Terminated){quiet=false;break;}
                                const auto& main=VMS_EXEC(g_main_thread);
                                if(&main!=&context&&!main.frames.empty()&&main.state!=ThreadState::Terminated)quiet=false;
                                for(const auto& monitor:VMS_EXEC(g_monitors))
                                    if(monitor.second.owner==&context&&monitor.second.depth>0){quiet=false;break;}
                            }
                            const int64_t host_left=VMS_EXEC(g_exec_ctx).native_poll_host_deadline_us-paint_clock_us();
                            if(quiet&&host_left>2000&&!p7_pending_ends()) {
                                audio_pump();
                                now=game_current_ms();
                                if(bounded) {
                                    if(evidence.owns_bounded_delay(fr.native_deadline_activation,target,VMS_RS(g_current),
                                        context.owned_draws,VMS_EXEC(g_game_clock).scale,now))evidence.last_ms=now;
                                    else evidence.reset();
                                }
                                const int64_t requested=evidence.poll.wait_us(now,VMS_EXEC(g_game_clock).scale,false);
                                const int64_t remaining=VMS_EXEC(g_exec_ctx).native_poll_host_deadline_us-paint_clock_us();
                                if(requested>0&&remaining>2000&&!p7_pending_ends()&&!VMS_RS(g_repaint)&&
                                   !VMS_RS(g_fb_dirty)&&VMS_EXEC(g_serial_calls).empty()) {
                                    const int64_t before=paint_clock_us();
                                    { RuntimeProfileScope wait(VMS_EXEC(g_runtime_profile),RuntimeProfile::Kind::PollWait);
                                      P7_SCOPE(Idle);std::this_thread::sleep_for(std::chrono::microseconds(requested)); }
                                    VMS_EXEC(g_poll_wait_us)+=paint_clock_us()-before;++VMS_EXEC(g_poll_waits);
                                    now=game_current_ms();
                                    if(bounded) {
                                        if(evidence.owns_bounded_delay(fr.native_deadline_activation,target,VMS_RS(g_current),
                                            context.owned_draws,VMS_EXEC(g_game_clock).scale,now))evidence.last_ms=now;
                                        else evidence.reset();
                                    }
                                }
                            }
                        }
                    }
                } else if(site.time_gate<=-2||site.bounded_deadline>1)context.native_deadline.reset();
                if (!r4::state.active && site.time_gate > 0 && VMS_RS(g_paint_depth) == 0) {
                    const auto gate = fr.method->time_gates[site.time_gate-1];
                    const int gate_pc=gate.site_pc>=0?gate.site_pc:fr.op_pc;
                    bool valid=gate.local>=0 && gate.local<int(fr.locals.size()) && fr.locals[gate.local].is_long;
                    int64_t previous=valid?fr.locals[gate.local].l:0;
                    if(gate.static_field>=0) {
                        // The field must still contain this thread's own
                        // anchor, followed by its completed drawing. Merely
                        // reading another thread's timestamp is not proof.
                        auto& field_site=fr.method->quick[fr.pc];
                        if(field_site.kind==1) {
                            const auto value=VMS_CL(g_classes)[field_site.a].statics[field_site.b];
                            previous=value.l;valid=value.is_long;
                        } else {
                            std::string cls,name,desc;resolve_member(owner,gate.static_field,cls,name,desc);
                            auto found=VMS_CL(g_class_index).find(cls);
                            Field* field=found==VMS_CL(g_class_index).end()?nullptr:find_field(found->second,name,desc);
                            valid=field && field->is_static && field->desc=="J";
                            if(valid)previous=VMS_CL(g_classes)[field->owner].statics[field->slot].l;
                        }
                        valid=valid && context.field_gate_anchor.owns(fr.method,gate,previous,paint_clock_us(),context.owned_draws);
                        if(!valid && context.auto_fps.verified_gate_seen && context.auto_fps.gate_method==fr.method &&
                           context.auto_fps.gate_pc==gate_pc)speed_invalidate_group(context);
                    }
                    if (valid) {
                        ++VMS_EXEC(g_poll_gate_reads);
                        const bool same_site=context.auto_fps.gate_method==fr.method && context.auto_fps.gate_pc==gate_pc;
                        if(context.auto_fps.verified_gate_seen && !same_site)speed_invalidate_group(context);
                        const bool closed_group=context.auto_fps.helper_drawing() && !context.auto_fps.group_complete && same_site;
                        if(closed_group) {
                            context.auto_fps.group_complete=true;
                            // The final repaint can be asynchronous. Finish it
                            // before opening the next logical gate group.
                            if(VMS_RS(g_repaint) && VMS_EXEC(g_repaint_owner_ref)==context.ref) {
                                VMS_RS(g_repaint)=false;repaint_current();
                            }
                            if(previous==context.auto_fps.gate_previous)context.auto_fps.group_open=false;
                            if(VMS_EXEC(g_can_suspend)) {VMS_EXEC(g_speed_frame_open)=true;VMS_EXEC(g_yield)=true;}
                        }
                        if (context.poll_gate.observe(fr.method, gate_pc, previous, gate.millis, now,
                                                      context.owned_draws > 0) && speed_period_us())
                            speed_certified_gate(context,fr.method,gate_pc,previous,gate.millis);
                        const bool paced_verified = speed_period_us() && &context==VMS_EXEC(g_speed_budget_owner) &&
                            context.auto_fps.verified_gate_seen && context.auto_fps.stable>=5 && same_site;
                        const int64_t nap = context.poll_gate.wait_us(now, VMS_EXEC(g_game_clock).scale, J2ME_OPT_POLL_MARGIN && paced_verified);
                        bool peer = false, locked = false;
                        if (nap > 0 && VMS_EXEC(g_can_suspend)) {
                            for (const auto& t : VMS_EXEC(g_threads))
                                if (&t != &context && t.state == ThreadState::Runnable && !t.executing) {peer=true;break;}
                            for (const auto& monitor : VMS_EXEC(g_monitors))
                                if (monitor.second.owner == &context && monitor.second.depth > 0) {locked=true;break;}
                            if (!peer && !locked) {
                                audio_pump();
                                const int64_t before = paint_clock_us();
                                { RuntimeProfileScope wait(VMS_EXEC(g_runtime_profile),RuntimeProfile::Kind::PollWait);
                                  P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::microseconds(nap)); }
                                const int64_t observed=paint_clock_us()-before;
                                if(J2ME_OPT_POLL_MARGIN && paced_verified && context.poll_gate.certified)
                                    context.poll_gate.note_wait(nap,observed);
                                VMS_EXEC(g_poll_wait_us) += observed; ++VMS_EXEC(g_poll_waits);
                                now = game_current_ms();
                                // Host oversleep can cross the gate. The value
                                // returned to Java must register that ready
                                // cycle too, instead of borrowing its old group.
                                if(now>=context.poll_gate.deadline_ms
                                   && context.poll_gate.observe(fr.method,gate_pc,previous,gate.millis,now,context.owned_draws>0)
                                   && speed_period_us())
                                    speed_certified_gate(context,fr.method,gate_pc,previous,gate.millis);
                            }
                        }
                    }
                }
                push(Value::of_l(now));
                return;
            }
            if (mm->native) {
                if(service_repaints_wait_event(mm,n?a[0].ref:0)){
                    // No native/paint work has begun. Preserve this exact
                    // invocation and receiver until the event callback returns.
                    for(int i=0;i<n;++i)push(a[i]);
                    fr.pc=fr.op_pc;VMS_EXEC(g_yield)=true;return;
                }
                const int paint_monitor=service_repaints_wait_monitor(mm,n?a[0].ref:0);
                if(paint_monitor && !monitor_enter(paint_monitor)){
                    // Exactly the existing synchronized-native retry contract:
                    // retain Java args and invocation BCI before yielding.
                    S0_HOOK(++s0.s4_r02_retries);
                    for(int i=0;i<n;++i)push(a[i]);
                    fr.pc=fr.op_pc;suspend_current();return;
                }
                Frame nf; nf.method = mm;
#if J2ME_OPT_FRAME_POOL
                VMS_HEAP(g_frame_buffer_pool).acquire(nf.locals,size_t(n));
                VMS_HEAP(g_frame_buffer_pool).acquire(nf.stack,is_void?0:1);
#endif
                if(n)nf.locals.assign(a, a + n); nf.op_pc = fr.op_pc;
                int native_lock = 0;
                if (mm->flags & 0x0020) {
                    native_lock = (mm->flags & 0x0008) ? class_object(VMS_CL(g_classes)[mm->class_id].name) : a[0].ref;
                    if (!monitor_enter(native_lock)) {
                        for (int i = 0; i < n; ++i) push(a[i]);
                        fr.pc = fr.op_pc; suspend_current(); return;
                    }
                }
                bool previous_suspend=VMS_EXEC(g_can_suspend);
                if(native_lock)VMS_EXEC(g_can_suspend)=false;
                call_native(mm, nf);
                VMS_EXEC(g_can_suspend)=previous_suspend;
                if (native_lock) monitor_exit(native_lock);
                if (current_context().unhandled) { int ref = current_context().unhandled; current_context().unhandled = 0; throw_ref(ref); }
                if (VMS_EXEC(g_thrown)) return;
                if (!is_void && !nf.stack.empty()) push(nf.stack.back());
            } else push_frame(mm, a, n);
        };
        if (op == 196) {
            op = u8();
            int idx = u16();
            if (op == 132) { int d = s16(); fr.locals[idx].i = int_bits((uint32_t)fr.locals[idx].i + (uint32_t)d); }
            else if (op == 21 || op == 22 || op == 23 || op == 24 || op == 25) push(fr.locals[idx]);
            else if (op == 54 || op == 55 || op == 56 || op == 57 || op == 58) fr.locals[idx] = pop();
            else if (op == 169) fr.pc = fr.locals[idx].i;
            else die("wide opcode " + std::to_string(op));
            continue;
        }
        switch (op) {
        case 0: break;
        case 1: push(Value::of_ref(0)); break;
        case 2: case 3: case 4: case 5: case 6: case 7: case 8: push(Value::of_i(op - 3)); break;
        case 9: case 10: push(Value::of_l(op - 9)); break;
        case 11: push(Value::of_i(f_bits(0))); break;
        case 12: push(Value::of_i(f_bits(1))); break;
        case 13: push(Value::of_i(f_bits(2))); break;
        case 14: push(Value::of_l(d_bits(0))); break;
        case 15: push(Value::of_l(d_bits(1))); break;
        case 16: push(Value::of_i((int8_t)u8())); break;
        case 17: push(Value::of_i(s16())); break;
        case 18: ldc(u8()); break;
        case 19: ldc(u16()); break;
        case 20: {
            int idx = u16();
            const CpEntry& e = owner.cp[idx];
            push(e.tag == 5 || e.tag == 6 ? Value::of_l(e.l) : Value::of_l((int64_t)e.a));
            break;
        }
        case 21: case 22: case 23: case 24: case 25: push(fr.locals[u8()]); break;
        case 26: case 27: case 28: case 29: push(fr.locals[op - 26]); break;
        case 30: case 31: case 32: case 33: push(fr.locals[op - 30]); break;
        case 34: case 35: case 36: case 37: push(fr.locals[op - 34]); break;
        case 38: case 39: case 40: case 41: push(fr.locals[op - 38]); break;
        case 42: case 43: case 44: case 45: push(fr.locals[op - 42]); break;
        case 46: case 47: case 48: case 49: case 50: case 51: case 52: case 53: {
            int idx = pop().i, ref = pop().ref;
            if (!ref) { throw_new("java/lang/NullPointerException", "aload"); break; }
            if (idx < 0 || idx >= (int)VMS_HEAP(g_heap)[ref].elems.size()) { throw_new("java/lang/ArrayIndexOutOfBoundsException", "aload"); break; }
            push(VMS_HEAP(g_heap)[ref].elems.get(idx));
            break;
        }
        case 54: case 55: case 56: case 57: case 58: fr.locals[u8()] = pop(); break;
        case 59: case 60: case 61: case 62: fr.locals[op - 59] = pop(); break;
        case 63: case 64: case 65: case 66: fr.locals[op - 63] = pop(); break;
        case 67: case 68: case 69: case 70: fr.locals[op - 67] = pop(); break;
        case 71: case 72: case 73: case 74: fr.locals[op - 71] = pop(); break;
        case 75: case 76: case 77: case 78: fr.locals[op - 75] = pop(); break;
        case 79: case 80: case 81: case 82: case 83: case 84: case 85: case 86: {
            Value v = pop(); int idx = pop().i, ref = pop().ref;
            if (!ref) { throw_new("java/lang/NullPointerException", "astore"); break; }
            if (idx < 0 || idx >= (int)VMS_HEAP(g_heap)[ref].elems.size()) { throw_new("java/lang/ArrayIndexOutOfBoundsException", "astore"); break; }
            if (op == 83 && !type_compatible(v.ref, component_type(VMS_HEAP(g_heap)[ref].array_desc))) { throw_new("java/lang/ArrayStoreException", "aastore"); break; }
            if (op == 84) v.i = VMS_HEAP(g_heap)[ref].array_desc == "[Z" ? (v.i & 1) : (int8_t)v.i;
            else if (op == 85) v.i = (uint16_t)v.i;
            else if (op == 86) v.i = (int16_t)v.i;
            VMS_HEAP(g_heap)[ref].elems.set(idx, v);
            break;
        }
        case 87: pop(); break;
        case 88: { Value a = pop(); if (!a.is_long) pop(); break; }
        case 89: { Value v = pop(); push(v); push(v); break; }
        case 90: { Value v = pop(); Value w = pop(); push(v); push(w); push(v); break; }
        case 91: { Value v = pop(), w = pop(); if (w.is_long) { push(v); push(w); push(v); } else { Value u = pop(); push(v); push(u); push(w); push(v); } break; }
        case 92: { Value a = pop(); if (a.is_long) { push(a); push(a); } else { Value b = pop(); push(b); push(a); push(b); push(a); } break; }
        case 93: {
            Value v1 = pop();
            if (v1.is_long) { Value v2 = pop(); push(v1); push(v2); push(v1); }
            else { Value v2 = pop(); Value v3 = pop(); push(v2); push(v1); push(v3); push(v2); push(v1); }
            break;
        }
        case 94: {
            Value v1 = pop();
            if (v1.is_long) {
                Value v2 = pop();
                if (v2.is_long) { push(v1); push(v2); push(v1); }
                else { Value v3 = pop(); push(v1); push(v3); push(v2); push(v1); }
            } else {
                Value v2 = pop(), v3 = pop();
                if (v3.is_long) { push(v2); push(v1); push(v3); push(v2); push(v1); }
                else { Value v4 = pop(); push(v2); push(v1); push(v4); push(v3); push(v2); push(v1); }
            }
            break;
        }
        case 95: { Value a = pop(); Value b = pop(); push(a); push(b); break; }
        case 96: { int b = pop().i, a = pop().i; push(Value::of_i(int_bits((uint32_t)a + (uint32_t)b))); break; }
        case 97: { int64_t b = as_long(pop()), a = as_long(pop()); push(Value::of_l(long_bits((uint64_t)a + (uint64_t)b))); break; }
        case 98: { float b = bits_f(pop().i), a = bits_f(pop().i); push(Value::of_i(f_bits(a + b))); break; }
        case 99: { double b = bits_d(as_long(pop())), a = bits_d(as_long(pop())); push(Value::of_l(d_bits(a + b))); break; }
        case 100: { int b = pop().i, a = pop().i; push(Value::of_i(int_bits((uint32_t)a - (uint32_t)b))); break; }
        case 101: { int64_t b = as_long(pop()), a = as_long(pop()); push(Value::of_l(speed_compensated_subtract(fr,a,b,long_bits((uint64_t)a - (uint64_t)b)))); break; }
        case 102: { float b = bits_f(pop().i), a = bits_f(pop().i); push(Value::of_i(f_bits(a - b))); break; }
        case 103: { double b = bits_d(as_long(pop())), a = bits_d(as_long(pop())); push(Value::of_l(d_bits(a - b))); break; }
        case 104: { int b = pop().i, a = pop().i; push(Value::of_i(int_bits((uint32_t)a * (uint32_t)b))); break; }
        case 105: { int64_t b = as_long(pop()), a = as_long(pop()); push(Value::of_l(long_bits((uint64_t)a * (uint64_t)b))); break; }
        case 106: { float b = bits_f(pop().i), a = bits_f(pop().i); push(Value::of_i(f_bits(a * b))); break; }
        case 107: { double b = bits_d(as_long(pop())), a = bits_d(as_long(pop())); push(Value::of_l(d_bits(a * b))); break; }
        case 108: { int b = pop().i; if (!b) { throw_new("java/lang/ArithmeticException", "div0"); break; } int a = pop().i; push(Value::of_i(a == INT32_MIN && b == -1 ? INT32_MIN : a / b)); break; }
        case 109: { int64_t b = as_long(pop()); if (!b) { throw_new("java/lang/ArithmeticException", "ldiv0"); break; } int64_t a = as_long(pop()); push(Value::of_l(a == INT64_MIN && b == -1 ? INT64_MIN : a / b)); break; }
        case 110: { float b = bits_f(pop().i); push(Value::of_i(f_bits(bits_f(pop().i) / b))); break; }
        case 111: { double b = bits_d(as_long(pop())); push(Value::of_l(d_bits(bits_d(as_long(pop())) / b))); break; }
        case 112: { int b = pop().i; if (!b) { throw_new("java/lang/ArithmeticException", "rem0"); break; } int a = pop().i; push(Value::of_i(a == INT32_MIN && b == -1 ? 0 : a % b)); break; }
        case 113: { int64_t b = as_long(pop()); if (!b) { throw_new("java/lang/ArithmeticException", "lrem0"); break; } int64_t a = as_long(pop()); push(Value::of_l(a == INT64_MIN && b == -1 ? 0 : a % b)); break; }
        case 114: { float b = bits_f(pop().i), a = bits_f(pop().i); push(Value::of_i(f_bits(std::fmod(a, b)))); break; }
        case 115: { double b = bits_d(as_long(pop())), a = bits_d(as_long(pop())); push(Value::of_l(d_bits(std::fmod(a, b)))); break; }
        case 116: push(Value::of_i(int_bits(0u - (uint32_t)pop().i))); break;
        case 117: push(Value::of_l(long_bits(0ull - (uint64_t)as_long(pop())))); break;
        case 118: push(Value::of_i(f_bits(-bits_f(pop().i)))); break;
        case 119: push(Value::of_l(d_bits(-bits_d(as_long(pop()))))); break;
        case 120: { int b = pop().i & 31, a = pop().i; push(Value::of_i(int_bits((uint32_t)a << b))); break; }
        case 121: { int b = (int)(as_long(pop()) & 63); push(Value::of_l(long_bits((uint64_t)as_long(pop()) << b))); break; }
        case 122: { int b = pop().i & 31, a = pop().i; push(Value::of_i(a >> b)); break; }
        case 123: { int b = (int)(as_long(pop()) & 63); push(Value::of_l(as_long(pop()) >> b)); break; }
        case 124: { int b = pop().i & 31; uint32_t a = (uint32_t)pop().i; push(Value::of_i((int)(a >> b))); break; }
        case 125: { int b = (int)(as_long(pop()) & 63); uint64_t a = (uint64_t)as_long(pop()); push(Value::of_l((int64_t)(a >> b))); break; }
        case 126: { int b = pop().i, a = pop().i; push(Value::of_i(a & b)); break; }
        case 127: { int64_t b = as_long(pop()), a = as_long(pop()); push(Value::of_l(a & b)); break; }
        case 128: { int b = pop().i, a = pop().i; push(Value::of_i(a | b)); break; }
        case 129: { int64_t b = as_long(pop()), a = as_long(pop()); push(Value::of_l(a | b)); break; }
        case 130: { int b = pop().i, a = pop().i; push(Value::of_i(a ^ b)); break; }
        case 131: { int64_t b = as_long(pop()), a = as_long(pop()); push(Value::of_l(a ^ b)); break; }
        case 132: { int idx = u8(); fr.locals[idx].i = int_bits((uint32_t)fr.locals[idx].i + (uint32_t)(int32_t)(int8_t)u8()); break; }
        case 133: push(Value::of_l(pop().i)); break;
        case 134: push(Value::of_i(f_bits((float)pop().i))); break;
        case 135: push(Value::of_l(d_bits((double)pop().i))); break;
        case 136: push(Value::of_i((int)as_long(pop()))); break;
        case 137: push(Value::of_i(f_bits((float)as_long(pop())))); break;
        case 138: push(Value::of_l(d_bits((double)as_long(pop())))); break;
        case 139: push(Value::of_i(java_fp_integer<int32_t>(bits_f(pop().i)))); break;
        case 140: push(Value::of_l(java_fp_integer<int64_t>(bits_f(pop().i)))); break;
        case 141: push(Value::of_l(d_bits((double)bits_f(pop().i)))); break;
        case 142: push(Value::of_i(java_fp_integer<int32_t>(bits_d(as_long(pop()))))); break;
        case 143: push(Value::of_l(java_fp_integer<int64_t>(bits_d(as_long(pop()))))); break;
        case 144: push(Value::of_i(f_bits((float)bits_d(as_long(pop()))))); break;
        case 145: push(Value::of_i((int8_t)pop().i)); break;
        case 146: push(Value::of_i((uint16_t)pop().i)); break;
        case 147: push(Value::of_i((int16_t)pop().i)); break;
        case 148: { int64_t b = as_long(pop()), a = as_long(pop()); push(Value::of_i(a > b ? 1 : a < b ? -1 : 0)); break; }
        case 149: { float b = bits_f(pop().i), a = bits_f(pop().i); push(Value::of_i(fcmp(a, b, -1))); break; }
        case 150: { float b = bits_f(pop().i), a = bits_f(pop().i); push(Value::of_i(fcmp(a, b, 1))); break; }
        case 151: { double b = bits_d(as_long(pop())), a = bits_d(as_long(pop())); push(Value::of_i(dcmp(a, b, -1))); break; }
        case 152: { double b = bits_d(as_long(pop())), a = bits_d(as_long(pop())); push(Value::of_i(dcmp(a, b, 1))); break; }
        case 153: { int off = s16(); if (pop().i == 0) branch(off); break; }
        case 154: { int off = s16(); if (pop().i != 0) branch(off); break; }
        case 155: { int off = s16(); if (pop().i < 0) branch(off); break; }
        case 156: { int off = s16(); int v=pop().i; if (fr.method->cached_elapsed_gates.empty()?v>=0:speed_cached_elapsed_branch(fr,fr.op_pc,v)) branch(off); break; }
        case 157: { int off = s16(); int v=pop().i; if (fr.method->has_rechecked_deadline)v=speed_rechecked_branch(fr,fr.op_pc,v); if (v > 0) branch(off); break; }
        case 158: { int off = s16(); int v=pop().i; if (fr.method->has_rechecked_deadline)v=speed_rechecked_branch(fr,fr.op_pc,v); if (v <= 0) branch(off); break; }
        case 159: { int off = s16(); int b = pop().i, a = pop().i; if (a == b) branch(off); break; }
        case 160: { int off = s16(); int b = pop().i, a = pop().i; if (a != b) branch(off); break; }
        case 161: { int off = s16(); int b = pop().i, a = pop().i; if (a < b) branch(off); break; }
        case 162: { int off = s16(); int b = pop().i, a = pop().i; if (a >= b) branch(off); break; }
        case 163: { int off = s16(); int b = pop().i, a = pop().i; if (a > b) branch(off); break; }
        case 164: { int off = s16(); int b = pop().i, a = pop().i; if (a <= b) branch(off); break; }
        case 165: { int off = s16(); int b = pop().ref, a = pop().ref; if (a == b) branch(off); break; }
        case 166: { int off = s16(); int b = pop().ref, a = pop().ref; if (a != b) branch(off); break; }
        case 167: branch(s16()); break;
        case 170: {
            int base = fr.op_pc, index = base + 1; while (index % 4) ++index;
            fr.pc = index;
            int def = (int)ru32(&code[fr.pc]); fr.pc += 4;
            int low = (int)ru32(&code[fr.pc]); fr.pc += 4;
            int high = (int)ru32(&code[fr.pc]); fr.pc += 4;
            int key = pop().i, off = def;
            if (key >= low && key <= high) off = (int)ru32(&code[fr.pc + (key - low) * 4]);
            fr.pc = base + off;
            break;
        }
        case 171: {
            int base = fr.op_pc, index = base + 1; while (index % 4) ++index;
            fr.pc = index;
            int def = (int)ru32(&code[fr.pc]); fr.pc += 4;
            int np = (int)ru32(&code[fr.pc]); fr.pc += 4;
            int key = pop().i, off = def;
            for (int i = 0; i < np; ++i) {
                int m = (int)ru32(&code[fr.pc]); fr.pc += 4;
                int o = (int)ru32(&code[fr.pc]); fr.pc += 4;
                if (m == key) { off = o; break; }
            }
            fr.pc = base + off;
            break;
        }
        case 172: case 173: case 174: case 175: case 176: p1_snapshot(current_context(),st->back(),st->size()); result = pop(); complete_class_initialization(st->back(),true); release_frame_monitor(st->back()); st->pop_back(); if (st->empty()) return result; st->back().stack.push_back(result); break;
        case 177: {
            p1_snapshot(current_context(),st->back(),st->size());
            bool object=st->back().return_object;Value value=st->back().constructed;
            complete_class_initialization(st->back(),true);release_frame_monitor(st->back());st->pop_back();
            if(st->empty())return object?value:Value{};
            if(object)st->back().stack.push_back(value);
            break;
        }
        case 178: case 180: {
            int idx = u16();
            QuickSite& site = fr.method->quick[fr.op_pc];
            int cid = site.a, slot = site.b;
            if (site.kind != 1) {
                std::string ocn, fn, desc; resolve_member(owner, idx, ocn, fn, desc);
                cid = ensure_class(ocn);
                Field* f = find_field(cid, fn, desc);
                if (!f) die("missing field " + ocn + "." + fn);
                slot = f->slot; cid = f->owner;
                site.kind = 1; site.a = cid; site.b = slot;
            }
            if(op==178 && !initialize(cid)){if(!VMS_EXEC(g_thrown))(*st)[frame_index].pc=(*st)[frame_index].op_pc;break;}
            if (op == 178) push(VMS_CL(g_classes)[cid].statics[slot]);
            else {
                int ref = pop().ref;
                if (!ref) { throw_new("java/lang/NullPointerException", "getfield"); break; }
                push(VMS_HEAP(g_heap)[ref].fields[slot]);
            }
            break;
        }
        case 179: case 181: {
            int idx = u16();
            QuickSite& site = fr.method->quick[fr.op_pc];
            int cid = site.a, slot = site.b;
            if (site.kind != 1) {
                std::string ocn, fn, desc; resolve_member(owner, idx, ocn, fn, desc);
                cid = ensure_class(ocn);
                Field* f = find_field(cid, fn, desc);
                if (!f) die("missing field " + ocn + "." + fn);
                slot = f->slot; cid = f->owner;
                site.kind = 1; site.a = cid; site.b = slot;
            }
            if(op==179 && !initialize(cid)){if(!VMS_EXEC(g_thrown))(*st)[frame_index].pc=(*st)[frame_index].op_pc;break;}
            Value v = pop();
            if (op == 179) VMS_CL(g_classes)[cid].statics[slot] = v;
            else {
                int ref = pop().ref;
                if (!ref) { throw_new("java/lang/NullPointerException", "putfield"); break; }
                if (VMS_HEAP(g_heap)[ref].indexed_key) ++VMS_NATIVE(g_index_key_epoch);
                VMS_HEAP(g_heap)[ref].fields[slot] = v;
            }
            break;
        }
        case 182: do_invoke(u16(), false, false, true); break;
        case 183: do_invoke(u16(), false, false, false); break;
        case 184: do_invoke(u16(), true, false, false); break;
        case 185: do_invoke(u16(), false, true, true); break;
        case 187: {
            int cid=ensure_class(cp_class(owner,u16()));
            if(!initialize(cid)){if(!VMS_EXEC(g_thrown))(*st)[frame_index].pc=(*st)[frame_index].op_pc;break;}
            push(Value::of_ref(alloc_obj(cid)));break;
        }
        case 188: case 189: {
            int n = pop().i;
            std::string desc;
            if (op == 189) { desc = cp_class(owner, u16()); desc = desc[0] == '[' ? "[" + desc : "[L" + desc + ";"; }
            else { int atype = u8(); const char* types = "ZCFDBSIJ"; if (atype < 4 || atype > 11) die("invalid newarray type"); desc = std::string("[") + types[atype - 4]; }
            if (n < 0) { throw_new("java/lang/NegativeArraySizeException", "newarray"); break; }
            int ref = alloc_array(desc, (size_t)n);
            push(Value::of_ref(ref));
            break;
        }
        case 190: {
            int ref = pop().ref;
            if (!ref) { throw_new("java/lang/NullPointerException", "arraylength"); break; }
            push(Value::of_i((int)VMS_HEAP(g_heap)[ref].elems.size()));
            break;
        }
        case 191: { int ref = pop().ref; if (!ref) throw_new("java/lang/NullPointerException", "athrow"); else throw_ref(ref); break; }
        case 192: {
            int idx = u16();
            int ref = fr.stack.empty() ? 0 : fr.stack.back().ref;
            if (ref && !type_compatible(ref, cp_class(owner, idx))) throw_new("java/lang/ClassCastException", cp_class(owner, idx));
            break;
        }
        case 193: {
            int idx = u16();
            int ref = pop().ref;
            push(Value::of_i(ref && type_compatible(ref, cp_class(owner, idx)) ? 1 : 0));
            break;
        }
        case 194: {
            int ref = pop().ref;
            if (!ref) { throw_new("java/lang/NullPointerException", "monitorenter"); break; }
            if (!monitor_enter(ref)) { push(Value::of_ref(ref)); fr.pc = fr.op_pc; suspend_current(); }
            break;
        }
        case 195: { int ref = pop().ref; if (!ref) throw_new("java/lang/NullPointerException", "monitorexit"); else monitor_exit(ref); break; }
        case 197: {
            int cls = u16(); int dims = u8(); std::string desc = cp_class(owner, cls);
            if (dims < 1) { push(Value::of_ref(0)); break; }
            std::vector<int> ds(dims);
            for (int i = dims - 1; i >= 0; --i) ds[i] = pop().i;
            if (std::any_of(ds.begin(), ds.end(), [](int n) { return n < 0; })) { throw_new("java/lang/NegativeArraySizeException", "multianewarray"); break; }
            std::function<int(int)> make = [&](int d) {
                int ref = alloc_array(desc.substr(d), (size_t)ds[d]);
                if (d + 1 < dims) for (int i = 0; i < ds[d]; ++i) VMS_HEAP(g_heap)[ref].elems.set(i, Value::of_ref(make(d + 1)));
                return ref;
            };
            push(Value::of_ref(make(0)));
            break;
        }
        case 198: { int off = s16(); if (pop().ref == 0) branch(off); break; }
        case 199: { int off = s16(); if (pop().ref != 0) branch(off); break; }
        case 200: { int off = (int)ru32(&code[fr.pc]); fr.pc = fr.op_pc + off; break; }
        case 168: { int off = s16(); push(Value::of_i(fr.pc)); fr.pc = fr.op_pc + off; break; }
        case 169: fr.pc = fr.locals[u8()].i; break;
        case 201: { int off = (int)ru32(&code[fr.pc]); fr.pc += 4; push(Value::of_i(fr.pc)); fr.pc = fr.op_pc + off; break; }
        default: die("opcode " + std::to_string(op) + " in " + owner.name + "." + fr.method->name);
        }
        if (VMS_EXEC(g_thrown) && st->empty()) break;
    }
    if (current_context().unhandled && !scope.has_parent()) {
        int ref = current_context().unhandled; current_context().unhandled = 0;
        report_uncaught(ref);
    }
    return result;
}

static Value run(Method* entry, const std::vector<Value>& args) {
    P7_SCOPE(Interp);
    RootScope roots(ReentrySite::RunArgs, {}, &args); // S5: host/native-supplied arguments until the callee frame owns them
#ifdef J2ME_TEST_REENTRY_GC_HOOK
    J2ME_TEST_REENTRY_GC_HOOK();
#endif
    return VMS_EXEC(g_execution_profile).enabled ? run_impl<true>(entry,args) : run_impl<false>(entry,args);
}

