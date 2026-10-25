#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

// Preparation is wake-to-draw wall time before cap waiting, not CPU time.
// Only an owner's completed drawing followed by an eligible sleep arms it.
struct FramePrepBudget {
    std::array<int64_t, 8> recent{};
    unsigned count = 0, cursor = 0;
    int canvas = 0, period = 0, sleep_pc = -1;
    const void* sleep_method = nullptr;
    int64_t woke_us = 0;
    bool awaiting_wake = false, awaiting_draw = false;
    void reset() { *this = {}; sleep_pc = -1; }
    void arm(int next_canvas, int next_period, const void* method, int pc) {
        if (canvas != next_canvas || period != next_period || sleep_method != method || sleep_pc != pc) reset();
        canvas = next_canvas; period = next_period; sleep_method = method; sleep_pc = pc;
        woke_us = 0; awaiting_wake = awaiting_draw = true;
    }
    void woke(int64_t now_us) { if (awaiting_wake) { awaiting_wake = false; woke_us = now_us; } }
    void before_draw(int current_canvas, int current_period, int64_t now_us) {
        if (canvas != current_canvas || period != current_period) { reset(); return; }
        if (!awaiting_draw || awaiting_wake || woke_us <= 0) return;
        awaiting_draw = false;
        const int64_t elapsed = now_us - woke_us;
        if (elapsed < 0 || elapsed > int64_t(period) * 4) { reset(); return; }
        recent[cursor] = elapsed; cursor = (cursor + 1) % recent.size();
        count = std::min<unsigned>(count + 1, recent.size());
    }
    int64_t lead_us(int current_period) const {
        if (period != current_period || count < 4 || current_period <= 0) return 0;
        int64_t peak = 0;
        for (unsigned i = 0; i < count; ++i) peak = std::max(peak, recent[i]);
        // The recent peak handles alternating update/helper phases and ages out
        // in eight cycles. Leave part of each period for cooperative sleep.
        if (peak < 1000) return 0;
        return std::min<int64_t>(peak + 500, int64_t(current_period) * 3 / 4);
    }
    int64_t wake_deadline(int64_t draw_due_us, int64_t now_us, int current_period) const {
        const int64_t lead = lead_us(current_period);
        return lead ? std::max(now_us + 1000, draw_due_us - lead) : draw_due_us;
    }
};
