#pragma once
// One native renderer owner, not a task pool. Backend and every Frame payload
// must outlive/use only native owned data: no VM, Java, or borrowed canvas data.
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <pthread.h>
#endif

namespace render_owner {
enum class Status { Submitted, FenceCompleted, Cancelled, InitFailed, ProcessFailed, FinishFailed, ThreadFailed };
struct Completion {
    uint64_t sequence=0,epoch=0;
    Status status=Status::Submitted;
    std::string error;
    uint64_t acquire_wait_us=0,gpu_wait_us=0,upload_us=0,total_us=0;
    uint64_t frame_upload_bytes=0,chrome_upload_bytes=0;
};
struct Snapshot {
    uint64_t accepted=0,submitted=0,fence_completed=0,cancelled=0,failed=0;
    bool processing=false,pending=false,credit_reserved=false,accepting=false;
};

// Backend contract (all four methods run on the same owner thread):
//   using Frame = movable native payload with uint64_t sequence and epoch;
//   bool init(std::string& error);
//   bool process(const Frame&, Completion& result); // submitted, not displayed
//   bool finish(std::string& error); // wait for ALL previously submitted fences
//   void shutdown(); // including cleanup after partially unsuccessful init
// The Backend object must outlive Queue. Calls into it must not throw. Main is
// the single producer/control caller; stats/poll methods may run concurrently.
// A frame owns its snapshots after move; pointed-to data must be immutable and
// separately owned, e.g. shared_ptr<const Packet>, never borrowed Java pixels.
template<class Backend> class Queue {
public:
    using Frame=typename Backend::Frame;
private:
    Backend& backend_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::unique_ptr<Frame> pending_;
    bool started_=false,ever_started_=false,init_done_=false,init_ok_=false;
    bool closing_=false,paused_=false,failed_=false,processing_=false,credit_=false;
    uint64_t minimum_epoch_=0,last_sequence_=0,barrier_requested_=0,barrier_done_=0;
    Snapshot totals_;
    Completion latest_,first_error_,last_submitted_;
    bool has_latest_=false,has_error_=false,error_recorded_=false;
#ifdef __SWITCH__
    Thread thread_{};
    static void entry(void* self){static_cast<Queue*>(self)->run();}
#else
    pthread_t thread_{};
    static void* entry(void* self){static_cast<Queue*>(self)->run();return nullptr;}
#endif
    bool healthy_locked()const{return started_&&init_done_&&init_ok_&&!closing_&&!failed_;}
    bool accepting_locked()const{return healthy_locked()&&!paused_;}
    void publish_locked(Completion completion){latest_=std::move(completion);has_latest_=true;}
    void error_locked(Completion completion) {
        ++totals_.failed;failed_=true;credit_=false;
        // The first fatal error is sticky even when later success/cancellation
        // notifications coalesce. poll_error acknowledges it exactly once.
        if(!error_recorded_){first_error_=completion;has_error_=error_recorded_=true;}
        publish_locked(std::move(completion));wake_.notify_all();
    }
    void cancel_pending_locked() {
        if(!pending_)return;
        Completion completion;completion.sequence=pending_->sequence;completion.epoch=pending_->epoch;
        completion.status=Status::Cancelled;pending_.reset();++totals_.cancelled;
        publish_locked(std::move(completion));wake_.notify_all();
    }
    void run() {
        std::string error;
        const bool initialized=backend_.init(error);
        if(!initialized)backend_.shutdown();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            init_ok_=initialized;init_done_=true;
            if(!initialized){Completion c;c.status=Status::InitFailed;c.error=std::move(error);error_locked(std::move(c));}
            wake_.notify_all();
        }
        if(!initialized)return;
        for(;;) {
            std::unique_ptr<Frame> frame;
            uint64_t barrier=0;
            bool stopping=false;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock,[&]{return pending_||closing_||barrier_requested_!=barrier_done_;});
                if(pending_&&!failed_){frame=std::move(pending_);processing_=true;wake_.notify_all();}
                else {barrier=barrier_requested_;stopping=closing_;}
            }
            if(frame) {
                Completion completion;completion.sequence=frame->sequence;completion.epoch=frame->epoch;
                const bool ok=backend_.process(*frame,completion);
                // Sequence/epoch always identify THIS payload, even if backend
                // populated result fields from a recycled GPU staging slot.
                completion.sequence=frame->sequence;completion.epoch=frame->epoch;
                frame.reset();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    processing_=false;
                    if(ok){completion.status=Status::Submitted;++totals_.submitted;last_submitted_=completion;publish_locked(std::move(completion));}
                    else {
                        completion.status=Status::ProcessFailed;
                        cancel_pending_locked();error_locked(std::move(completion));
                    }
                    wake_.notify_all();
                }
                continue;
            }
            // Neither finish nor shutdown holds the queue mutex. finish is a
            // real resource barrier, deliberately NOT performed per submit.
            error.clear();const bool finished=backend_.finish(error);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if(finished) {
                    if(totals_.fence_completed<totals_.submitted){
                        Completion c=last_submitted_;c.status=Status::FenceCompleted;publish_locked(std::move(c));
                    }
                    totals_.fence_completed=totals_.submitted;
                } else {Completion c;c.status=Status::FinishFailed;c.error=std::move(error);error_locked(std::move(c));}
                barrier_done_=barrier;wake_.notify_all();
            }
            if(stopping)break;
        }
        backend_.shutdown();
    }
public:
    explicit Queue(Backend& backend):backend_(backend){}
    ~Queue(){close();}
    Queue(const Queue&)=delete;
    Queue& operator=(const Queue&)=delete;
    // init handshake completes BEFORE the caller may transfer a window or
    // depend on owner initialization. Failed init is shut down on owner first.
    // This object is single-use; create a fresh Queue for a later renderer.
    bool start() {
        std::unique_lock<std::mutex> lock(mutex_);
        if(ever_started_)return false;
        ever_started_=true;
#ifdef __SWITCH__
        Result result=threadCreate(&thread_,&Queue::entry,this,nullptr,256*1024,0x2d,1);
        if(R_SUCCEEDED(result)) {
            result=threadStart(&thread_);
            if(R_FAILED(result))threadClose(&thread_);
        }
        if(R_FAILED(result)) {
            Completion c;c.status=Status::ThreadFailed;c.error="render owner thread creation/start failed: "+std::to_string(result);
            error_locked(std::move(c));return false;
        }
#else
        const int result=pthread_create(&thread_,nullptr,&Queue::entry,this);
        if(result!=0){Completion c;c.status=Status::ThreadFailed;
            c.error="render owner pthread_create failed: "+std::to_string(result);error_locked(std::move(c));return false;}
#endif
        started_=true;
        wake_.wait(lock,[&]{return init_done_;});
        const bool ok=init_ok_;lock.unlock();
        if(!ok)close();
        return ok;
    }
    // Credit reserves the SINGLE pending slot before producer snapshot work.
    // It is not a GPU acquire. While N processes, producer may prepare N+1.
    bool acquire_credit() {
        std::unique_lock<std::mutex> lock(mutex_);
        if(!accepting_locked())return false;
        wake_.wait(lock,[&]{return !accepting_locked()||(!pending_&&!credit_);});
        if(!accepting_locked())return false;
        credit_=true;return true;
    }
    bool try_acquire_credit() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!accepting_locked()||pending_||credit_)return false;
        credit_=true;return true;
    }
    void release_credit(){std::lock_guard<std::mutex> lock(mutex_);credit_=false;wake_.notify_all();}
    bool submit(Frame&& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!credit_)return false;
        credit_=false;
        if(!accepting_locked()||pending_||frame.epoch<minimum_epoch_||frame.sequence<=last_sequence_){wake_.notify_all();return false;}
        pending_=std::make_unique<Frame>(std::move(frame));
        last_sequence_=pending_->sequence;++totals_.accepted;wake_.notify_all();return true;
    }
    // CPU frame drain plus a Backend::finish fence barrier on owner. Does not
    // establish display/scanout. The single producer must not submit in drain.
    bool drain() {
        std::unique_lock<std::mutex> lock(mutex_);
        if(!healthy_locked())return false;
        credit_=false;const uint64_t barrier=++barrier_requested_;wake_.notify_all();
        wake_.wait(lock,[&]{return barrier_done_>=barrier||failed_||closing_;});
        return !failed_&&!closing_&&barrier_done_>=barrier;
    }
    bool pause() {
        {std::lock_guard<std::mutex> lock(mutex_);paused_=true;credit_=false;wake_.notify_all();}
        return drain();
    }
    bool resume(uint64_t epoch) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!healthy_locked()||processing_||pending_)return false;
        if(epoch>minimum_epoch_)minimum_epoch_=epoch;
        paused_=false;wake_.notify_all();return true;
    }
    // Invalidates only not-yet-started frames. In-flight SDK operations CANNOT
    // be interrupted: GPU acquire/finish can delay pause/close indefinitely if
    // the real SDK blocks. Tests with gates do not prove SDK cancellation.
    void cancel_before_epoch(uint64_t epoch) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(epoch>minimum_epoch_)minimum_epoch_=epoch;
        credit_=false;
        if(pending_&&pending_->epoch<minimum_epoch_)cancel_pending_locked();
        wake_.notify_all();
    }
    // Close retires accepted frames, finishes GPU resources, then destroys all
    // backend resources on owner. Never join with mutex held. Control caller
    // must not call close concurrently with another lifecycle control call.
    void close() {
        {std::lock_guard<std::mutex> lock(mutex_);if(!started_)return;closing_=true;credit_=false;wake_.notify_all();}
#ifdef __SWITCH__
        threadWaitForExit(&thread_);threadClose(&thread_);
#else
        pthread_join(thread_,nullptr);
#endif
        std::lock_guard<std::mutex> lock(mutex_);started_=false;
    }
    // Fixed-size metadata mailbox: successes/cancellations may coalesce, but
    // submitted draws are never dropped and cumulative counters remain exact.
    // Fatal error has a separate sticky mailbox, so no polling cadence loses it.
    bool poll_completion(Completion& completion){std::lock_guard<std::mutex> lock(mutex_);if(!has_latest_)return false;completion=latest_;has_latest_=false;return true;}
    bool poll_error(Completion& completion){std::lock_guard<std::mutex> lock(mutex_);if(!has_error_)return false;completion=first_error_;has_error_=false;return true;}
    Snapshot stats()const {
        std::lock_guard<std::mutex> lock(mutex_);Snapshot result=totals_;
        result.processing=processing_;result.pending=bool(pending_);result.credit_reserved=credit_;result.accepting=accepting_locked();return result;
    }
};
}
