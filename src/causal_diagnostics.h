#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

// Single VM owner only. Copied immutable label IDs/weak handle generations,
// no Java roots, clocks, allocation, disk I/O or scheduling in these counters.
namespace runtime_cause {
struct AllocationSite {
    bool used=false;uint32_t method=0;int bci=-1;char kind='?';
    uint64_t calls=0,bytes=0,min_length=0,max_length=0;
};
struct Allocations {
    static constexpr size_t Capacity=64,ProbeLimit=8;
    std::array<AllocationSite,Capacity> rows{};
    uint64_t calls=0,bytes=0,overflow_calls=0,overflow_bytes=0;
    void clear(){rows={};calls=bytes=overflow_calls=overflow_bytes=0;}
    void note(uint32_t method,int bci,char kind,uint64_t length,uint64_t payload){
        ++calls;bytes+=payload;
        const uint32_t hash=method*2654435761u^uint32_t(bci)*2246822519u^uint32_t(uint8_t(kind))*3266489917u;
        for(size_t probe=0;probe<ProbeLimit;++probe){auto& r=rows[(hash+probe)&(Capacity-1)];
            if(!r.used){r.used=true;r.method=method;r.bci=bci;r.kind=kind;r.min_length=r.max_length=length;}
            if(r.method==method&&r.bci==bci&&r.kind==kind){++r.calls;r.bytes+=payload;r.min_length=std::min(r.min_length,length);r.max_length=std::max(r.max_length,length);return;}
        }
        ++overflow_calls;overflow_bytes+=payload;
    }
};
enum class Route:uint8_t {Unpaced,Frame,Checked,Cached,Residual,LongUnpaced,Join,Virtual,EventFrame};
inline const char* route_name(Route route){
    switch(route){case Route::Frame:return "frame_owned";case Route::Checked:return "checked_deadline";case Route::Cached:return "cached_elapsed";
    case Route::EventFrame:return "event_frame";
    case Route::Residual:return "residual";case Route::LongUnpaced:return "outside_frame_range";case Route::Join:return "join";case Route::Virtual:return "virtual_test";default:return "unchanged_unowned";}
}
enum class Status:uint8_t {Completed,Canceled,StaleGeneration,Superseded};
enum class CancelReason:uint8_t {None,Manual,Reset,Terminated,Interrupted,Generation,Superseded};
inline const char* cancel_name(CancelReason r){switch(r){case CancelReason::Reset:return "lifecycle_reset";case CancelReason::Terminated:return "thread_terminated";case CancelReason::Interrupted:return "thread_interrupted";case CancelReason::Generation:return "handle_generation_changed";case CancelReason::Superseded:return "new_request_replaced_pending";case CancelReason::Manual:return "canceled";default:return "none";}}
inline const char* status_name(Status s){switch(s){case Status::Completed:return "resumed";case Status::StaleGeneration:return "stale_generation";case Status::Superseded:return "superseded";default:return "canceled";}}
struct SleepRequest {
    uint64_t session=0,launch=0;
    int thread=0;uint32_t generation=0,method=0;int bci=-1,cap=0;
    int64_t started_us=0,requested_ms=0,effective_ms=0,deadline_ms=0,deadline_us=0;
    Route route=Route::Unpaced;bool owned_before=false,paced_after=false,virtual_clock=false;
    double clock_scale=1.0;uint64_t draw=0;
    std::array<uint32_t,4> callers{};std::array<int,4> caller_bcis{{-1,-1,-1,-1}};
};
struct SleepCompletion {SleepRequest request;uint64_t id=0,sequence=0;int64_t ready_us=0,dispatched_us=0;Status status=Status::Canceled;CancelReason cancel_reason=CancelReason::None;};
class Sleeps {
    struct Pending {SleepRequest request;uint64_t id=0;int64_t ready_us=0;bool used=false;};
    std::array<Pending,64> pending_{};uint64_t next_id_=1;
    void finish(Pending& p,int64_t now,Status status,CancelReason reason=CancelReason::None){
        auto& c=history[size_t(completions%history.size())];c={p.request,p.id,++completions,p.ready_us,now,status,reason};
        if(status==Status::Completed){++normal_completed;++window_completed;if(now>=p.request.started_us)window_resume_us+=uint64_t(now-p.request.started_us);}
        else ++canceled;
        p.used=false;
    }
    Pending* find(int thread,uint32_t generation,int64_t now){
        for(auto& p:pending_)if(p.used&&p.request.thread==thread){
            if(p.request.generation==generation)return &p;
            finish(p,now,Status::StaleGeneration,CancelReason::Generation);return nullptr;
        }return nullptr;
    }
public:
    std::array<SleepCompletion,128> history{};
    uint64_t completions=0,normal_completed=0,canceled=0,overflow=0;
    uint64_t window_requests=0,window_requested_ms=0,window_effective_ms=0,window_completed=0,window_resume_us=0;
    uint64_t begin(const SleepRequest& request){
        ++window_requests;window_requested_ms+=uint64_t(std::max<int64_t>(0,request.requested_ms));window_effective_ms+=uint64_t(std::max<int64_t>(0,request.effective_ms));
        if(request.thread<=0){++overflow;return 0;}
        for(auto& p:pending_)if(p.used&&p.request.thread==request.thread)finish(p,request.started_us,Status::Superseded,CancelReason::Superseded);
        for(auto& p:pending_)if(!p.used){p={request,next_id_++,0,true};return p.id;}
        ++overflow;return 0;
    }
    bool ready(int thread,uint32_t generation,int64_t now){auto* p=find(thread,generation,now);if(!p)return false;if(!p->ready_us)p->ready_us=now;return true;}
    bool dispatch(int thread,uint32_t generation,int64_t now){auto* p=find(thread,generation,now);if(!p)return false;if(!p->ready_us)p->ready_us=now;finish(*p,now,Status::Completed);return true;}
    void cancel(int thread,uint32_t generation,int64_t now,CancelReason reason=CancelReason::Manual){auto* p=find(thread,generation,now);if(p)finish(*p,now,Status::Canceled,reason);}
    void cancel_all(int64_t now){for(auto& p:pending_)if(p.used)finish(p,now,Status::Canceled,CancelReason::Reset);}
    size_t pending()const{size_t count=0;for(const auto& p:pending_)count+=p.used;return count;}
    uint64_t history_overwritten()const{return completions>history.size()?completions-history.size():0;}
    void clear_window(){window_requests=window_requested_ms=window_effective_ms=window_completed=window_resume_us=0;}
};
} // namespace runtime_cause
