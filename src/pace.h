#pragma once
#include <cstdint>

// Thread.sleep of 8..20 ms is a game frame delay. At a vsync slot, wake it if it
// has not expired so that slot still paints. Longer sleeps stay on the wall clock.
// Joins and Object.wait never set a request in this range.
inline bool pace_align_sleep(int requested_ms, bool sleeping, int64_t wake, int64_t now) {
    return sleeping && requested_ms >= 8 && requested_ms <= 20 && wake > now;
}

// Keep the absolute phase through ordinary late wakes. A missed full period
// starts a fresh grid, so a long hitch cannot accumulate catch-up debt.
inline int64_t speed_next_paint_deadline(int period_us,int64_t next_us,int64_t anchor_us) {
    if(period_us<=0)return 0;
    if(next_us<=0 || anchor_us-next_us>=period_us)return anchor_us+period_us;
    if(anchor_us<next_us)return next_us;
    return next_us+period_us;
}

// Retaining the nominal grid must not permit a near-slot drawing burst.
// Repay at most the existing 2ms allowance per actual drawing interval.
inline int64_t speed_paint_wait_deadline(int period_us,int64_t next_us,int64_t last_us) {
    if(period_us<=0 || next_us<=0)return 0;
    if(last_us<=0)return next_us;
    const int64_t earliest=last_us+(period_us>3000?period_us-2000:period_us);
    return next_us<earliest?earliest:next_us;
}

// Absolute paint-clock deadline for a speed cap, or 0 to keep the Java sleep.
// A slot that has already passed must not become a zero-length sleep: a game
// that polls with Thread.sleep would yield until the worker slice is exhausted,
// and the host would then wait another period (~18fps on a 30 cap).
inline int64_t speed_frame_deadline(int period_us, int64_t next_us, int64_t now_us,
                                    int64_t requested_ms, int64_t& cut_us, int64_t& add_us,
                                    int64_t anchor_us = 0) {
    if (period_us <= 0 || requested_ms < 8 || requested_ms > 120) return 0;
    const int64_t asked_us = requested_ms * 1000;
    const int64_t due = next_us > 0 ? next_us : now_us + period_us;
    if (due <= now_us) {
        if (asked_us <= period_us) return 0;
        // Stay on the grid. due+period is the next slot; now+period would add
        // this wake's lateness forever (a 1.2ms bias is 60 -> ~56 and 30 -> ~29).
        // More than one period late still takes a single period so a hitch
        // cannot burst catch-up frames. A paint already done in this slice
        // only waits out what that slot has left.
        int64_t target = due + period_us;
        if (target <= now_us) {
            int64_t remain = period_us;
            if (anchor_us > 0 && now_us > anchor_us) {
                const int64_t spent = now_us - anchor_us;
                remain = spent < period_us ? period_us - spent : 1000;
            }
            if (remain < 1000) remain = 1000;
            cut_us += asked_us - remain;
            return now_us + remain;
        }
        int64_t remain = target - now_us;
        if (anchor_us > 0 && now_us > anchor_us) {
            const int64_t spent = now_us - anchor_us;
            const int64_t slot_left = spent < period_us ? period_us - spent : 1000;
            if (slot_left < remain) remain = slot_left;
        }
        if (remain < 1000) remain = 1000;
        cut_us += asked_us - remain;
        return now_us + remain;
    }
    const int64_t wait_us = due - now_us;
    if (wait_us < asked_us) cut_us += asked_us - wait_us;
    else if (wait_us > asked_us) add_us += wait_us - asked_us;
    return due;
}

// A completed frame belongs to a Java thread, not to a host execution slice.
// Retain that frame's deadline across yields and consume it only once.
inline int64_t speed_owned_frame_deadline(int64_t due_us, int64_t now_us,
                                          int64_t requested_ms, int64_t& cut_us, int64_t& add_us) {
    const int64_t wait_us = due_us > now_us ? due_us - now_us : 1000;
    const int64_t asked_us = requested_ms * 1000;
    if (wait_us < asked_us) cut_us += asked_us - wait_us;
    else if (wait_us > asked_us) add_us += wait_us - asked_us;
    return now_us + wait_us;
}
