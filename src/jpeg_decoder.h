#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

// Compile the in-memory JPEG decoder plus the bounded raw DEFLATE routine
// used by the existing PNG parser. No PNG color/filter conversion is replaced.
// The same 1024-pixel limit bounds dimensions before decoding.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#if J2ME_OPT_FAST_PNG
#define STBI_SUPPORT_ZLIB
#endif
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_MAX_DIMENSIONS 1024
#include "third_party/stb_image.h"
#undef STBI_MAX_DIMENSIONS
#undef STBI_NO_THREAD_LOCALS
#undef STBI_NO_LINEAR
#undef STBI_NO_HDR
#undef STBI_NO_STDIO
#undef STBI_ONLY_JPEG
#undef STBI_SUPPORT_ZLIB
#undef STB_IMAGE_STATIC
#undef STB_IMAGE_IMPLEMENTATION

namespace j2me_image {

inline bool decode_jpeg(const std::vector<uint8_t>& bytes,
                        std::vector<uint8_t>& rgb, std::vector<uint8_t>& alpha,
                        int& width, int& height, std::string& error) {
    if (bytes.size() < 4 || bytes[0] != 0xff || bytes[1] != 0xd8) {
        error = "not jpeg";
        return false;
    }
    if (bytes.size() > size_t(std::numeric_limits<int>::max())) {
        error = "jpeg input too large";
        return false;
    }
    // stb can recover incomplete entropy data; MIDP must reject a genuinely
    // truncated stream. EOI may precede harmless trailing application bytes.
    bool end_marker = false;
    for (size_t i = bytes.size() - 1; i > 1; --i)
        if (bytes[i - 1] == 0xff && bytes[i] == 0xd9) { end_marker = true; break; }
    if (!end_marker) { error = "jpeg truncated (missing EOI)"; return false; }
    int w = 0, h = 0, channels = 0;
    const int length = int(bytes.size());
    if (!stbi_info_from_memory(bytes.data(), length, &w, &h, &channels)) {
        const char* reason = stbi_failure_reason();
        error = std::string("jpeg header: ") + (reason ? reason : "invalid data");
        return false;
    }
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) {
        error = "jpeg size exceeds 1024x1024";
        return false;
    }
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(), length, &w, &h, &channels, 3);
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        error = std::string("jpeg decode: ") + (reason ? reason : "invalid data");
        return false;
    }
    // Outputs change only after a complete successful decode.
    rgb.assign(pixels, pixels + size_t(w) * size_t(h) * 3);
    stbi_image_free(pixels);
    alpha.assign(size_t(w) * size_t(h), 255);
    width = w;
    height = h;
    error.clear();
    return true;
}

} // namespace j2me_image
