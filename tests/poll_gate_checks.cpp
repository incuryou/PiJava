#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>

int main() {
    auto constant=[](int i)->int64_t{return i==1?50:0;};
    auto yield=[](int i){return i==2;};
    std::vector<uint8_t> code={0xb8,0,3,0x1f,0x65,0x14,0,1,0x94,0x9c,0,9,0xb8,0,2,0xa7,0xff,0xf1,0xb1};
    auto gate=recognize_time_gate(code,0,3,constant,yield);
    assert(gate.local==1 && gate.millis==50);
    for(size_t size=0;size<18;++size) {
        std::vector<uint8_t> shortCode(code.begin(),code.begin()+size);
        assert(recognize_time_gate(shortCode,0,3,constant,yield).local<0);
    }
    code[12]=0x84; // A waiting-path mutation must prevent optimization.
    assert(recognize_time_gate(code,0,3,constant,yield).local<0);
    PollGateState state;
    for(int i=0;i<4;++i)assert(!state.observe(&state,1,100,50,110+i,true));
    assert(state.wait_us(120,1)==500);
    assert(state.observe(&state,1,100,50,150,true));
    assert(state.wait_us(150,1)==0);
    assert(state.observe(&state,1,150,50,200,true)); // Pacing consumed this gate.
    state.reset();assert(!state.observe(&state,1,100,50,150,true));
    assert(!state.observe(&state,1,100,50,120,false));
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    for(int cap:{0,30,60})for(int mode:{0,1,2,3,4}) {
        stop_thread();VMS_EXEC(g_speed_cap)=cap;VMS_EXEC(g_slice_ms)=14;
        VMS_HEAP(g_heap).assign(1,Obj{});VMS_HEAP(g_free).clear();VMS_CL(g_classes).clear();VMS_CL(g_class_index).clear();
        VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);VMS_EXEC(g_midlet_state)=MidletState::Active;
        int cls=ensure_class("PollGateFixture");
        run(find_method(cls,"start","(I)V",false),{Value::of_i(mode)});
        uint64_t visible=0;uint32_t last_pixel=0xffffffff;
        auto observe=[&](int ms) {
            const auto start=monotonic_ms();const int frames=VMS_RS(g_game_frames);
            while(monotonic_ms()-start<ms) {
                speed_wait_paced_sleep();advance_game_frame();
                // Match the Switch present guard. An unfinished logical body
                // cannot submit a helper image or clear its final dirty frame.
                if(VMS_RS(g_fb_dirty) && !speed_group_incomplete()) {
                    const uint32_t pixel=(uint32_t(VMS_RS(g_fb)[0])<<16)|(uint32_t(VMS_RS(g_fb)[1])<<8)|VMS_RS(g_fb)[2];
                    if(pixel!=last_pixel){++visible;last_pixel=pixel;}
                    VMS_RS(g_fb_dirty)=false;
                }
            }
            return double(VMS_RS(g_game_frames)-frames)*1000/(monotonic_ms()-start);
        };
        observe(2400);
        const auto update_field=find_field(cls,"updates","I");
        const int update_before=VMS_CL(g_classes)[cls].statics[update_field->slot].i;
        const auto paints_field=find_field(cls,"paints","I");
        const int paints_before=VMS_CL(g_classes)[cls].statics[paints_field->slot].i;
        const uint64_t budgets_before=VMS_EXEC(g_speed_budget_frames),visible_before=visible;
        const int64_t sample_start=monotonic_ms();const auto fps=observe(800);
        const double sample_ms=monotonic_ms()-sample_start;
        const int update_count=VMS_CL(g_classes)[cls].statics[update_field->slot].i-update_before;
        const int paint_count=VMS_CL(g_classes)[cls].statics[paints_field->slot].i-paints_before;
        const double update_fps=update_count*1000.0/sample_ms;
        const uint64_t budget_count=VMS_EXEC(g_speed_budget_frames)-budgets_before;
        const double budget_fps=budget_count*1000.0/sample_ms;
        const double visible_fps=(visible-visible_before)*1000.0/sample_ms;
        const auto aux=find_field(cls,"auxiliary","I");
        const int auxCount=VMS_CL(g_classes)[cls].statics[aux->slot].i;
        std::cout<<"literal_gate cap="<<cap<<" mode="<<mode<<" fps="<<fps<<" scale="<<VMS_EXEC(g_game_clock).scale
                 <<" updates_fps="<<update_fps<<" visible_fps="<<visible_fps<<" budget_fps="<<budget_fps<<" raw_paints="<<paint_count<<" gate_reads="<<VMS_EXEC(g_poll_gate_reads)<<" waits="<<VMS_EXEC(g_poll_waits)<<" auxiliary="<<auxCount<<"\n";
        assert(VMS_EXEC(g_poll_gate_reads)>0 && VMS_EXEC(g_poll_waits)>0);
        if(mode==3) {
            assert(!speed_group_incomplete());
            assert(cap ? fps>cap*0.88 && fps<cap*1.1 : fps>23);
            if(cap)assert(VMS_EXEC(g_speed_group_timeouts)>0);
        } else {
            assert(auxCount>=18);
            assert(cap ? update_fps>cap*0.88 && update_fps<cap*1.1 : update_fps>17 && update_fps<23);
        }
        if(mode==2 || mode==4) {
            assert(VMS_CL(g_classes)[cls].statics[find_field(cls,"helperFailures","I")->slot].i==0);
            assert(paint_count>=update_count*2-2);
            assert(VMS_CL(g_classes)[cls].statics[find_field(cls,"lastStage","I")->slot].i==2);
            assert(VMS_CL(g_classes)[cls].statics[find_field(cls,"lastUpdate","I")->slot].i==VMS_CL(g_classes)[cls].statics[update_field->slot].i);
        }
        if(cap)assert(budget_fps>cap*0.88 && budget_fps<cap*1.1);
        if(cap && mode!=3)assert(std::llabs(int64_t(budget_count)-update_count)<=1);
        if(mode==4 && cap)assert(VMS_EXEC(g_speed_group_timeouts)>0);
        if(mode!=3)assert(cap ? visible_fps>cap*0.88 && visible_fps<cap*1.1 : visible_fps>17 && visible_fps<23);
        if(cap && mode!=3)assert(std::fabs(VMS_EXEC(g_game_clock).scale-(cap==30?1.5:3.0))<0.08);
    }
    stop_thread();std::cout<<"PASS PollGateChecks literal gates, pure polling, auxiliary timers and resets\n";
}
