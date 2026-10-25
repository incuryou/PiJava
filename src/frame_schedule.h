#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

struct WakeStats {
    uint64_t sleep_wakes=0, wake_late_ms=0, wake_late_max_ms=0;
    uint64_t wake_late_3ms_count=0;
    uint64_t near_waits=0, near_wait_us=0, near_wait_max_us=0;
    uint64_t deferred_wakes=0,extended_waits=0,budget_samples=0;
    int budget_cap_min_ms=0,budget_cap_max_ms=0;
    int64_t ignore_before_ms=0;
    void dispatched(int64_t due,int64_t now) {
        if(due<=0 || due<ignore_before_ms)return;
        uint64_t late=uint64_t(std::max<int64_t>(0,now-due));
        ++sleep_wakes;wake_late_ms+=late;wake_late_max_ms=std::max(wake_late_max_ms,late);
        if(late>=3)++wake_late_3ms_count;
    }
    void waited(int64_t us) {
        ++near_waits;uint64_t value=uint64_t(std::max<int64_t>(0,us));
        near_wait_us+=value;near_wait_max_us=std::max(near_wait_max_us,value);
    }
    void budget_decision(int cap_ms,int64_t remaining_ms) {
        cap_ms=std::max(0,cap_ms);
        if(!budget_samples)budget_cap_min_ms=budget_cap_max_ms=cap_ms;
        else {
            budget_cap_min_ms=std::min(budget_cap_min_ms,cap_ms);
            budget_cap_max_ms=std::max(budget_cap_max_ms,cap_ms);
        }
        ++budget_samples;
        if(remaining_ms>=3) {
            if(remaining_ms>cap_ms)++deferred_wakes;
            else ++extended_waits;
        }
    }
    WakeStats take() {WakeStats old=*this;*this={};ignore_before_ms=old.ignore_before_ms;return old;}
};
// Preserve the existing one-to-two millisecond deadline bridge at every load.
// Only its extension to three-to-six milliseconds needs room in the current
// screen period for the next Java update and submission. Recent maxima keep a
// cheap sleeping frame from hiding a recent expensive paint. The legacy bridge
// itself is not guaranteed to fit within one screen period under heavy load.
// Observe the whole advance (including synchronous and pending Canvas paints),
// and nonblocking prepare/submit work separately. Never include acquire or the
// optional wait, and never add worker_elapsed to paint_elapsed a second time.
struct FrameWaitBudget {
    static constexpr int64_t frame_period_us=16667;
    static constexpr int64_t guard_us=1000;
    static constexpr int64_t legacy_wait_us=2000;
    static constexpr int64_t hard_wait_us=6000;
    static constexpr unsigned history_size=8;
    std::array<int64_t,history_size> work_us{},submit_us{};
    unsigned samples=0,next=0;
    void observe(int64_t work,int64_t submit) {
        work_us[next]=std::max<int64_t>(0,std::min(work,frame_period_us));
        submit_us[next]=std::max<int64_t>(0,std::min(submit,frame_period_us));
        next=(next+1)%history_size;
        samples=std::min(samples+1,history_size);
    }
    int64_t recent_work_us() const {
        return *std::max_element(work_us.begin(),work_us.end());
    }
    int64_t recent_submit_us() const {
        return *std::max_element(submit_us.begin(),submit_us.end());
    }
    int cap_ms(int64_t slot_elapsed_us=0) const {
        const int64_t elapsed=std::max<int64_t>(0,std::min(slot_elapsed_us,frame_period_us));
        // Without a cost sample retain the old rounding bridge, not a large wait.
        const int64_t slack=frame_period_us-guard_us-elapsed-
            recent_work_us()-recent_submit_us();
        if(!samples)return int(legacy_wait_us/1000);
        return int(std::min(hard_wait_us,std::max(legacy_wait_us,slack))/1000);
    }
};
// The screen slot must be ready before checking Java wake deadlines. A single
// short sleep bridges millisecond rounding near a frame boundary. Never shorten
// Java sleep, run a second worker slice, or wait for a distant timer.
template<class Acquire,class Delay,class Wait,class Advance>
inline void scheduled_frame(Acquire acquire,Delay delay,Wait wait,Advance advance,int max_wait_ms=2) {
    acquire();
    const int64_t remaining=delay();
    if(remaining>0 && remaining<=max_wait_ms)wait(remaining);
    advance();
}
// Clock reports host microseconds. Read the Java deadline only after acquiring
// the slot, account for deadline-query overhead, and perform at most one wait.
// The actual Java timer is untouched. Preserve the legacy rounding bridge and
// defer larger timers if the extension exceeds the available slot budget.
// GPU callers can keep using the integer overload with a zero budget.
template<class Acquire,class Delay,class Wait,class Advance,class Clock>
inline void scheduled_frame(Acquire acquire,Delay delay,Wait wait,Advance advance,
                            const FrameWaitBudget& budget,Clock clock,WakeStats* stats=nullptr) {
    acquire();
    const int64_t slot_started_us=clock();
    const int64_t remaining=delay();
    const int cap=budget.cap_ms(clock()-slot_started_us);
    if(stats)stats->budget_decision(cap,remaining);
    if(remaining>0 && remaining<=cap)wait(remaining);
    advance();
}
