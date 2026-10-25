#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

static void mediaCheck(bool ok,const char* label) {
    if(!ok) {std::cerr << "FAIL " << label << "\n";std::exit(1);}
}
int main(int argc,char** argv) {
    VMS_CL(g_classpath)={argc>1?argv[1]:"build/test-classes",argc>2?argv[2]:"romfs/classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    int c=ensure_class("MediaEventChecks");
    auto invoke=[&](const char* name){return run(find_method(c,name,"()I",false),{}).i;};
    auto setup=[&](int loops,bool close){return run(find_method(c,"setup","(II)I",false),{Value::of_i(loops),Value::of_i(close)}).i;};
    auto render=[&](size_t frames){std::vector<int16_t> pcm(frames*2);VMS_PLAT(g_audio).render(pcm.data(),frames);};
    mediaCheck(setup(1,true)==400,"start real PCM player");
    render(48000);
    mediaCheck(invoke("ended")==0 && invoke("closed")==0,"audio render cannot invoke Java listeners");
    host_events();
    mediaCheck(invoke("ended")==1 && invoke("closed")==1 && invoke("state")==0,"main-thread end callback may close its own player");
    mediaCheck(run(find_method(c,"endedAt","()J",false),{}).l==1000000,"end event carries sample-derived Long time");
    host_events();mediaCheck(invoke("ended")==1,"end event exactly once");
    mediaCheck(setup(2,false)==400,"two-loop start");
    render(48000);host_events();
    mediaCheck(invoke("state")==400 && invoke("ended")==0,"first loop does not emit completion");
    render(80000);
    mediaCheck(invoke("ended")==0,"final audio loop queues callback");host_events();
    mediaCheck(invoke("ended")==1 && invoke("state")==300,"final loop completes to PREFETCHED");invoke("close");
    setup(-1,false);render(144000);host_events();
    mediaCheck(invoke("ended")==0 && invoke("state")==400,"infinite loop remains started");invoke("close");
    setup(1,false);render(48000);invoke("close");host_events();
    mediaCheck(invoke("ended")==0 && invoke("closed")==1,"closing discards queued end");
    mediaCheck(invoke("tone")==1 && !VMS_PLAT(g_tones).empty(),"actual tone registered");render(4800);host_events();
    mediaCheck(VMS_PLAT(g_tones).empty(),"completed tone handle reclaimed");
    setup(-1,false);stop_thread();
    render(48000);int ended;
    mediaCheck(!VMS_PLAT(g_audio).pollEnd(ended) && VMS_PLAT(g_tones).empty(),"VM stop clears engine and completion queue");
    mediaCheck(invoke("state")==0 && invoke("closed")==0,"VM stop disposes Java handles without application callbacks");
    std::cout << "PASS MediaEventChecks 14 real rendering, host completion, loop, close and tone cleanup\n";
}
