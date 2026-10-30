#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Host-only RGB presentation transform. The logical framebuffer is never changed.
namespace presentation { namespace orientation {
class Cache {
public:
    // turns: 0=original, 1=90 CW, 2=180, 3=270 CW.
    // A dirty source must be signalled even when epoch/frame have not advanced.
    // Returned storage remains valid until this cache is reused (or rgb changes).
    const std::vector<uint8_t>& apply(const std::vector<uint8_t>& rgb, int w, int h,
                                     int turns, uint64_t epoch, uint64_t frame, bool dirty) {
        if (w <= 0 || h <= 0 || w > 800 || h > 800 ||
            size_t(w) * size_t(h) > 409600 || turns < 0 || turns > 3 ||
            rgb.size() < size_t(w) * size_t(h) * 3) {
            valid_ = false;
            width_ = height_ = 0;
            rotated_.clear();
            return rotated_;
        }
        width_ = (turns & 1) ? h : w;
        height_ = (turns & 1) ? w : h;
        if (turns == 0) {
            // No allocation/copy on the normal route. Invalidate so a later
            // rotated route cannot revive pixels from before this direct route.
            valid_ = false;
            return rgb;
        }
        if (valid_ && !dirty && source_ == rgb.data() &&
            source_width_ == w && source_height_ == h && turns_ == turns &&
            epoch_ == epoch && frame_ == frame) return rotated_;

        rotated_.resize(size_t(w) * size_t(h) * 3);
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            int dx, dy;
            if (turns == 1) { dx = h - 1 - y; dy = x; }
            else if (turns == 2) { dx = w - 1 - x; dy = h - 1 - y; }
            else { dx = y; dy = w - 1 - x; }
            const size_t src = (size_t(y) * w + x) * 3;
            const size_t dst = (size_t(dy) * width_ + dx) * 3;
            rotated_[dst] = rgb[src];
            rotated_[dst + 1] = rgb[src + 1];
            rotated_[dst + 2] = rgb[src + 2];
        }
        source_ = rgb.data(); source_width_ = w; source_height_ = h;
        turns_ = turns; epoch_ = epoch; frame_ = frame; valid_ = true;
        return rotated_;
    }
    int width() const { return width_; }
    int height() const { return height_; }
private:
    std::vector<uint8_t> rotated_;
    const uint8_t* source_ = nullptr;
    int width_ = 0, height_ = 0, source_width_ = 0, source_height_ = 0, turns_ = 0;
    uint64_t epoch_ = 0, frame_ = 0;
    bool valid_ = false;
};
}}
