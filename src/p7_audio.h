// P7.3 (0.2.66-p7.6): audio mixing + MIDI synthesis on the P7.2 worker (core 1 on Switch).
// Switch: sdmc:/switch/j2me/p7-audio.enable ; desktop: J2ME_P7_AUDIO=1. Requires the worker pool (p7-worker), default OFF.
//
// Determinism contract (why the PCM is unchanged): the main thread still decides WHEN and HOW MANY frames are rendered,
// exactly as before (desktop: elapsed-time frame count and the same <=1024-frame chunk loop; Switch: the same
// pumpThrottled() call). Only the execution of that render moves to the worker. Every main-thread access to the audio
// engine or the Switch audio sink goes through the VMS accessors, which call fence() first: fence() waits for the
// in-flight render job. So the engine sees the identical sequence of operations (render N, control call, render M, ...)
// as with the switch off. At most one render job is ever in flight. Save/restore drain the pool before touching state.
#pragma once
#include "p7_worker.h"
#include <atomic>
#include <cstdio>
#include <vector>

namespace p7audio {
inline bool g_enabled = false;                 // set once at startup, before any job exists
inline uint64_t g_ticket = 0;                  // main-thread only
inline uint64_t g_jobs = 0, g_fences = 0, g_inline = 0; // main-thread only
inline std::atomic<int64_t> g_failure_rc{0};   // Switch: worker-side pump failure, reported by the main thread
inline std::atomic<const char*> g_failure_op{nullptr};
#ifndef __SWITCH__
inline FILE* g_dump = nullptr;                 // desktop test-only PCM capture (J2ME_P7_PCM_DUMP); written by the rendering thread
inline std::atomic<uint64_t> g_dump_frames{0};
inline void dump(const int16_t* pcm, size_t frames) {
    if (!g_dump) return;
    std::fwrite(pcm, sizeof(int16_t) * 2, frames, g_dump);
    g_dump_frames.fetch_add(frames, std::memory_order_relaxed);
}
#endif

#ifndef __SWITCH__
// Desktop batching: the per-slice pump appends its frame count here; the renders are executed (in the same order, each
// with its own <=1024-frame chunk loop) either on worker 0 once >= BatchFrames are pending, or inline by fence() as soon
// as anything touches the engine. Nothing can observe the engine in between, so the operation sequence is unchanged.
constexpr uint64_t BatchFrames = 1024;
inline std::vector<uint32_t> g_pending;        // main-thread only
inline uint64_t g_pending_frames = 0, g_frames_worker = 0, g_frames_inline = 0, g_batches = 0;
inline void (*g_render_counts)(const std::vector<uint32_t>&) = nullptr; // set by the VM; renders without fencing
#endif
// Exact fast answers for pendingEnds()/paused() without waiting for the render owner. Snapshot taken while the main
// thread owns a quiescent engine; render never clears ends_ or touches paused_, and can only add an end after
// q_headroom frames. Any accessor call (possible mutation) invalidates the snapshot.
inline bool q_valid = false, q_ends = false, q_paused = false, q_idle = false;
inline uint64_t g_idle_pumps = 0; // pumps run inline because the snapshot proved the engine idle (no submit)
inline uint64_t q_headroom = 0, q_frames = 0, g_fast_hits = 0;
// Desktop pump clock diagnostics (log only; identical off/on): calls, frames requested, us lost to the 200 ms clamp.
inline uint64_t g_pump_calls = 0, g_pump_frames = 0, g_pump_clamped_us = 0, g_pump_elapsed_us = 0, g_pump_first_us = 0, g_pump_last_us = 0;
inline bool fast_ends_ok() { return g_enabled && q_valid && q_frames < q_headroom; }
inline bool fast_paused_ok() { return g_enabled && q_valid; }
inline void fence_job();
// Called by the VMS accessors for the engine / sink. Workers never fence (they own the engine while a job runs).
inline void fence() {
    if (p7::t_slot != 0) return;
    q_valid = false;
#ifndef __SWITCH__
    if (!g_ticket && g_pending.empty()) return;
#else
    if (!g_ticket) return;
#endif
    fence_job();
#ifndef __SWITCH__
    if (!g_pending.empty()) { // flush not-yet-submitted renders inline, in order
        std::vector<uint32_t> counts; counts.swap(g_pending);
        g_frames_inline += g_pending_frames; g_pending_frames = 0;
        g_render_counts(counts);
    }
#endif
}
inline void fence_job() {
    if (!g_ticket) return;
    const uint64_t t = g_ticket;
    g_ticket = 0; ++g_fences;
    p7::Pool::Task job;
    if (p7::pool().steal(t, job)) { ++g_inline; job(); return; } // not started yet: run it here, same order
    P7_SCOPE(AudioWait);
    p7::pool().wait(t);
}

template <class Job> inline void submit(Job&& job) {
    fence_job();
    g_ticket = p7::pool().submit(std::forward<Job>(job), 0); // always worker 0 (core 1): keeps render order trivially serial
    ++g_jobs;
}

inline std::string configure(bool requested) {
    if (!requested) return "P7AUDIO schema=1 requested=0 enabled=0 reason=disabled";
    if (!p7::pool().enabled()) return "P7AUDIO schema=1 requested=1 enabled=0 reason=worker_off";
    g_enabled = true;
    return "P7AUDIO schema=1 requested=1 enabled=1 worker=0 mode=render_on_worker fence=vms_accessor";
}
} // namespace p7audio
