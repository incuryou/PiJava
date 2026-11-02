// P7.2 (0.2.66-p7.6): background worker base and per-thread timing counters.
//
// Worker pool: OFF by default. Switch: sdmc:/switch/j2me/p7-worker.enable ; desktop: J2ME_P7_WORKER=1.
//   Switch workers are libnx threads pinned to cores 1 and 2 (threadCreate cpuid); desktop uses std::thread.
//   API: submit / wait / drain / epoch_clear / shutdown. P7.2 moves NO work onto the workers: the only task ever
//   submitted is the startup self-test that records which core each worker runs on. Tasks are never serialized,
//   never touch the Java heap, and are drained before any J2S5 save/restore and cleared on every heap epoch change.
//   The VM builds with -fno-exceptions: misuse (submit while disabled) is a logged fatal abort, never ignored.
//
// Counters: always compiled in. Exclusive (self) time per category per host thread, accumulated with one tick read
//   per scope boundary; reported only to the log ("P7COUNT ...") at game stop/reset. They never feed back into
//   scheduling, pacing, draw counts or any game-visible state.
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <cstdlib>
#include <string>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <chrono>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sched.h>
#include <pthread.h>
#include <time.h>
#endif
#endif

namespace p7 {

// ---------------------------------------------------------------- counters
enum Cat : int { Interp = 0, AudioMix, AudioOut, AudioWait, Gc, GcWait, Decode, Present, PresentWait, Idle, Worker, CatCount };
inline const char* cat_name(int c) {
    static const char* n[CatCount] = {"java_exec", "audio_mix_midi", "audio_out", "audio_wait", "gc", "gc_wait", "decode", "present_blit", "present_wait", "idle_sleep", "worker_task"};
    return n[c];
}
constexpr int RenderSlot = 3;
constexpr int MaxSlots = 4; // logical roles, NOT CPU ids: main, worker1, worker2, independent renderer
inline const char* slot_name(int slot) {
    static const char* names[MaxSlots] = {"main", "w1", "w2", "render"};
    return names[slot];
}

inline uint64_t ticks() {
#ifdef __SWITCH__
    return armGetSystemTick();
#else
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
inline uint64_t ticks_to_us(uint64_t t) {
#ifdef __SWITCH__
    return armTicksToNs(t) / 1000;
#else
    return t / 1000;
#endif
}

struct Slot {
    std::atomic<uint64_t> t[CatCount];
    std::atomic<uint64_t> n[CatCount];
    // single writer (owning thread); relaxed load+store keeps the hot path free of RMW atomics and race-free for readers
    void add(int c, uint64_t d) { t[c].store(t[c].load(std::memory_order_relaxed) + d, std::memory_order_relaxed); }
    void hit(int c) { n[c].store(n[c].load(std::memory_order_relaxed) + 1, std::memory_order_relaxed); }
};
inline Slot g_slots[MaxSlots] = {};
inline std::atomic<uint64_t> g_window_start{0};
inline bool g_scope_on = false; // P8.2: scopes tick when P7COUNT or P8FRAME is on
inline bool g_count_on = false; // P7.3: counters default OFF (p7-count.enable / J2ME_P7_COUNT=1); set once at startup
inline thread_local int t_slot = 0;
// Registration belongs to the measured thread. Publish a complete clock under the same mutex used
// by readers and retirement, so a sampler can never read an unpublished or already closed handle.
// Retain retired CPU totals: summaries after owner/pool shutdown must still include their work.
struct ThreadClocks {
    std::mutex mutex;
    bool active[MaxSlots] = {};
    uint64_t origin_us[MaxSlots] = {}, retired_us[MaxSlots] = {};
#ifdef __SWITCH__
    Handle handle[MaxSlots] = {}; // borrowed real Thread handles; unregister before threadClose
#elif defined(_WIN32)
    HANDLE handle[MaxSlots] = {}; // owned duplicates, never GetCurrentThread pseudo handles
    ~ThreadClocks() { for (auto h : handle) if (h) CloseHandle(h); }
#else
    clockid_t clock[MaxSlots] = {};
#endif
};
inline ThreadClocks g_thread_clocks;
inline uint64_t g_cpu_start_us[MaxSlots] = {};
inline thread_local int t_registered_slot = -1;
inline thread_local int t_cur = -1;
inline thread_local uint64_t t_mark = 0;

// mutex must be held; all returned CPU values use microseconds, independent of wall ticks.
inline uint64_t live_thread_cpu_us(int slot) {
#ifdef __SWITCH__
    u64 value = 0;
    if (R_FAILED(svcGetInfo(&value, InfoType_ThreadTickCount, g_thread_clocks.handle[slot], -1))) return 0;
    return armTicksToNs(value) / 1000;
#elif defined(_WIN32)
    FILETIME create{}, exit{}, kernel{}, user{};
    if (!GetThreadTimes(g_thread_clocks.handle[slot], &create, &exit, &kernel, &user)) return 0;
    const uint64_t k = (uint64_t(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;
    const uint64_t u = (uint64_t(user.dwHighDateTime) << 32) | user.dwLowDateTime;
    return (k + u) / 10; // FILETIME counts 100 ns units
#else
    timespec ts{};
    if (clock_gettime(g_thread_clocks.clock[slot], &ts) != 0) return 0;
    return uint64_t(ts.tv_sec) * 1000000ull + uint64_t(ts.tv_nsec) / 1000ull;
#endif
}

// Must run before the first P7_SCOPE, including owner init, and finish before publishing readiness.
// Switch callers supply their real Thread.handle; a pseudo handle would sample the reader instead.
inline bool register_thread_clock(int slot
#ifdef __SWITCH__
    , Handle handle
#endif
) {
    if (slot < 0 || slot >= MaxSlots || t_cur >= 0) return false;
    if (t_registered_slot == slot) return true;
    if (t_registered_slot >= 0) return false;
    std::lock_guard<std::mutex> lock(g_thread_clocks.mutex);
    if (g_thread_clocks.active[slot]) return false; // preserve the slot's single writer
#ifdef __SWITCH__
    if (!handle || handle == CUR_THREAD_HANDLE) return false;
    g_thread_clocks.handle[slot] = handle;
#elif defined(_WIN32)
    HANDLE handle = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle,
                         THREAD_QUERY_INFORMATION, FALSE, 0)) return false;
    g_thread_clocks.handle[slot] = handle;
#else
    if (pthread_getcpuclockid(pthread_self(), &g_thread_clocks.clock[slot]) != 0) return false;
#endif
    g_thread_clocks.origin_us[slot] = live_thread_cpu_us(slot);
    g_thread_clocks.active[slot] = true;
    t_slot = t_registered_slot = slot;
    return true;
}

// On the owning thread, after final callbacks and after every P7_SCOPE has unwound, before exit.
inline void unregister_thread_clock(int slot) {
    if (slot < 0 || slot >= MaxSlots || t_registered_slot != slot || t_cur >= 0) return;
    std::lock_guard<std::mutex> lock(g_thread_clocks.mutex);
    const uint64_t value = live_thread_cpu_us(slot), origin = g_thread_clocks.origin_us[slot];
    if (value > origin) g_thread_clocks.retired_us[slot] += value - origin;
    g_thread_clocks.active[slot] = false;
#if defined(_WIN32) && !defined(__SWITCH__)
    CloseHandle(g_thread_clocks.handle[slot]);
    g_thread_clocks.handle[slot] = nullptr;
#elif defined(__SWITCH__)
    g_thread_clocks.handle[slot] = 0;
#endif
    t_registered_slot = -1;
}
inline uint64_t thread_cpu_us(int slot) {
    if (slot < 0 || slot >= MaxSlots) return 0;
    std::lock_guard<std::mutex> lock(g_thread_clocks.mutex);
    uint64_t result = g_thread_clocks.retired_us[slot];
    if (g_thread_clocks.active[slot]) {
        const uint64_t value = live_thread_cpu_us(slot), origin = g_thread_clocks.origin_us[slot];
        if (value > origin) result += value - origin;
    }
    return result;
}
struct Scope {
    int prev;
    // Same-category nesting (run() inside thread_resume(), nested run()) is already being timed by the outer
    // scope: skip all bookkeeping, no tick read. prev == -2 marks such a pass-through scope.
    explicit Scope(int c) {
        if (!g_scope_on || t_cur == c) { prev = -2; return; }
        const uint64_t now = ticks();
        Slot& s = g_slots[t_slot];
        if (t_cur >= 0) s.add(t_cur, now - t_mark);
        prev = t_cur; t_cur = c; t_mark = now; s.hit(c);
    }
    ~Scope() {
        if (prev == -2) return;
        const uint64_t now = ticks();
        g_slots[t_slot].add(t_cur, now - t_mark);
        t_cur = prev; t_mark = now;
    }
    Scope(const Scope&) = delete; Scope& operator=(const Scope&) = delete;
};

inline void counters_reset() {
    if (!g_count_on) return; // window stays 0: no P7COUNT line when counters are off
    for (int s = 0; s < MaxSlots; ++s) g_cpu_start_us[s] = thread_cpu_us(s);
    for (auto& s : g_slots) for (int c = 0; c < CatCount; ++c) { s.t[c].store(0, std::memory_order_relaxed); s.n[c].store(0, std::memory_order_relaxed); }
    // an open main-thread scope keeps accumulating from now on
    if (t_cur >= 0) t_mark = ticks();
    g_window_start.store(ticks(), std::memory_order_relaxed);
}

// One log line; shares are of the main-thread wall window since the last reset.
inline std::string counters_summary(const char* reason) {
    const uint64_t start = g_window_start.load(std::memory_order_relaxed);
    const uint64_t now = ticks();
    if (t_cur >= 0) { g_slots[t_slot].add(t_cur, now - t_mark); t_mark = now; } // flush the open scope
    const uint64_t wall = start ? ticks_to_us(now - start) : 0;
    char buf[160];
    std::snprintf(buf, sizeof buf, "P7COUNT schema=2 reason=%s wall_us=%llu", reason, (unsigned long long)wall);
    std::string out = buf;
    uint64_t main_sum = 0;
    for (int c = 0; c < CatCount; ++c) {
        const uint64_t us = ticks_to_us(g_slots[0].t[c].load(std::memory_order_relaxed));
        main_sum += us;
        const unsigned long long pm = wall ? (unsigned long long)(us * 1000ull / wall) : 0ull;
        std::snprintf(buf, sizeof buf, " %s=%lluus/%llu/%llu.%llu%%", cat_name(c), (unsigned long long)us,
                      (unsigned long long)g_slots[0].n[c].load(std::memory_order_relaxed), pm / 10, pm % 10);
        out += buf;
    }
    const uint64_t other = wall > main_sum ? wall - main_sum : 0;
    std::snprintf(buf, sizeof buf, " host_other=%lluus", (unsigned long long)other); out += buf;
    // Keep schema=2 and existing field order. Renderer fields are appended; cpu_total includes all roles.
    uint64_t total = 0, cpu_us[MaxSlots] = {};
    for (int s = 0; s < MaxSlots; ++s) {
        const uint64_t now_cpu = thread_cpu_us(s), cpu = now_cpu > g_cpu_start_us[s] ? now_cpu - g_cpu_start_us[s] : 0;
        total += cpu; cpu_us[s] = cpu;
        if (s == RenderSlot) continue;
        const unsigned long long pm = wall ? (unsigned long long)(cpu * 1000ull / wall) : 0ull;
        std::snprintf(buf, sizeof buf, " cpu_%s=%lluus/%llu.%llu%%", slot_name(s), (unsigned long long)cpu, pm / 10, pm % 10);
        out += buf;
    }
    std::snprintf(buf, sizeof buf, " cpu_total=%lluus", (unsigned long long)total); out += buf;
    for (int s = 1; s < MaxSlots; ++s) {
        if (s == RenderSlot) {
            const uint64_t cpu = cpu_us[s];
            const unsigned long long pm = wall ? (unsigned long long)(cpu * 1000ull / wall) : 0ull;
            std::snprintf(buf, sizeof buf, " cpu_render=%lluus/%llu.%llu%%", (unsigned long long)cpu, pm / 10, pm % 10);
            out += buf;
        }
        for (int c = 0; c < CatCount; ++c) {
            const uint64_t n = g_slots[s].n[c].load(std::memory_order_relaxed);
            if (!n && (s == RenderSlot || c != Worker)) continue;
            std::snprintf(buf, sizeof buf, " %s_%s=%lluus/%llu", slot_name(s), cat_name(c),
                          (unsigned long long)ticks_to_us(g_slots[s].t[c].load(std::memory_order_relaxed)), (unsigned long long)n);
            out += buf;
        }
    }
    return out;
}

inline int current_core() {
#ifdef __SWITCH__
    return (int)svcGetCurrentProcessorNumber();
#elif defined(_WIN32)
    return (int)GetCurrentProcessorNumber();
#else
    return sched_getcpu();
#endif
}

// ---------------------------------------------------------------- worker pool
class Pool {
public:
    using Task = std::function<void()>;
    static constexpr int Workers = 2;

    Pool() = default;
    // Process teardown without an explicit shutdown() (tests, abnormal host exit paths) must not leave workers
    // blocked on their condition variables: glibc's condition-variable destructor would wait for them forever.
    ~Pool() { shutdown(); }
    Pool(const Pool&) = delete; Pool& operator=(const Pool&) = delete;
    bool enabled() const { return enabled_; }
    const std::string& status() const { return status_; }

    // Starts workers (cores 1 and 2 on Switch). Returns false (pool stays disabled) if the platform refuses a thread;
    // the reason is kept in status() and logged by the caller - nothing is silently ignored.
    bool start() {
        if (enabled_) return true;
        stop_ = false; started_ = 0;
        for (int i = 0; i < Workers; ++i) {
            W& w = w_[i]; w.pool = this; w.index = i;
#ifdef __SWITCH__
            Result rc = threadCreate(&w.thread, &Pool::entry, &w, nullptr, 0x10000, 0x2C, i + 1);
            if (R_FAILED(rc)) { status_ = "threadCreate_failed core=" + std::to_string(i + 1) + " rc=" + std::to_string(rc); break; }
            rc = threadStart(&w.thread);
            if (R_FAILED(rc)) { threadClose(&w.thread); status_ = "threadStart_failed core=" + std::to_string(i + 1) + " rc=" + std::to_string(rc); break; }
#else
            w.thread = std::thread(&Pool::entry, &w); // -fno-exceptions: a failed create terminates loudly
#endif
            ++started_;
        }
        if (started_ != Workers) { join_all(); return false; }
        enabled_ = true; status_ = "running";
        return true;
    }

    // Queue a task on worker `which` (-1: round robin). Returns a ticket for wait(). Pool must be enabled.
    uint64_t submit(Task fn, int which = -1) {
        if (!enabled_) { std::fprintf(stderr, "P7WORKER fatal submit_while_disabled\n"); std::abort(); }
        std::unique_lock<std::mutex> lk(m_);
        if (which < 0) which = int(rr_++ % Workers);
        const uint64_t ticket = ++next_ticket_;
        w_[which].q.push_back(Item{ticket, epoch_, std::move(fn)});
        ++pending_;
        lk.unlock(); w_[which].cv.notify_one(); // only the target worker wakes
        return ticket;
    }

    // Wait until `ticket` finished (or was dropped by epoch_clear).
    void wait(uint64_t ticket) {
        std::unique_lock<std::mutex> lk(m_);
        cv_done_.wait(lk, [&] { return done_through(ticket); });
    }

    // If `ticket` is still queued (no worker has started it), remove it and hand it to the caller to run inline.
    // Returns false if it is running or finished. Used by the P7.3 audio fence: running a not-yet-started job on the
    // fencing thread keeps the exact operation order and avoids waiting for a worker wake-up.
    bool steal(uint64_t ticket, Task& out) {
        std::lock_guard<std::mutex> lk(m_);
        for (auto& w : w_)
            for (auto it = w.q.begin(); it != w.q.end(); ++it)
                if (it->ticket == ticket) { out = std::move(it->fn); w.q.erase(it); --pending_; ++steals_; cv_done_.notify_all(); return true; }
        return false;
    }
    uint64_t steals() const { return steals_; }
    // Wait until every queued and running task is finished.
    void drain() {
        if (!enabled_) return;
        std::unique_lock<std::mutex> lk(m_);
        cv_done_.wait(lk, [&] { return pending_ == 0; });
        ++drains_;
    }

    // Heap epoch changed (launch / suite switch / snapshot restore): drop queued tasks of older epochs, wait for the
    // running ones, then adopt the new epoch. Returns the number of tasks dropped.
    size_t epoch_clear(uint32_t new_epoch) {
        if (!enabled_) { epoch_ = new_epoch; return 0; }
        std::unique_lock<std::mutex> lk(m_);
        size_t dropped = 0;
        for (auto& w : w_) {
            for (auto it = w.q.begin(); it != w.q.end();) {
                if (it->epoch != new_epoch) { it = w.q.erase(it); ++dropped; --pending_; }
                else ++it;
            }
        }
        epoch_ = new_epoch;
        cv_done_.notify_all();
        cv_done_.wait(lk, [&] { return running_ == 0; });
        ++epoch_clears_;
        return dropped;
    }

    void shutdown() {
        if (!enabled_) return;
        drain();
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
        for (auto& w : w_) w.cv.notify_all();
        join_all();
        enabled_ = false; status_ = "stopped";
    }

    int core_of(int i) const { return w_[i].core.load(std::memory_order_acquire); }
    uint64_t drains() const { return drains_; }
    uint64_t epoch_clears() const { return epoch_clears_; }

private:
    struct Item { uint64_t ticket; uint32_t epoch; Task fn; };
    struct W {
        Pool* pool = nullptr; int index = 0; std::deque<Item> q; std::atomic<int> core{-1}; std::condition_variable cv;
#ifdef __SWITCH__
        Thread thread{};
#else
        std::thread thread;
#endif
    };
    bool done_through(uint64_t ticket) const {
        for (auto& w : w_) for (auto& it : w.q) if (it.ticket == ticket) return false;
        for (uint64_t r : running_tickets_) if (r == ticket) return false;
        return true;
    }
#ifdef __SWITCH__
    static void entry(void* arg) { static_cast<W*>(arg)->pool->loop(*static_cast<W*>(arg)); }
#else
    static void entry(W* w) { w->pool->loop(*w); }
#endif
    void loop(W& w) {
        t_slot = 1 + w.index; // retain correct scope role even if platform CPU clock acquisition fails
        register_thread_clock(t_slot
#ifdef __SWITCH__
            , w.thread.handle
#endif
        );
        w.core.store(current_core(), std::memory_order_release);
        std::unique_lock<std::mutex> lk(m_);
        for (;;) {
            w.cv.wait(lk, [&] { return stop_ || !w.q.empty(); });
            if (w.q.empty()) { if (stop_) { unregister_thread_clock(1 + w.index); return; } continue; }
            Item item = std::move(w.q.front()); w.q.pop_front();
            ++running_; running_tickets_.push_back(item.ticket);
            lk.unlock();
            { Scope s(Worker); item.fn(); }
            lk.lock();
            --running_; --pending_;
            for (auto it = running_tickets_.begin(); it != running_tickets_.end(); ++it) if (*it == item.ticket) { running_tickets_.erase(it); break; }
            cv_done_.notify_all();
        }
    }
    void join_all() {
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
        for (auto& w : w_) w.cv.notify_all();
        for (int i = 0; i < started_; ++i) {
#ifdef __SWITCH__
            threadWaitForExit(&w_[i].thread); threadClose(&w_[i].thread);
#else
            if (w_[i].thread.joinable()) w_[i].thread.join();
#endif
        }
        started_ = 0;
    }

    W w_[Workers];
    std::mutex m_;
    std::condition_variable cv_done_;
    std::vector<uint64_t> running_tickets_;
    uint64_t next_ticket_ = 0, rr_ = 0, drains_ = 0, epoch_clears_ = 0, steals_ = 0;
    size_t pending_ = 0, running_ = 0;
    uint32_t epoch_ = 0;
    int started_ = 0;
    bool stop_ = false, enabled_ = false;
    std::string status_ = "off";
};

inline Pool& pool() { static Pool p; return p; }

// Startup: read the switch, start workers, run the core self-test. Returns the log line.
inline std::string configure(bool requested, uint32_t epoch) {
    register_thread_clock(0
#ifdef __SWITCH__
        , threadGetCurHandle()
#endif
    ); // main thread
    if (!requested) return "P7WORKER schema=1 requested=0 enabled=0 reason=disabled";
    Pool& p = pool();
    if (!p.start()) return "P7WORKER schema=1 requested=1 enabled=0 reason=" + p.status();
    p.epoch_clear(epoch);
    std::atomic<int> seen[Pool::Workers];
    uint64_t t[Pool::Workers];
    for (int i = 0; i < Pool::Workers; ++i) { seen[i].store(-1); t[i] = p.submit([&seen, i] { seen[i].store(current_core()); }, i); }
    for (int i = 0; i < Pool::Workers; ++i) p.wait(t[i]);
    std::string line = "P7WORKER schema=1 requested=1 enabled=1 workers=" + std::to_string(Pool::Workers) + " cores=";
    bool ok = true;
    for (int i = 0; i < Pool::Workers; ++i) {
        line += (i ? "," : "") + std::to_string(seen[i].load());
#ifdef __SWITCH__
        ok = ok && seen[i].load() == i + 1;
#else
        ok = ok && seen[i].load() >= 0;
#endif
    }
    line += std::string(" main_core=") + std::to_string(current_core()) + " selftest=" + (ok ? "pass" : "fail") + " moved_work=none";
    if (!ok) { p.shutdown(); line += " action=disabled_after_selftest_fail"; }
    return line;
}

} // namespace p7
#define P7_CAT2(a, b) a##b
#define P7_CAT(a, b) P7_CAT2(a, b)
#define P7_SCOPE(cat) ::p7::Scope P7_CAT(p7_scope_, __COUNTER__)(::p7::cat)
