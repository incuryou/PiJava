#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include <iostream>
#include <random>
int main() {
    // Exact differential against the previous byte-by-byte walker, including
    // invalid/truncated encodings, embedded NUL and negative/oversized offsets.
    std::mt19937 rng(0x234a5);
    for(int trial=0;trial<20000;++trial) {
        std::string s(size_t(rng()%1025),'\0');
        for(char& c:s)c=char(rng()%(trial%3?128:256));
        if(trial%4==0 && s.size()>8) {s[2]=char(0xed);s[3]=char(0xa0);s[4]=char(0x80);}
        int old_len=0;size_t p=0;while(p<s.size()){utf8_next(s,p);++old_len;}
        assert(utf8_len(s)==old_len);
        for(int k: {-1,0,1,7,8,9,int(rng()%1100),old_len,old_len+1}) {
            size_t old_byte=0;int n=0;while(old_byte<s.size() && n<k){utf8_next(s,old_byte);++n;}
            assert(utf8_byte(s,k)==old_byte);
            size_t at=old_byte;assert(utf8_at(s,k)==utf8_next(s,at));
        }
    }
    std::cout<<"PASS 20000 ASCII/UTF-8 differential strings\n";
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    const int cls=ensure_class("Optimization234Checks");
    Method* runmethod=find_method(cls,"run","()I",false);
    int result=run(runmethod,{}).i;assert(!VMS_EXEC(g_thrown));
#if J2ME_OPT_FAST_DISPATCH
    assert(VMS_EXEC(g_fast_ops)>100000 && VMS_EXEC(g_fast_blocks)>1000);
#endif
    std::vector<uint8_t> rgb(7*5*3);for(size_t i=0;i<rgb.size();i++)rgb[i]=uint8_t(i*37);
    const char* path="build/cache-fixture.png";write_png(path,rgb,7,5);
    std::vector<uint8_t> png;assert(read_whole(path,png));std::remove(path);VMS_CL(g_jar_files)["cache.png"]=png;
    int images=run(find_method(cls,"images","()I",false),{}).i;assert(!VMS_EXEC(g_thrown));
#if J2ME_OPT_IMAGE_CACHE
    assert(VMS_RS(g_decoded_images).hits==1 && VMS_RS(g_decoded_images).misses==1 && VMS_RS(g_decoded_images).entries()==1);
#endif
#if J2ME_OPT_FAST_PNG
    assert(VMS_RS(g_png_fast)>0);
#endif
    // Native time and sleep, stop/pause/mode boundaries must never reuse proof.
    Method dummy;dummy.name="fixture";auto& t=VMS_EXEC(g_main_thread);VMS_EXEC(g_speed_cap)=60;speed_sync_clock();
    auto prove=[&](){t.auto_fps.started=true;t.auto_fps.verified_gate_seen=true;t.auto_fps.gate_method=&dummy;t.auto_fps.gate_pc=7;t.auto_fps.candidate=50000;t.auto_fps.stable=9;VMS_EXEC(g_speed_budget_owner)=&t;VMS_EXEC(g_game_clock).rate(paint_clock_us(),2.999940);};
    prove();speed_invalidate_group(t,true);assert(VMS_EXEC(g_game_clock).scale==1.0);
#if J2ME_OPT_GATE_RECOVERY
    assert(t.gate_recovery.method==&dummy);t.auto_fps.started=true;
    speed_certified_gate(t,&dummy,7,game_current_ms()-50,50);
    assert(t.gate_recovery.pending_rate>2.99);speed_note_paint(paint_clock_us(),&t);
    assert(VMS_EXEC(g_game_clock).scale>2.99 && VMS_EXEC(g_game_clock).scale<=2.999940);
    prove();speed_invalidate_group(t,true);speed_pause_clock(true);assert(!t.gate_recovery.method && VMS_EXEC(g_game_clock).scale==1.0);
    speed_pause_clock(false);prove();speed_invalidate_group(t,true);VMS_EXEC(g_speed_cap)=30;speed_sync_clock();assert(!t.gate_recovery.method && VMS_EXEC(g_game_clock).scale==1.0);
    VMS_EXEC(g_speed_cap)=60;speed_sync_clock();prove();speed_invalidate_group(t,false);assert(!t.gate_recovery.method && VMS_EXEC(g_game_clock).scale==1.0);
#endif
    const auto fast=VMS_EXEC(g_fast_ops),slow=VMS_EXEC(g_slow_ops);
    stop_thread();assert(!VMS_RS(g_decoded_images).entries() && !t.gate_recovery.method);
    std::cout<<"PASS 2.34 VM checksum="<<result<<" image_checksum="<<images<<" fast_ops="<<fast<<" slow_ops="<<slow<<"\n";
}
