#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Bounded host observations. No Java handles/roots, pointers to VM frames,
// disk I/O, locks or scheduling decisions. All clocks are monotonic usec.
namespace runtime_diag {
inline bool text(char* dst,size_t size,const char* src){
    if(!size)return true;
    size_t n=0;for(;src&&src[n]&&n+1<size;++n){const unsigned char c=src[n];dst[n]=c<32||c==127?' ':char(c);}
    const bool truncated=src&&src[n]!=0;
    if(truncated&&n){
        size_t lead=n-1;while(lead&&(uint8_t(dst[lead])&0xc0)==0x80)--lead;
        const auto c=uint8_t(dst[lead]);const size_t need=(c&0xf8)==0xf0?4:(c&0xf0)==0xe0?3:(c&0xe0)==0xc0?2:1;
        if(need>n-lead)n=lead; // Do not turn a valid UTF-8 prefix into a broken log line.
    }
    dst[n]=0;return truncated;
}
class Flight {
public:
    static constexpr size_t Capacity=128,TextBytes=640;
private:
    struct Event {uint64_t seq=0;int64_t us=0;char kind[32]{},detail[TextBytes]{};bool truncated=false;};
    std::array<Event,Capacity> events_{};uint64_t count_=0;
public:
    void reset(){count_=0;}
    void note(int64_t us,const char* kind,const char* detail){
        auto& e=events_[size_t(count_%Capacity)];e.seq=++count_;e.us=us;
        e.truncated=text(e.kind,sizeof(e.kind),kind);e.truncated|=text(e.detail,sizeof(e.detail),detail);
    }
    uint64_t overwritten()const{return count_>Capacity?count_-Capacity:0;}
    template<class Sink>void dump(Sink&& sink,size_t limit=Capacity)const{
        const size_t retained=size_t(std::min<uint64_t>(count_,std::min(Capacity,limit)));
        for(uint64_t n=count_-retained;n<count_;++n){const auto& e=events_[size_t(n%Capacity)];char line[800];
            std::snprintf(line,sizeof(line),"DIAGHISTORY seq=%llu mono_us=%lld event=%s truncated=%d %s",(unsigned long long)e.seq,(long long)e.us,e.kind,int(e.truncated),e.detail);sink(line);}
    }
};
class Labels {
public:static constexpr size_t Capacity=4096,Bytes=192;
private:
    // Single owner writes each cell once, before publishing its ID. No reset
    // or slot reuse while the logger can be alive; readers never see VM data.
    std::array<std::array<char,Bytes>,Capacity> labels_{};
    std::atomic<uint32_t> size_{0};std::atomic<uint64_t> overflow_{0};
public:
    uint32_t add(const char* label){
        const auto n=size_.load(std::memory_order_relaxed);
        if(n>=Capacity){overflow_.fetch_add(1,std::memory_order_relaxed);return 0;}
        if(text(labels_[n].data(),Bytes,label)){
            constexpr char marker[]="[truncated]";
            text(labels_[n].data(),Bytes-sizeof(marker)+1,label);
            std::memcpy(labels_[n].data()+std::strlen(labels_[n].data()),marker,sizeof(marker));
        }
        size_.store(n+1,std::memory_order_release);return n+1;
    }
    const char* get(uint32_t id)const{
        return id&&id<=size_.load(std::memory_order_acquire)?labels_[id-1].data():"unknown";
    }
    uint64_t overflow()const{return overflow_.load(std::memory_order_relaxed);}
};
enum class Phase:uint64_t {Host,Java,Native,Present,GC,Audio,Launch,Restore,Error};
inline const char* phase_name(Phase p){
    switch(p){case Phase::Java:return "java";case Phase::Native:return "native";case Phase::Present:return "present";
    case Phase::GC:return "gc";case Phase::Audio:return "audio";case Phase::Launch:return "launch";
    case Phase::Restore:return "restore";case Phase::Error:return "error";default:return "host";}
}
struct LiveSnapshot {
    uint64_t session=0,launch=0;int64_t host_us=0,point_us=0,input_us=0;
    uint32_t method=0,caller=0;int bci=-1,depth=0,key=0;bool release=false,active=false,paused=false;Phase phase=Phase::Host;
};
class Live {
    // Every payload word is atomic: a plain-POD seqlock would still have a
    // C++ data race. Only one VM owner publishes; the logger retries boundedly.
    static constexpr size_t Words=14;
    std::atomic<uint64_t> sequence_{0};std::array<std::atomic<uint64_t>,Words> words_{};
public:
    void publish(const LiveSnapshot& v){
        sequence_.fetch_add(1,std::memory_order_acq_rel);
        const uint64_t a[Words]={v.session,v.launch,uint64_t(v.host_us),uint64_t(v.point_us),uint64_t(v.input_us),v.method,v.caller,uint64_t(int64_t(v.bci)),uint64_t(int64_t(v.depth)),uint64_t(int64_t(v.key)),uint64_t(v.release),uint64_t(v.active),uint64_t(v.paused),uint64_t(v.phase)};
        for(size_t n=0;n<Words;++n)words_[n].store(a[n],std::memory_order_relaxed);
        sequence_.fetch_add(1,std::memory_order_release);
    }
    bool read(LiveSnapshot& out)const{
        for(unsigned attempt=0;attempt<4;++attempt){const auto first=sequence_.load(std::memory_order_acquire);if(first&1)continue;
            uint64_t a[Words];for(size_t n=0;n<Words;++n)a[n]=words_[n].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if(first!=sequence_.load(std::memory_order_acquire))continue;
            out.session=a[0];out.launch=a[1];out.host_us=int64_t(a[2]);out.point_us=int64_t(a[3]);out.input_us=int64_t(a[4]);out.method=uint32_t(a[5]);out.caller=uint32_t(a[6]);out.bci=int(int64_t(a[7]));out.depth=int(int64_t(a[8]));out.key=int(int64_t(a[9]));out.release=a[10]!=0;out.active=a[11]!=0;out.paused=a[12]!=0;out.phase=Phase(a[13]);return true;
        }return false;
    }
};
class Watchdog {
    bool stalled_=false;int64_t last_report_=0,stall_begin_=0;uint64_t reports_=0;
    uint64_t session_=0,launch_=0;
public:
    template<class Sink>void poll(int64_t now,const Live& live,const Labels& labels,Sink&& sink){
        LiveSnapshot v;if(!live.read(v))return;
        if(v.session!=session_||v.launch!=launch_){session_=v.session;launch_=v.launch;stalled_=false;last_report_=stall_begin_=0;reports_=0;}
        if(!v.active||v.paused||!v.host_us||now<v.host_us){stalled_=false;return;}
        const auto age=now-v.host_us;
        if(age<2000000){
            if(stalled_){char line[320];std::snprintf(line,sizeof(line),"DIAGWATCH event=HOST_TURN_RECOVERED session=%llu launch=%llu mono_us=%lld observed_stall_us=%lld reports=%llu root_cause=unattributed",(unsigned long long)v.session,(unsigned long long)v.launch,(long long)now,(long long)(now-stall_begin_),(unsigned long long)reports_);sink(line);stalled_=false;}return;
        }
        if(stalled_&&now-last_report_<10000000)return;
        if(!stalled_)stall_begin_=v.host_us;
        stalled_=true;last_report_=now;++reports_;
        char line[1000];std::snprintf(line,sizeof(line),"DIAGWATCH event=HOST_TURN_STALLED session=%llu launch=%llu mono_us=%lld host_age_us=%lld point_age_us=%lld phase=%s method=%s caller=%s bci=%d depth=%d last_key=%d release=%d input_age_us=%lld sample_is_last_published=1 root_cause=unattributed logger_thread=1",(unsigned long long)v.session,(unsigned long long)v.launch,(long long)now,(long long)age,(long long)(v.point_us?std::max<int64_t>(0,now-v.point_us):-1),phase_name(v.phase),labels.get(v.method),labels.get(v.caller),v.bci,v.depth,v.key,int(v.release),(long long)(v.input_us?std::max<int64_t>(0,now-v.input_us):-1));sink(line);
    }
};
struct Window {
    int64_t duration_us=0,worker_cpu_ns=-1;uint64_t draws=0,changes=0,presents=0,max_draw_interval_us=0;
    uint64_t budget_frames=0;int cap=0;bool active=false,paused=false,budget_valid=false;
};
class WindowDetector {
    int cap_=-1;unsigned warm_=0,short_=0;int64_t demand_us_=0;
public:
    static constexpr unsigned LongGap=1,CapShortfall=2,NoProgress=4;
    void reset(){cap_=-1;warm_=short_=0;demand_us_=0;}
    unsigned observe(int64_t now,const Window& v){
        if(!v.active||v.paused||v.duration_us<=0){reset();return 0;}
        if(v.cap!=cap_){reset();cap_=v.cap;}
        if(v.draws>=5&&v.changes>=3)demand_us_=now;
        const bool demand=demand_us_>0&&now>=demand_us_&&now-demand_us_<=3000000;
        if(warm_<2){++warm_;return 0;}
        unsigned flags=0;
        if(v.max_draw_interval_us>=250000&&demand)flags|=LongGap;
        const bool busy=v.worker_cpu_ns>=0&&v.worker_cpu_ns/1000>v.duration_us/4;
        if(!v.draws&&demand&&busy&&v.duration_us>=1000000)flags|=NoProgress;
        const auto paced=v.budget_valid?v.budget_frames:v.draws;
        const double rate=double(paced)*1000000.0/double(v.duration_us);
        const bool shortfall=(v.cap==30||v.cap==60)&&v.changes>=3&&demand&&rate<double(v.cap)*0.75;
        short_=shortfall?short_+1:0;
        if(shortfall&&(rate<double(v.cap)*0.5||short_>=2))flags|=CapShortfall;
        return flags;
    }
};
class IncidentGate {
    std::array<int64_t,9> last_{};std::array<uint64_t,9> count_{};
public:
    bool allow(size_t kind,int64_t now,bool force=false){
        kind=std::min<size_t>(8,kind);++count_[kind];
        if(!force&&last_[kind]&&now-last_[kind]<10000000)return false;
        last_[kind]=std::max<int64_t>(1,now);return true;
    }
    uint64_t count(size_t kind)const{return count_[std::min<size_t>(8,kind)];}
};
struct AudioSnapshot {
    uint64_t request_us=0,service_us=0,queued=0,pumps=0,empty_polls=0,blocks=0,nonzero_blocks=0,errors=0,max_gap_us=0,max_mix_us=0;
};
class AudioLive {
    std::atomic<uint64_t> sequence_{0};std::array<std::atomic<uint64_t>,10> words_{};
public:
    // Serialized by existing audio sink ownership, never by a diagnostic lock.
    void publish(const AudioSnapshot& v){
        sequence_.fetch_add(1,std::memory_order_acq_rel);
        const uint64_t a[]={v.request_us,v.service_us,v.queued,v.pumps,v.empty_polls,v.blocks,v.nonzero_blocks,v.errors,v.max_gap_us,v.max_mix_us};
        for(size_t n=0;n<10;++n)words_[n].store(a[n],std::memory_order_relaxed);
        sequence_.fetch_add(1,std::memory_order_release);
    }
    bool read(AudioSnapshot& v)const{
        for(unsigned attempt=0;attempt<4;++attempt){const auto first=sequence_.load(std::memory_order_acquire);if(first&1)continue;
            uint64_t a[10];for(size_t n=0;n<10;++n)a[n]=words_[n].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);if(first!=sequence_.load(std::memory_order_acquire))continue;
            v={a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]};return true;
        }return false;
    }
};
} // namespace runtime_diag
