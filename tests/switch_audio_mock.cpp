// Host-side ownership and lifecycle checks; no Switch hardware is simulated.
#include "../src/switch_audio.h"
#include <cassert>
#include <deque>
#include <iostream>
static std::deque<AudioOutBuffer*> pending, released;
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
    if(failAppend)return 123; pending.push_back(b); ++appends; return 0;
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
int main(){
    j2me_audio::Engine engine; j2me_audio::SwitchAudioSink sink(clockMicros);
    assert(sink.init()==0 && sink.available() && sink.queuedBuffers()==4 && pending.size()==4 && appends==4 && cacheFlushes==4);
    assert(sink.metrics().pump_count==0 && sink.metrics().render_blocks==0 && sink.metrics().released_buffers==0);
    for(auto* b:pending)for(size_t i=0;i<2048;++i)assert(static_cast<int16_t*>(b->buffer)[i]==0);
    int h=engine.playTone(69,1000,100);assert(h>0);
    int polls=releasePolls;
    assert(sink.pump(engine)==0 && engine.time(h)==0 && releasePolls-polls==1);play(2);
    assert(sink.pump(engine)==0 && pending.size()==4 && engine.time(h)==42666);
    auto stats=sink.takeStats();
    assert(stats.pump_count==2 && stats.released_buffers==2 && stats.min_queued_buffers==2);
    assert(stats.render_blocks==2 && stats.render_frames==2048 && stats.total_mix_us==14 && stats.max_mix_us==7);
    assert(stats.nonzero_blocks==2 && stats.peak_abs_sample>0 && stats.peak_abs_sample<=32768);
    assert(stats.max_pump_gap_us==7 && stats.queue_empty_polls==0 && stats.errors==0);
    assert(sink.metrics().pump_count==0 && sink.metrics().render_blocks==0 && sink.metrics().render_frames==0);
    assert(sink.metrics().released_buffers==0 && sink.metrics().queue_empty_polls==0 && sink.metrics().min_queued_buffers==4);
    assert(sink.metrics().total_mix_us==0 && sink.metrics().max_mix_us==0 && sink.metrics().max_pump_gap_us==0 && sink.metrics().errors==0);
    assert(sink.metrics().nonzero_blocks==0 && sink.metrics().peak_abs_sample==0);
    // A stalled caller drains the queue. A single bounded pump recovers it,
    // while reporting risk without claiming that audible content dropped out.
    fakeUs+=100000;play(4);polls=releasePolls;
    assert(sink.pump(engine)==0 && pending.size()==4 && sink.queuedBuffers()==4 && releasePolls-polls==5);
    stats=sink.takeStats();
    assert(stats.pump_count==1 && stats.released_buffers==4 && stats.render_frames==4096 && stats.min_queued_buffers==0);
    assert(stats.queue_empty_polls==1 && stats.max_pump_gap_us>=100000 && stats.total_mix_us==28 && stats.max_mix_us==7);
    assert(sink.pump(engine)==0 && sink.metrics().queue_empty_polls==0 && sink.metrics().released_buffers==0);
    sink.takeStats();
    assert(sink.suspend(engine,true)==0 && pending.empty() && released.empty() && engine.paused());
    auto t=engine.time(h);fakeUs+=5000000; assert(sink.pump(engine)==0 && engine.time(h)==t);
    assert(sink.metrics().pump_count==0 && sink.metrics().released_buffers==0 && sink.metrics().queue_empty_polls==0);
    assert(sink.suspend(engine,false)==0 && pending.size()==4 && engine.time(h)>t && !engine.paused());
    assert(sink.pump(engine)==0 && sink.metrics().max_pump_gap_us==7 && sink.metrics().queue_empty_polls==0);
    sink.takeStats();
    assert(sink.reset(engine)==0 && pending.size()==4 && engine.state(h)==0);
    assert(sink.metrics().released_buffers==0 && sink.metrics().render_blocks==0 && sink.metrics().queue_empty_polls==0);
    assert(sink.pump(engine)==0 && sink.metrics().max_pump_gap_us==7 && sink.metrics().min_queued_buffers==4);
    play(1);assert(sink.pump(engine)==0 && sink.metrics().render_blocks==1 && sink.metrics().nonzero_blocks==0 && sink.metrics().peak_abs_sample==0);
    sink.takeStats();
    assert(sink.shutdown()==0 && !service && exits==1 && !sink.available());
    assert(sink.queuedBuffers()==0 && sink.metrics().queue_empty_polls==0 && sink.metrics().released_buffers==0);
    assert(sink.shutdown()==0 && exits==1);
    sink.takeStats();
    assert(sink.init()==0); play(1);failAppend=true;
    assert(sink.pump(engine)==123 && !sink.available() && !service && exits==2 && sink.lastError()==123);
    assert(sink.metrics().errors==1 && sink.metrics().released_buffers==1 && sink.queuedBuffers()==0);
    failAppend=false; assert(sink.init()==0); retainOnFlush=true;
    assert(R_FAILED(sink.suspend(engine,true)) && !sink.available() && !service && exits==3);
    assert(sink.metrics().errors==2 && sink.metrics().released_buffers==1);
    retainOnFlush=false;
    // Failures retain diagnostic operation names and close service ownership.
    sink.takeStats();failInitialize=true;
    assert(sink.init()==124 && sink.metrics().errors==1 && !sink.available() && !service);
    assert(std::strcmp(sink.errorOperation(),"audoutInitialize")==0);
    failInitialize=false;failStart=true;
    assert(sink.init()==126 && sink.metrics().errors==2 && !sink.available() && !service);
    assert(std::strcmp(sink.errorOperation(),"audoutStartAudioOut")==0);
    failStart=false;assert(sink.init()==0);failRelease=true;
    assert(sink.pump(engine)==125 && sink.metrics().errors==3 && !sink.available() && !service);
    assert(std::strcmp(sink.errorOperation(),"audoutGetReleasedAudioOutBuffer")==0);
    failRelease=false;assert(sink.init()==0);failStop=true;
    assert(sink.suspend(engine,true)==127 && sink.metrics().errors==4 && !sink.available() && !service);
    assert(std::strcmp(sink.errorOperation(),"audoutStopAudioOut")==0);
    failStop=false;assert(sink.init()==0);failFlush=true;
    assert(sink.reset(engine)==128 && sink.metrics().errors==5 && !sink.available() && !service);
    assert(std::strcmp(sink.errorOperation(),"audoutFlushAudioOutBuffers")==0);
    failFlush=false;sink.takeStats();
    assert(sink.metrics().errors==0 && sink.metrics().min_queued_buffers==4);
    assert(sink.init()==0);
    sink.takeStats();
    assert(sink.pumpFrame(engine)==0 && sink.metrics().render_blocks==0 && sink.queuedBuffers()==4);
    play(3);
    assert(sink.pumpFrame(engine)==0 && sink.metrics().render_blocks==1 && sink.queuedBuffers()==2);
    assert(sink.pumpFrame(engine)==0 && sink.metrics().render_blocks==1 && sink.queuedBuffers()==2);
    assert(sink.shutdown()==0);
    // New cached service path: raw pump API assertions above remain unchanged.
    assert(sink.init()==0);sink.takeStats();polls=releasePolls;
    for(int i=0;i<80;++i)assert(sink.pumpThrottled(engine)==0);
#if J2ME_OPT_AUDIO_POLL
    assert(releasePolls==polls && sink.metrics().skipped_release_polls==80);
#endif
    fakeUs+=2000;play(1);polls=releasePolls;
    assert(sink.pumpThrottled(engine,true)==0 && releasePolls>polls && sink.queuedBuffers()==3);
    fakeUs+=2000;play(2);assert(sink.pumpThrottled(engine,true)==0 && sink.queuedBuffers()==2);
    fakeUs+=100000;play(2);assert(sink.pumpThrottled(engine,true)==0 && sink.queuedBuffers()==1);
    polls=releasePolls;assert(sink.pumpThrottled(engine,true)==0 && releasePolls>polls && sink.queuedBuffers()==2);
    fakeUs=1;polls=releasePolls;assert(sink.pumpThrottled(engine,true)==0 && releasePolls>polls);
    assert(sink.suspend(engine,true)==0);polls=releasePolls;
    assert(sink.pumpThrottled(engine)==0 && releasePolls==polls && pending.empty());
    assert(sink.suspend(engine,false)==0 && sink.queuedBuffers()==4);
    fakeUs+=2000;failRelease=true;assert(sink.pumpThrottled(engine)==125 && !sink.available());
    failRelease=false;assert(sink.init()==0 && sink.shutdown()==0);
    std::cout<<"PASS: queue ownership, bounded stall recovery, window metrics, lifecycle gap exclusion, and audout failure diagnostics\n";
}
