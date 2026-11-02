#pragma once
#include "../p7_worker.h"
#include <algorithm>
#include <memory>
#include <new>
#include <utility>
#if defined(_WIN32) && !defined(__SWITCH__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace render3d {
using Epoch = uint32_t;
using Sequence = uint64_t;

enum class PrepPriority : uint8_t { CurrentFrame, NextFrame, Prewarm };
enum class PrepSubmitCode { Accepted, Backpressure, StaleEpoch, Frozen, Disabled, Invalid, OutOfMemory };
struct PrepSubmitResult { PrepSubmitCode code; Sequence sequence = 0; };
enum class PrepResultCode { Succeeded, Failed, Exception };
enum class PrepDrainCode { Drained, DependenciesPending, StaleEpoch, DispatchFailed };
enum class PrepWaitCode { Taken, Cancelled, StaleEpoch, Invalid };
struct PrepLimits { size_t max_jobs = 32; size_t max_bytes = 8 * 1024 * 1024; };

struct PrepOutput { virtual ~PrepOutput() = default; };
class PrepCancel {
public:
    bool requested() const { return flag_.load(std::memory_order_acquire); }
private:
    friend class PrepQueue;
    explicit PrepCancel(const std::atomic<bool>& flag) : flag_(flag) {}
    const std::atomic<bool>& flag_;
};
// An implementation owns every input it accesses, including immutable native resource references.
// No live VM/Java pointers, callbacks into Java, waiting for a producer, or GPU calls are permitted.
// run() is bounded/cooperatively cancellable. In -fno-exceptions builds it reports errors as false.
struct PrepWork {
    virtual ~PrepWork() = default;
    virtual bool run(const PrepCancel&, std::unique_ptr<PrepOutput>&) = 0;
    // A successful slice may yield without an output. The same owned work, ticket and
    // reservation are retained until a final slice. Default keeps all existing jobs unchanged.
    virtual bool continues()const noexcept{return false;}
};
#define P7_PREP_CONTINUATIONS 1
struct PrepJob {
    Epoch epoch = 0;
    // Reservation covers owned input capacities + peak scratch + maximum output, including metadata.
    // This is a caller contract: the queue cannot inspect allocations inside a native decoder.
    size_t reserved_bytes = 0;
    PrepPriority priority = PrepPriority::NextFrame;
    bool ready = true;
    std::unique_ptr<PrepWork> work;
};
struct PrepResult {
    Epoch epoch = 0;
    Sequence sequence = 0;
    PrepResultCode code = PrepResultCode::Failed;
    std::unique_ptr<PrepOutput> output;
};
struct PrepMetrics {
    size_t held_jobs = 0, held_bytes = 0, peak_jobs = 0, peak_bytes = 0;
    size_t queued = 0, ready = 0, running = 0, completed = 0;
    uint64_t accepted = 0, backpressure = 0, rejected = 0;
    uint64_t succeeded = 0, failed = 0, exceptions = 0;
    uint64_t cancelled_queued = 0, cancelled_running = 0;
    uint64_t max_queue_us = 0, max_run_us = 0, run_us = 0, run_cpu_us = 0;
    uint64_t cpu_samples = 0, cpu_unavailable = 0;
    uint64_t slices=0,yields=0,total_queue_us=0,max_job_queue_us=0;
    uint64_t dispatcher_turns=0,dispatch_failures=0;
    uint64_t oldest_queue_us = 0;
};

// One admission domain over the EXISTING P7 worker1 (index 1 / Switch core 2).
// No new threads and no fallback/steal onto main or audio worker0. A dispatcher is the only P7
// item owned by this queue. Each turn runs one bounded slice, then submits at the SAME P7 FIFO
// tail so independent core2 clients can run. set_ready() restarts an idle dispatcher.
//
// Lifecycle owner must freeze/cancel/drain BEFORE P7 epoch_clear/shutdown; otherwise P7 could
// discard the dispatcher without invoking it. Pool startup/shutdown is externally serialized,
// exactly as for existing P7 clients. The queue must outlive dispatchers. Destructor closes it.
// Never invoke lifecycle/drain from a PrepWork: waiting for one's own dispatcher cannot complete.
// Disabled/thread-start-failed pools reject submissions with ownership retained for caller fallback.
// P7's existing host thread-creation failure terminates in -fno-exceptions; Switch reports disabled.
// Allocator exhaustion retains the existing program's fail-fast policy in -fno-exceptions builds.
class PrepQueue {
public:
    PrepQueue(p7::Pool& pool, PrepLimits limits, Epoch epoch) : pool_(pool), limits_(limits), epoch_(epoch) {
        completed_.reserve(limits.max_jobs); // no result-container allocation on the worker after admission
    }
    ~PrepQueue() { close(); }
    PrepQueue(const PrepQueue&) = delete;
    PrepQueue& operator=(const PrepQueue&) = delete;
    static bool current_thread_cpu_us(uint64_t& out){return current_cpu_us(out);}

    PrepSubmitResult try_submit(PrepJob&& job) {
        std::lock_guard<std::mutex> lock(m_);
        if (job.epoch != epoch_) return reject(PrepSubmitCode::StaleEpoch);
        if (frozen_ || closed_) return reject(PrepSubmitCode::Frozen);
        if (!pool_.enabled()) return reject(PrepSubmitCode::Disabled);
        if (!job.work || !job.reserved_bytes || unsigned(job.priority) > unsigned(PrepPriority::Prewarm)) return reject(PrepSubmitCode::Invalid);
        if (stats_.held_jobs >= limits_.max_jobs || job.reserved_bytes > limits_.max_bytes ||
            stats_.held_bytes > limits_.max_bytes - job.reserved_bytes) {
            ++stats_.backpressure; return {PrepSubmitCode::Backpressure, 0};
        }
        const Sequence sequence = ++next_;
#if defined(__cpp_exceptions)
        try {
#endif
            // Allocate the empty slot BEFORE moving caller ownership. A failed deque growth must
            // leave the original job available for the explicitly synchronous fallback.
            queued_.push_back(Item{sequence, p7::ticks(), PrepJob{}});
#if defined(__cpp_exceptions)
        } catch (const std::bad_alloc&) { return reject(PrepSubmitCode::OutOfMemory); }
#endif
        queued_.back().job = std::move(job);
        ++stats_.held_jobs; stats_.held_bytes += queued_.back().job.reserved_bytes;
        if (!kick_locked()) {
            job = std::move(queued_.back().job); queued_.pop_back(); release_locked(job.reserved_bytes);
            return reject(PrepSubmitCode::OutOfMemory);
        }
        stats_.peak_jobs = std::max(stats_.peak_jobs, stats_.held_jobs);
        stats_.peak_bytes = std::max(stats_.peak_bytes, stats_.held_bytes);
        ++stats_.accepted;
        return {PrepSubmitCode::Accepted, sequence};
    }
    bool set_ready(Sequence sequence, Epoch epoch) {
        std::lock_guard<std::mutex> lock(m_);
        if (epoch != epoch_ || closed_ || transitioning_) return false;
        for (auto& item : queued_) if (item.sequence == sequence) {
            item.job.ready = true; return kick_locked();
        }
        return false;
    }
    // Freeze admission only: already-admitted work may finish. This makes save/GC quiesce finite.
    bool freeze(Epoch epoch) {
        std::lock_guard<std::mutex> lock(m_);
        if (epoch != epoch_) return false;
        frozen_ = true; return true;
    }
    bool resume(Epoch epoch) {
        std::lock_guard<std::mutex> lock(m_);
        if (epoch != epoch_ || closed_ || transitioning_) return false;
        frozen_ = false; return true;
    }
    // Cancelled queued tasks have no result and immediately return their budget; the return count
    // tells the owner which queued results will never arrive. Running work is unaffected.
    size_t cancel_queued(Epoch epoch) {
        std::lock_guard<std::mutex> lock(m_);
        if (epoch != epoch_) return 0;
        return cancel_locked();
    }
    PrepDrainCode drain(Epoch epoch) {
        std::unique_lock<std::mutex> lock(m_);
        if (epoch != epoch_) return PrepDrainCode::StaleEpoch;
        if (!kick_locked()) return PrepDrainCode::DispatchFailed;
        done_.wait(lock, [&]{ return !dispatcher_ || epoch != epoch_; });
        if (epoch != epoch_) return PrepDrainCode::StaleEpoch;
        return queued_.empty() ? PrepDrainCode::Drained : PrepDrainCode::DependenciesPending;
    }
    // Explicit sequence retrieval leaves ordering with main/render owner. Never waits for a consumer.
    // Caller must reserve destination cache/owner budget before taking: the queue's reservation ends
    // at this ownership transfer; output retained by a consumer is no longer charged to this queue.
    bool try_take(Sequence sequence, PrepResult& out) {
        std::lock_guard<std::mutex> lock(m_);
        for (auto it = completed_.begin(); it != completed_.end(); ++it) if (it->result.sequence == sequence) {
            release_locked(it->bytes); out = std::move(it->result); completed_.erase(it); return true;
        }
        return false;
    }
    // Wait for exactly this ticket, not the dispatcher's remaining queue. A cancelled
    // queued job has no result; cancellation/epoch changes wake this waiter explicitly.
    // Caller must not hold a VM lock or call from a worker. Unlike drain this does not
    // require a frozen queue. The submitted job must be ready without a Java callback.
    PrepWaitCode wait_take(Sequence sequence, Epoch epoch, PrepResult& out) {
        std::unique_lock<std::mutex> lock(m_);
        if(epoch!=epoch_)return PrepWaitCode::StaleEpoch;
        if(!sequence||sequence>next_)return PrepWaitCode::Invalid;
        for(;;){
            if(epoch!=epoch_)return PrepWaitCode::StaleEpoch;
            for(auto it=completed_.begin();it!=completed_.end();++it)if(it->result.sequence==sequence){
                release_locked(it->bytes);out=std::move(it->result);completed_.erase(it);return PrepWaitCode::Taken;
            }
            bool live=running_&&running_sequence_==sequence;
            for(const auto& item:queued_)if(item.sequence==sequence){live=true;break;}
            if(!live)return PrepWaitCode::Cancelled;
            done_.wait(lock);
        }
    }
    // Serialize epoch/reset/close; cancel running output before waiting. run() observes PrepCancel,
    // or completes its bounded native operation. No lock is held across work execution or wait.
    void reset_epoch(Epoch epoch) {
        std::lock_guard<std::mutex> lifecycle(lifecycle_);
        std::unique_lock<std::mutex> lock(m_);
        if (closed_) return;
        transitioning_ = frozen_ = true; cancel_locked(); cancel_running_.store(true, std::memory_order_release);
        done_.wait(lock, [&]{ return !dispatcher_; });
        clear_completed_locked(); epoch_ = epoch; transitioning_ = frozen_ = false;
        done_.notify_all();
    }
    void close() {
        std::lock_guard<std::mutex> lifecycle(lifecycle_);
        std::unique_lock<std::mutex> lock(m_);
        closed_ = frozen_ = transitioning_ = true;
        cancel_locked(); cancel_running_.store(true, std::memory_order_release);
        done_.wait(lock, [&]{ return !dispatcher_; });
        clear_completed_locked();
    }
    PrepMetrics metrics() const {
        std::lock_guard<std::mutex> lock(m_);
        PrepMetrics out = stats_;
        out.running = running_ ? 1 : 0; out.queued = queued_.size(); out.completed = completed_.size();
        const uint64_t now = p7::ticks();
        for (const auto& item : queued_) {
            out.ready += item.job.ready ? 1 : 0;
            out.oldest_queue_us = std::max(out.oldest_queue_us, p7::ticks_to_us(now - item.enqueued));
        }
        return out;
    }
private:
    struct Item { Sequence sequence; uint64_t enqueued; PrepJob job; uint64_t waited_us=0; };
    struct Completed { size_t bytes; PrepResult result; };
    // Called only on the preparation worker, so no cross-thread clock registration is needed.
    // Do not add this inclusive work total to an exclusive stage breakdown of the same work.
    static bool current_cpu_us(uint64_t& out) {
#ifdef __SWITCH__
        u64 value = 0;
        if (R_FAILED(svcGetInfo(&value, InfoType_ThreadTickCount, CUR_THREAD_HANDLE, -1))) return false;
        out = armTicksToNs(value) / 1000; return true;
#elif defined(_WIN32)
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return false;
        const uint64_t k = (uint64_t(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;
        const uint64_t u = (uint64_t(user.dwHighDateTime) << 32) | user.dwLowDateTime;
        out = (k + u) / 10; return true;
#else
        timespec value{};
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value)) return false;
        out = uint64_t(value.tv_sec) * 1000000 + uint64_t(value.tv_nsec) / 1000; return true;
#endif
    }
    PrepSubmitResult reject(PrepSubmitCode code) { ++stats_.rejected; return {code, 0}; }
    void release_locked(size_t bytes) { --stats_.held_jobs; stats_.held_bytes -= bytes; }
    size_t cancel_locked() {
        const size_t count = queued_.size();
        for (auto& item : queued_) release_locked(item.job.reserved_bytes);
        queued_.clear(); stats_.cancelled_queued += count; done_.notify_all(); return count;
    }
    void clear_completed_locked() {
        for (auto& result : completed_) release_locked(result.bytes);
        completed_.clear();
    }
    // Waiting age promotes a job one level per 50ms, then FIFO at the same effective priority.
    // Only READY jobs participate; work never runs early to satisfy an unmet dependency.
    size_t next_ready_locked() const {
        size_t best = queued_.size(); unsigned best_priority = 3;
        const uint64_t now = p7::ticks();
        for (size_t i = 0; i < queued_.size(); ++i) {
            const auto& item = queued_[i]; if (!item.job.ready) continue;
            const uint64_t promotions = p7::ticks_to_us(now - item.enqueued) / 50000;
            const unsigned priority = promotions >= unsigned(item.job.priority) ? 0 : unsigned(item.job.priority) - unsigned(promotions);
            if (priority < best_priority) { best = i; best_priority = priority; }
        }
        return best;
    }
    bool kick_locked() {
        if (dispatcher_ || next_ready_locked() == queued_.size()) return true;
        dispatcher_ = true;
#if defined(__cpp_exceptions)
        try {
#endif
            pool_.submit([this]{ dispatch(); }, 1);
#if defined(__cpp_exceptions)
        } catch (const std::bad_alloc&) { dispatcher_ = false; return false; }
#endif
        return true;
    }
    // Keep dispatcher_ true across a successful handoff: drain/close must include the
    // queued P7 turn even though no PrepWork is currently running. Allocation failure
    // publishes terminal results for admitted jobs; no waiter is left awaiting a lost turn.
    void finish_turn_locked() {
        dispatcher_ = false;
        if (!kick_locked()) {
            ++stats_.dispatch_failures;
            for (auto& item : queued_) {
                item.job.work.reset();
                completed_.push_back(Completed{item.job.reserved_bytes,
                    PrepResult{item.job.epoch,item.sequence,PrepResultCode::Failed,{}}});
                ++stats_.failed;
            }
            queued_.clear();
        }
        done_.notify_all();
    }
    void dispatch() {
        std::unique_lock<std::mutex> lock(m_);
        ++stats_.dispatcher_turns;
        const size_t index = next_ready_locked();
        if (index == queued_.size()) { dispatcher_ = false; done_.notify_all(); return; }
        Item item = std::move(queued_[index]); queued_.erase(queued_.begin() + index);
        running_ = true; running_sequence_=item.sequence; cancel_running_.store(false, std::memory_order_release);
        const uint64_t started = p7::ticks();
        uint64_t cpu_started = 0; const bool cpu_start_ok = current_cpu_us(cpu_started);
        const uint64_t queued_us=p7::ticks_to_us(started-item.enqueued);item.waited_us+=queued_us;
        stats_.total_queue_us+=queued_us;stats_.max_job_queue_us=std::max(stats_.max_job_queue_us,item.waited_us);
        stats_.max_queue_us = std::max(stats_.max_queue_us,queued_us);
        lock.unlock();
        PrepResult result; result.epoch = item.job.epoch; result.sequence = item.sequence;
#if defined(__cpp_exceptions)
        try {
#endif
            result.code = item.job.work->run(PrepCancel(cancel_running_), result.output) ? PrepResultCode::Succeeded : PrepResultCode::Failed;
#if defined(__cpp_exceptions)
        } catch (...) { result.code = PrepResultCode::Exception; }
#endif
        const bool continuation=result.code==PrepResultCode::Succeeded&&item.job.work->continues();
        if(continuation&&result.output){result.code=PrepResultCode::Failed;result.output.reset();}
        if (result.code != PrepResultCode::Succeeded) result.output.reset();
        if(!continuation)item.job.work.reset(); // final inputs released before publishing/returning credits
        const uint64_t elapsed = p7::ticks_to_us(p7::ticks() - started);
        uint64_t cpu_ended = 0; const bool cpu_end_ok = current_cpu_us(cpu_ended);
        lock.lock(); running_ = false; running_sequence_=0;
        ++stats_.slices;
        stats_.max_run_us = std::max(stats_.max_run_us, elapsed); stats_.run_us += elapsed;
        if (cpu_start_ok && cpu_end_ok) {
            ++stats_.cpu_samples; stats_.run_cpu_us += cpu_ended > cpu_started ? cpu_ended - cpu_started : 0;
        } else ++stats_.cpu_unavailable;
        if(continuation&&result.code==PrepResultCode::Succeeded&&!cancel_running_.load(std::memory_order_acquire)){
            // Only waiting since the last granted slice earns aging priority. An unserved
            // background job still ages, but one old job cannot monopolize the highest level.
            item.enqueued=p7::ticks();
#if defined(__cpp_exceptions)
            try{
#endif
                queued_.push_back(std::move(item));++stats_.yields;finish_turn_locked();return;
#if defined(__cpp_exceptions)
            }catch(const std::bad_alloc&){result.code=PrepResultCode::Failed;}
#endif
        }
        // Cancellation/epoch/close never requeue old continuations. A host requeue allocation
        // failure becomes a terminal Failed result; -fno-exceptions keeps existing OOM policy.
        item.job.work.reset();
        if (cancel_running_.load(std::memory_order_acquire)) {
            result.output.reset(); release_locked(item.job.reserved_bytes); ++stats_.cancelled_running;
        } else {
            if (result.code == PrepResultCode::Succeeded) ++stats_.succeeded;
            else if (result.code == PrepResultCode::Exception) ++stats_.exceptions;
            else ++stats_.failed;
            completed_.push_back(Completed{item.job.reserved_bytes, std::move(result)});
        }
        finish_turn_locked();return;
    }
    p7::Pool& pool_;
    const PrepLimits limits_;
    mutable std::mutex m_;
    std::mutex lifecycle_;
    std::condition_variable done_;
    std::deque<Item> queued_;
    std::vector<Completed> completed_;
    PrepMetrics stats_;
    Epoch epoch_;
    Sequence next_ = 0;
    Sequence running_sequence_ = 0;
    bool frozen_ = false, closed_ = false, transitioning_ = false, dispatcher_ = false, running_ = false;
    std::atomic<bool> cancel_running_{false};
};
} // namespace render3d
