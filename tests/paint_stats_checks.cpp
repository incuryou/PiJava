#include "../src/paint_stats.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    CanvasPaintStats stats;
    stats.reset(10000000, 100);
    assert(!stats.ready(10999999));
    assert(stats.ready(11000000));
    stats.completed(16666);
    stats.completed(16667);
    stats.completed(33333);
    stats.completed(33334);
    stats.worker_elapsed(250000);
    // Four Canvas completions plus two GameCanvas flushGraphics calls.
    auto window = stats.take(11250000, 106);
    assert(window.elapsed_us == 1250000);
    assert(window.canvas_paints == 4 && window.gamecanvas_flushes == 2);
    assert(window.over_60hz == 3 && window.over_30hz == 1);
    assert(window.paint_us == 100000 && window.paint_max_us == 33334);
    assert(window.worker_us == 250000);
    assert(std::fabs(window.canvas_fps() - 3.2) < 0.000001);
    assert(std::fabs(window.paint_avg_ms() - 25.0) < 0.000001);

    // Flush-only windows do not invent Canvas FPS or callback durations.
    auto flush_only = stats.take(12250000, 166);
    assert(flush_only.canvas_paints == 0 && flush_only.gamecanvas_flushes == 60);
    assert(flush_only.canvas_fps() == 0 && flush_only.paint_avg_ms() == 0);
    assert(flush_only.paint_max_us == 0 && flush_only.worker_us == 0);

    stats.completed(40000);
    stats.reset(20000000, 200); // Relaunch/back-to-menu discards old counts.
    assert(!stats.ready(20999999));
    auto empty = stats.take(21000000, 200);
    assert(empty.canvas_paints == 0 && empty.gamecanvas_flushes == 0);
    assert(empty.paint_us == 0 && empty.over_60hz == 0 && empty.over_30hz == 0);
    std::puts("paint stats checks passed");
}
