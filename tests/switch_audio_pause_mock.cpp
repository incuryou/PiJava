// P2.1-c5: Switch audio sink pause/resume continuity with the libnx test double. The sink lives in a never-destroyed
// static cell (same layout rule as VMStorage::platform) and its buffer addresses are checked to stay fixed.
#include "../src/switch_audio.h"
#include <cassert>
#include <deque>
#include <iostream>
static std::deque<AudioOutBuffer*> pending, released;
#include <vector>
#include <set>
static std::vector<std::vector<int16_t>> appended; static std::set<void*> bufaddrs;
static void record(AudioOutBuffer* b){auto* s=static_cast<int16_t*>(b->buffer);appended.emplace_back(s,s+2048);bufaddrs.insert(b->buffer);}
static int exits=0, stops=0, flushes=0, appends=0, cacheFlushes=0;
static int releasePolls=0;
static bool service=false, started=false, failAppend=false, retainOnFlush=false;
static bool failInitialize=false, failStart=false, failStop=false, failFlush=false, failRelease=false;
static uint64_t fakeUs=0;
static uint64_t clockMicros(){const uint64_t now=fakeUs;fakeUs+=7;return now;}
Result audoutInitialize(){ assert(!service); if(failInitialize)return 124; service=true; return 0; }
void audoutExit(){ assert(service); service=false; started=false; pending.clear(); released.clear(); ++exits; }
u32 audoutGetSampleRate(){return 48000;} u32 audoutGetChannelCount(){return 2;}
PcmFormat audoutGetPcmFormat(){return PcmFormat_Int16;} bool hosversionBefore(int,int,int){return false;}
void armDCacheFlush(void* p,size_t n){assert((uintptr_t(p)&4095)==0 && n==4096);++cacheFlushes;}
Result audoutAppendAudioOutBuffer(AudioOutBuffer* b){
    assert(service && b->next==nullptr && b->buffer_size==4096 && b->data_size==4096 && b->data_offset==0);
    for(auto* p:pending)assert(p!=b); for(auto* p:released)assert(p!=b);
    if(failAppend)return 123; pending.push_back(b); ++appends; record(b); return 0;
}
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer** p,u32* count){
    assert(service); ++releasePolls; *p=nullptr; *count=0; if(failRelease)return 125;
    if(!released.empty()){*p=released.front();released.pop_front();*count=1;}return 0;
}
Result audoutStartAudioOut(){assert(service); if(failStart)return 126; started=true;return 0;}
Result audoutStopAudioOut(){assert(service); ++stops;if(failStop)return 127;started=false;return 0;}
Result audoutFlushAudioOutBuffers(bool* f){
    assert(service&&!started); ++flushes; if(failFlush)return 128; *f=!pending.empty();
    if(!retainOnFlush)while(!pending.empty()){released.push_back(pending.front());pending.pop_front();}return 0;
}
static void play(size_t n){while(n--){assert(!pending.empty());released.push_back(pending.front());pending.pop_front();}}
union SinkCell { j2me_audio::SwitchAudioSink value; SinkCell() : value(clockMicros) {} ~SinkCell() {} };
static SinkCell g_cell;
static constexpr j2me_audio::SwitchAudioSink* kSink = &g_cell.value;   // address constant => fixed static address
int main(){
    auto& sink=*kSink; j2me_audio::Engine engine, ref; int fails=0;
    auto check=[&](bool c,const char* w){std::printf("%s %s\n",c?"PASS":"FAIL",w);if(!c)++fails;};
    check(sink.init()==0 && appended.size()==4,"init queues 4 silent buffers");
    std::set<void*> addrs0=bufaddrs; appended.clear();
    int h=engine.playTone(64,3000,90), hr=ref.playTone(64,3000,90); check(h>0&&hr>0,"tones");
    size_t dropped=0;
    for(int round=0;round<3;++round){
        for(int i=0;i<4;++i){play(2);fakeUs+=2000;check(sink.pump(engine)==0,"pump");}
        dropped+=pending.size();            // rendered but not yet played when the pause flushes the queue
        auto t=engine.time(h);
        check(sink.suspend(engine,true)==0 && pending.empty() && engine.paused(),"suspend flushes queue and pauses engine");
        fakeUs+=500000; check(sink.pump(engine)==0 && engine.time(h)==t,"paused: no render, position frozen");
        check(sink.suspend(engine,false)==0 && pending.size()==4 && !engine.paused(),"resume refills 4 buffers");
    }
    // every buffer handed to audout must be the next block of an uninterrupted reference render
    int16_t pcm[2048]; size_t mismatch=0;
    for(size_t i=0;i<appended.size();++i){ ref.render(pcm,1024); if(!std::equal(pcm,pcm+2048,appended[i].begin()))++mismatch; }
    std::printf("INFO blocks_handed_to_audout=%zu mismatched=%zu queued_blocks_flushed_by_pause=%zu\n",appended.size(),mismatch,dropped);
    check(mismatch==0,"engine stream across pause/resume: every block exactly once, in order (no lost/reordered samples)");
    check(bufaddrs==addrs0 && bufaddrs.size()==4,"audout buffer addresses fixed for the whole run (4 page buffers)");
    check(kSink==&g_cell.value,"sink address constant");
    check(sink.shutdown()==0 && !service,"shutdown");
    std::printf("SWITCH_AUDIO_PAUSE_RESULT %s fails=%d\n",fails?"FAIL":"PASS",fails);
    return fails?1:0;
}
