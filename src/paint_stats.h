#pragma once
#include <algorithm>
#include <cstdint>

// Canvas callback timings; GameCanvas flushes are counted separately because
// draw_end() also runs for flushGraphics(), which supplies no paint duration.
struct PaintStatsWindow {
    int64_t elapsed_us = 0, paint_us = 0, paint_max_us = 0, worker_us = 0;
    uint64_t canvas_paints = 0, gamecanvas_flushes = 0, over_60hz = 0, over_30hz = 0;
    double canvas_fps() const { return elapsed_us > 0 ? canvas_paints * 1000000.0 / elapsed_us : 0; }
    double draw_fps() const { return elapsed_us > 0 ? (canvas_paints + gamecanvas_flushes) * 1000000.0 / elapsed_us : 0; }
    double paint_avg_ms() const { return canvas_paints ? paint_us / (1000.0 * canvas_paints) : 0; }
};

struct CanvasPaintStats {
    int64_t window_start_us = 0;
    int draw_start_count = 0;
    PaintStatsWindow pending;
    void reset(int64_t now_us, int draw_count) {
        window_start_us = now_us; draw_start_count = draw_count; pending = {};
    }
    void completed(int64_t duration_us) {
        duration_us = std::max<int64_t>(0, duration_us);
        ++pending.canvas_paints;
        pending.paint_us += duration_us;
        pending.paint_max_us = std::max(pending.paint_max_us, duration_us);
        // Exact 1/60 and 1/30 second budgets, without millisecond rounding.
        pending.over_60hz += duration_us > 1000000 / 60;
        pending.over_30hz += duration_us > 1000000 / 30;
    }
    void worker_elapsed(int64_t duration_us) { pending.worker_us += std::max<int64_t>(0, duration_us); }
    bool ready(int64_t now_us) const { return now_us - window_start_us >= 1000000; }
    PaintStatsWindow take(int64_t now_us, int draw_count) {
        PaintStatsWindow result = pending;
        result.elapsed_us = std::max<int64_t>(0, now_us - window_start_us);
        int64_t flushes = (int64_t)draw_count - draw_start_count - (int64_t)pending.canvas_paints;
        result.gamecanvas_flushes = (uint64_t)std::max<int64_t>(0, flushes);
        reset(now_us, draw_count);
        return result;
    }
};
