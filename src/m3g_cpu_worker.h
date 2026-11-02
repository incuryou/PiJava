#pragma once
#include "m3g_raster.h"
#include "p7_worker.h"
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_set>

namespace m3g_raster {
enum class Code {Success,Unavailable,Failed,Cancelled};
struct Result {
    Code code=Code::Unavailable;Counts counts;std::string reason;
    int worker_slot=-1;uint64_t worker_cpu_us=0,worker_wall_us=0;
};
class RasterQueue {
    struct Budget {std::mutex mutex;size_t bytes=0,peak=0,jobs=0;};
public:
    static constexpr size_t MaxBytes=16*1024*1024,MaxJobs=2,MaxCommands=4096;
    struct Pending {
        std::mutex mutex;std::condition_variable wake;bool done=false;Result result;
        std::shared_ptr<Budget> budget;size_t reserved=0;
        ~Pending(){if(budget){std::lock_guard<std::mutex> lock(budget->mutex);budget->bytes-=reserved;--budget->jobs;}}
        void complete(Result value){std::lock_guard<std::mutex> lock(mutex);if(done)return;result=std::move(value);done=true;wake.notify_all();}
    };
    using Ticket=std::shared_ptr<Pending>;
private:
    struct Job {
        Ticket pending;std::shared_ptr<Surface> surface;std::vector<Command> commands;
        ~Job(){Result r;r.code=Code::Cancelled;r.reason="worker_retired_without_result";pending->complete(std::move(r));}
        void run(){
            Result r;r.code=Code::Success;r.worker_slot=p7::t_slot;
            const uint64_t began=p7::ticks(),cpu=p7::thread_cpu_us(p7::t_slot);
            for(const auto& command:commands){const auto c=draw_owned(command,*surface);r.counts.triangles+=c.triangles;r.counts.pixels+=c.pixels;if(!c.ok){r.code=Code::Failed;r.counts.ok=false;r.reason="raster_command";break;}}
            r.worker_wall_us=p7::ticks_to_us(p7::ticks()-began);const auto after=p7::thread_cpu_us(p7::t_slot);r.worker_cpu_us=after>=cpu?after-cpu:0;
            pending->complete(std::move(r));
        }
    };
    std::shared_ptr<Budget> budget_=std::make_shared<Budget>();
    static size_t reservation(const Surface& surface,const std::vector<Command>& commands){
        const uint64_t area=uint64_t(surface.width)*uint64_t(surface.height);
        if(surface.width<=0||surface.height<=0||surface.width>800||surface.height>800||area>409600||surface.colors.size()!=area||(!surface.depth.empty()&&surface.depth.size()!=area)||commands.empty()||commands.size()>MaxCommands)return 0;
        if(surface.colors.capacity()>MaxBytes/sizeof(uint32_t)||surface.depth.capacity()>MaxBytes/sizeof(int16_t)||commands.capacity()>MaxBytes/sizeof(Command))return 0;
        size_t bytes=sizeof(Job)+sizeof(Pending)+256+surface.colors.capacity()*sizeof(uint32_t)+surface.depth.capacity()*sizeof(int16_t)+commands.capacity()*sizeof(Command);
        if(bytes>MaxBytes)return 0;std::unordered_set<const void*> seen;
        for(const auto& q:commands){
            if(q.w!=surface.width||q.h!=surface.height||q.count<0||q.count>4||q.mode<64||q.mode>68||q.alphaFactor<0||q.alphaFactor>256||!std::isfinite(q.threshold)||!std::isfinite(q.factor)||!std::isfinite(q.units)||(q.depthEnabled&&surface.depth.size()!=area))return 0;
            for(int i=0;i<3;++i)if(!std::isfinite(q.x[i])||!std::isfinite(q.y[i])||!std::isfinite(q.z[i])||!std::isfinite(q.iw[i]))return 0;
            for(int i=0;i<q.count;++i){const auto& t=q.textures[size_t(i)];
                if(t.width<=0||t.height<=0||uint64_t(t.width)*uint64_t(t.height)>MaxBytes/sizeof(uint32_t)||!t.pixels||t.pixels->size()!=uint64_t(t.width)*uint64_t(t.height)||t.mode<224||t.mode>228||t.format<96||t.format>100)return 0;
                for(float uv:t.uv)if(!std::isfinite(uv))return 0;
                if(seen.insert(t.pixels.get()).second){const size_t n=t.pixels->capacity();if(MaxBytes-bytes<256)return 0;bytes+=256; // native vector/control block and dedup bookkeeping allowance
                    if(n>(MaxBytes-bytes)/sizeof(uint32_t))return 0;bytes+=n*sizeof(uint32_t);}
            }
        }
        return bytes;
    }
public:
    bool available()const{return p7::pool().enabled();}
    // Caller must not access surface while a ticket is pending. Same-surface
    // jobs use the same existing core2 worker's FIFO; no Java pointer escapes.
    Ticket submit(std::shared_ptr<Surface> surface,std::vector<Command>&& commands){
        if(!available()||!surface)return {};const size_t bytes=reservation(*surface,commands);if(!bytes)return {};
        {std::lock_guard<std::mutex> lock(budget_->mutex);if(budget_->jobs>=MaxJobs||bytes>MaxBytes-budget_->bytes)return {};budget_->bytes+=bytes;++budget_->jobs;budget_->peak=std::max(budget_->peak,budget_->bytes);}
        auto ticket=std::make_shared<Pending>();ticket->budget=budget_;ticket->reserved=bytes;
        auto job=std::make_shared<Job>();job->pending=ticket;job->surface=std::move(surface);job->commands=std::move(commands);
        p7::pool().submit([job]{job->run();},1);return ticket;
    }
    static Result wait(const Ticket& ticket){
        if(!ticket){Result r;r.reason="no_ticket";return r;}
        std::unique_lock<std::mutex> lock(ticket->mutex);ticket->wake.wait(lock,[&]{return ticket->done;});return ticket->result;
    }
    size_t held_bytes()const{std::lock_guard<std::mutex> lock(budget_->mutex);return budget_->bytes;}
    size_t peak_bytes()const{std::lock_guard<std::mutex> lock(budget_->mutex);return budget_->peak;}
};
} // namespace m3g_raster
