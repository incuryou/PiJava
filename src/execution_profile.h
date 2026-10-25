#pragma once
#include <cstdint>
#include "cpu_clock.h"
#include "optimization_config.h"

struct MethodCpuCounters { uint64_t calls = 0, self_cpu_ns = 0; };
// Optional exact CPU accounting at method transitions. It is diagnostic only:
// enabled runs are not comparable to production wall-time performance runs.
// One active method across nested VM/native/peer calls avoids double counting.
class ExecutionProfile {
    MethodCpuCounters* active_ = nullptr;
    int64_t started_ = -1;
public:
    using CpuClock = int64_t (*)();
    CpuClock cpu_clock = j2me_cpu_clock::now_ns;
    bool enabled = J2ME_PROFILE_DEFAULT != 0;
    uint64_t generation = 1, clock_failures = 0, transitions = 0;
    void activate(MethodCpuCounters* next) {
        if (!enabled || next == active_) return;
        const int64_t now = cpu_clock();
        if (active_) {
            const int64_t delta = j2me_cpu_clock::difference(started_, now);
            if (delta >= 0) active_->self_cpu_ns += uint64_t(delta);
            else ++clock_failures;
        }
        active_ = next; started_ = now; ++transitions;
    }
    MethodCpuCounters* active() const { return active_; }
    void reset() { ++generation; active_ = nullptr; started_ = -1; clock_failures = transitions = 0; }
    void called(MethodCpuCounters& counter) { if (enabled) ++counter.calls; }
    class Context {
        ExecutionProfile* p_ = nullptr;
        MethodCpuCounters* previous_ = nullptr;
        uint64_t generation_ = 0;
    public:
        // preserve=true brackets a nested run without claiming a native method.
        Context(ExecutionProfile& p, MethodCpuCounters* next, bool enter = true, bool preserve = false) {
            if (!p.enabled || !enter) return;
            p_ = &p; previous_ = p.active_; generation_ = p.generation;
            if (!preserve) p.activate(next);
        }
        ~Context() { if (p_ && p_->generation == generation_) p_->activate(previous_); }
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
    };
};
