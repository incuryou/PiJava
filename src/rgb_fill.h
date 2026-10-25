#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "optimization_config.h"

namespace rgb_fill {
// Validated, clipped RGB24 rectangle. Populate one row, then copy it instead
// of re-running three stores per pixel on every row. Padding remains untouched.
inline void rectangle(uint8_t* destination, size_t stride, size_t width,
                      size_t height, uint8_t red, uint8_t green, uint8_t blue) {
    if (!width || !height) return;
    const size_t row_bytes = width * 3;
#if J2ME_OPT_RGB_FILL
    if (red == green && green == blue) {
        if (row_bytes == stride) { std::memset(destination, red, row_bytes*height); return; }
        for (size_t y = 0; y < height; ++y) std::memset(destination+y*stride,red,row_bytes);
        return;
    }
    if (row_bytes >= 24 && height >= 2) {
        uint8_t* row = destination;
        for (size_t x = 0; x < width; ++x) { *row++=red; *row++=green; *row++=blue; }
        for (size_t y = 1; y < height; ++y) std::memcpy(destination+y*stride,destination,row_bytes);
        return;
    }
#endif
    for (size_t y = 0; y < height; ++y) {
        uint8_t* row=destination+y*stride;
        for (size_t x=0;x<width;++x) { *row++=red; *row++=green; *row++=blue; }
    }
}
}
