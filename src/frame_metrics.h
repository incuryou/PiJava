#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

// Quantiles are bucket UPPER bounds (250 us resolution up to 128 ms), not
// fabricated exact per-frame percentiles. The last bucket uses observed max.
struct DurationHistogram {
    static constexpr size_t Bins = 512;
    std::array<uint64_t, Bins> buckets{};
    uint64_t count = 0, sum_us = 0, max_us = 0, overflow = 0;
    void add(uint64_t us) {
        const uint64_t index = us / 250;
        ++buckets[size_t(std::min<uint64_t>(Bins - 1, index))];
        ++count; sum_us += us; max_us = std::max(max_us, us);
        overflow += index >= Bins - 1;
    }
    uint64_t quantile_upper(unsigned percent) const {
        if (!count) return 0;
        const uint64_t target = (count * std::min(100u, std::max(1u, percent)) + 99) / 100;
        uint64_t seen = 0;
        for (size_t i = 0; i < Bins; ++i) {
            seen += buckets[i];
            if (seen >= target) return i == Bins-1 ? max_us : std::min(max_us, uint64_t(i+1)*250);
        }
        return max_us;
    }
};
struct FrameMetricWindow {
    uint64_t completed_draws = 0, content_frames = 0, unchanged_draws = 0, content_unavailable = 0;
    DurationHistogram draw_intervals, gc_pauses;
};
class FrameMetrics {
    std::vector<uint8_t> previous_;
    int64_t last_draw_us_ = 0;
public:
    FrameMetricWindow pending;
    bool content_enabled = true;
    void reset() { previous_.clear(); last_draw_us_ = 0; pending = {}; }
    void record(int64_t now_us, const std::vector<uint8_t>& rgb) {
        ++pending.completed_draws;
        if (last_draw_us_ && now_us >= last_draw_us_) pending.draw_intervals.add(uint64_t(now_us-last_draw_us_));
        last_draw_us_ = now_us;
        if (!content_enabled || rgb.empty() || rgb.size() > 4u*1024u*1024u) { ++pending.content_unavailable; return; }
        const bool changed = previous_.size() != rgb.size() ||
            std::memcmp(previous_.data(), rgb.data(), rgb.size()) != 0;
        if (changed) { ++pending.content_frames; previous_.assign(rgb.begin(),rgb.end()); }
        else ++pending.unchanged_draws;
    }
    FrameMetricWindow take() { auto result = pending; pending = {}; return result; }
    size_t snapshot_bytes() const { return previous_.capacity(); }
};
