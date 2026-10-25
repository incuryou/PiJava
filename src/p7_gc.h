// P7.4: parallel GC mark on the P7.2 worker pool (switch p7-gc.enable / J2ME_P7_GC=1, requires p7-worker, default off).
// Only the TRACE runs on the workers: roots are enumerated and marked on the main thread, sweep / weak-index sweep /
// slot reuse / freelist order stay exactly as before on the main thread (index order). The mark set is a reachability
// closure, so it is independent of trace order; J2ME_P7_GC_VERIFY=1 (desktop) recomputes it serially and compares
// every slot. The main thread blocks (no spinning) while the workers trace.
#pragma once
#include "p7_worker.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>
namespace p7gc {
inline bool g_enabled = false, g_verify = false;
inline uint64_t g_parallel = 0, g_serial_small = 0, g_verify_runs = 0, g_verify_bad_slots = 0;
inline uint64_t g_donations = 0;
constexpr size_t MinSlots = 512; // P7.5: below this always serial; above it the adaptive rule decides
// P7.5 adaptive rule: parallel only if it shortens the wall-clock trace. Per mode keep an EMA of trace wall time per
// heap slot (ns/slot); pick the faster mode, re-probe the other every ProbeEvery collections so the choice follows the
// heap. Both modes produce the identical mark set, so the choice affects timing only.
constexpr int ProbeEvery = 16;
struct Adaptive {
    double rate[2] = {0, 0}; uint32_t samples[2] = {0, 0}; uint64_t since_probe = 0;
    uint64_t chose[2] = {0, 0}, probes = 0;
    int choose() { // 0 = serial, 1 = parallel
        if (samples[0] < 2 || samples[1] < 2) { const int m = samples[0] <= samples[1] ? 0 : 1; ++probes; return m; }
        const int best = rate[1] < rate[0] ? 1 : 0;
        if (++since_probe >= ProbeEvery) { since_probe = 0; ++probes; return 1 - best; }
        ++chose[best]; return best;
    }
    void observe(int mode, uint64_t ns, size_t slots) {
        const double r = double(ns) / double(slots ? slots : 1);
        rate[mode] = samples[mode] ? rate[mode] * 0.75 + r * 0.25 : r; ++samples[mode];
    }
};
inline Adaptive g_adapt;
inline uint64_t g_trace_ns[2] = {0, 0}, g_trace_n[2] = {0, 0};

struct Shared {
    std::mutex m; std::condition_variable cv;
    std::vector<int> pool; std::atomic<int> idle{0}; bool done = false; int n = p7::Pool::Workers;
    std::atomic<uint64_t> scanned{0};
};
inline bool try_mark(char* mark, int ref) { return __atomic_exchange_n(&mark[ref], char(1), __ATOMIC_RELAXED) == 0; }

// children(ref, push): pushes every child reference (bounds-checked by push). Runs on a worker.
template <class Children> inline void trace_worker(Shared& s, char* mark, size_t slots, Children children) {
    P7_SCOPE(Gc);
    std::vector<int> local; local.reserve(1024); uint64_t scanned = 0;
    auto push = [&](int c) { if (c > 0 && size_t(c) < slots && try_mark(mark, c)) local.push_back(c); };
    for (;;) {
        if (local.empty()) {
            std::unique_lock<std::mutex> lk(s.m);
            s.idle.fetch_add(1);
            while (s.pool.empty() && !s.done) {
                if (s.idle.load() == s.n) { s.done = true; s.cv.notify_all(); break; }
                s.cv.wait(lk);
            }
            if (s.pool.empty()) { s.scanned += scanned; return; } // done: nobody has work and nothing is pooled
            s.idle.fetch_sub(1);
            const size_t take = std::min<size_t>(s.pool.size(), std::max<size_t>(64, s.pool.size() / size_t(s.n)));
            local.insert(local.end(), s.pool.end() - take, s.pool.end()); s.pool.resize(s.pool.size() - take);
            if (!s.pool.empty()) s.cv.notify_one();
        }
        const int r = local.back(); local.pop_back();
        scanned += children(r, push);
        if (local.size() > 256 && s.idle.load(std::memory_order_relaxed) > 0) { // someone starves: donate half
            std::lock_guard<std::mutex> lk(s.m);
            const size_t half = local.size() / 2;
            s.pool.insert(s.pool.end(), local.end() - half, local.end()); local.resize(local.size() - half);
            ++g_donations; s.cv.notify_one();
        }
    }
}
inline std::string configure(bool requested, bool verify) {
    if (!requested) return "P7GC schema=1 requested=0 enabled=0 reason=disabled";
    if (!p7::pool().enabled()) return "P7GC schema=1 requested=1 enabled=0 reason=worker_off";
    g_enabled = true; g_verify = verify;
    return std::string("P7GC schema=1 requested=1 enabled=1 workers=") + std::to_string(p7::Pool::Workers) +
           " mode=parallel_mark_serial_sweep rule=adaptive_wall probe_every=" + std::to_string(ProbeEvery) + " verify=" + (verify ? "1" : "0") + " min_slots=" + std::to_string(MinSlots);
}
} // namespace p7gc
