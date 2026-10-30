#pragma once
#include <climits>
#include <cstdint>

// Counted new-frame presentations per elapsed host microsecond. The selected speed
// cap is deliberately absent: rounding must not disguise a slower renderer.
inline int rounded_present_fps(uint64_t frames, int64_t elapsed_us) {
    if (elapsed_us <= 0) return 0;
    const long double value = static_cast<long double>(frames) * 1000000.0L / elapsed_us;
    return value >= INT_MAX ? INT_MAX : int(value + 0.5L);
}
