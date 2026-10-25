#ifndef J2ME_AUDIO_STATS_H
#define J2ME_AUDIO_STATS_H

#include <cstddef>
#include <cstdint>

namespace j2me_audio {
// One reporting window of main-thread sink activity. Lifecycle queue flushes
// do not count as releases, pumps, low-water observations, or empty polls.
struct SinkStats {
    uint64_t render_blocks = 0;
    uint64_t render_frames = 0;
    uint64_t total_mix_us = 0;
    uint64_t max_mix_us = 0;
    // Nonzero generated PCM demonstrates mixer output, not speaker playback.
    uint64_t nonzero_blocks = 0;
    uint32_t peak_abs_sample = 0;
    uint64_t pump_count = 0;
    uint64_t throttled_calls = 0, skipped_release_polls = 0;
    uint64_t max_pump_gap_us = 0;
    uint64_t released_buffers = 0;
    // A running queue was observed fully returned before a normal refill.
    // This indicates refill-late risk, not a proven audible dropout: audout
    // does not report when the final sample played or whether it was silent.
    uint64_t queue_empty_polls = 0;
    // Lowest owned-by-audout buffer count observed after collecting releases
    // in a normal pump. Four means no lower observation in this window.
    size_t min_queued_buffers = 4;
    uint64_t errors = 0;
};
} // namespace j2me_audio
#endif
