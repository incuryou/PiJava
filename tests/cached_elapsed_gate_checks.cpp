#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <cassert>
int main(int argc,char** argv) {
    VMS_CL(g_classpath)={argc>1?argv[1]:"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    for(int mode:{0,1,2})for(int cap:{0,60}) {
        stop_thread();VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_slice_ms)=14;
        VMS_HEAP(g_heap).assign(1,Obj{});VMS_HEAP(g_free).clear();VMS_CL(g_classes).clear();VMS_CL(g_class_index).clear();
        VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);VMS_EXEC(g_midlet_state)=MidletState::Active;
        const int cls=ensure_class("CachedElapsedGateFixture");run(find_method(cls,"start","(I)V",false),{Value::of_i(mode)});
        auto sample=[&](int milliseconds) {
            const int64_t begin=monotonic_ms();const int drawn=VMS_RS(g_game_frames);
            while(monotonic_ms()-begin<milliseconds) {
                const auto started=monotonic_ms();sched_game_work(false,[]{},[](int64_t ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));});
                sched_present_turn(true,false,false,3,[&]{auto left=16-(monotonic_ms()-started);if(left>0)std::this_thread::sleep_for(std::chrono::milliseconds(left));VMS_RS(g_fb_dirty)=false;});
            }
            return (VMS_RS(g_game_frames)-drawn)*1000.0/(monotonic_ms()-begin);
        };
        sample(400);const double fps=sample(mode==2?2100:1100);
        std::cout<<"cached_elapsed mode="<<mode<<" cap="<<cap<<" fps="<<fps<<" scale="<<VMS_EXEC(g_game_clock).scale<<std::endl;
        assert(VMS_EXEC(g_game_clock).scale==1.0);
#ifndef CACHED_ELAPSED_BASELINE
        if(mode==0&&cap) {
            Method* method=find_method(cls,"run","()V",false);assert(!method->cached_elapsed_gates.empty());
            JitRegionMethod regions;jit_region_scan(method,regions);
            for(const auto& pattern:method->cached_elapsed_gates)
                assert(regions.banned[pattern.branch]&&regions.banned[pattern.sleep]&&regions.banned[pattern.repeat_clock]);
            auto& t=VMS_EXEC(g_threads).front();VMS_EXEC(g_running_thread)=&t;
            RunScope scope(&t.frames,true);Frame& frame=t.frames.back();assert(frame.method==method);
            const auto pattern=method->cached_elapsed_gates.front();
            int64_t anchor=0,elapsed=0;assert(cached_elapsed_static(frame,pattern.anchor_field,anchor));
            assert(cached_elapsed_static(frame,pattern.elapsed_field,elapsed));
            for(const auto* name:{"anchor","elapsed"}) {
                Field* field=find_field(cls,name,"J");const auto flags=field->flags;field->flags|=0x0040;
                int64_t value=0;const int cp=name==std::string("anchor")?pattern.anchor_field:pattern.elapsed_field;
                assert(!cached_elapsed_static(frame,cp,value));
                frame.op_pc=pattern.clock;QuickSite volatile_site;speed_cached_elapsed_clock(frame,anchor+elapsed,volatile_site);
                assert(volatile_site.cached_elapsed==0);field->flags=flags;
            }
            auto prepare=[&] {
                auto& s=t.cached_elapsed;s.reset();s.method=method;s.pattern=pattern;s.canvas=VMS_RS(g_current);s.cap=cap;
                s.depth=t.frames.size();s.draws=t.owned_draws-1;s.anchor_ms=anchor;s.latest_ms=anchor+elapsed;
                s.anchor_us=paint_clock_us()-10000;s.due_us=paint_clock_us()-1;s.active=true;
                t.field_gate_anchor.record(method,pattern.anchor_clock,pattern.anchor_field,anchor,s.anchor_us,s.draws);
                VMS_EXEC(g_speed_budget_owner)=&t;
            };
            prepare();assert(speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();--t.owned_draws;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));++t.owned_draws;
            prepare();++t.owned_draws;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));--t.owned_draws;
            prepare();++t.cached_elapsed.anchor_ms;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();++t.cached_elapsed.depth;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();++t.field_gate_anchor.clock_pc;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();++t.field_gate_anchor.field;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();++t.cached_elapsed.canvas;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();VMS_EXEC(g_game_clock).rate(paint_clock_us(),2.0);assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));VMS_EXEC(g_game_clock).rate(paint_clock_us(),1.0);
            prepare();t.cached_elapsed.cap=30;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();VMS_EXEC(g_speed_budget_owner)=nullptr;assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));
            prepare();set_interrupted(t.ref,true);assert(!speed_cached_elapsed_branch(frame,pattern.branch,-1));assert(thread_interrupted(t.ref));set_interrupted(t.ref,false);
            prepare();++t.field_gate_anchor.real_us;frame.op_pc=pattern.sleep;
            const int exception=alloc_obj(ensure_class("java/lang/InterruptedException"));throw_ref(exception);
            assert(!t.cached_elapsed.active);assert(t.cached_elapsed.rejected_anchor_us==t.field_gate_anchor.real_us);const int64_t rejected=t.cached_elapsed.rejected_anchor_us;
            frame.op_pc=pattern.clock;QuickSite site;speed_cached_elapsed_clock(frame,anchor+elapsed,site);
            assert(!t.cached_elapsed.active&&t.cached_elapsed.rejected_anchor_us==rejected);
            VMS_EXEC(g_running_thread)=nullptr;
        }
#endif
        if(mode==0 && cap)assert(fps>48 && fps<64);
        else if(mode==2)assert(fps<2);
        else assert(fps>15 && fps<22);
    }
    p7::pool().shutdown();
}
