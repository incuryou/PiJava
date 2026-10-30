#pragma once

#include "host_menu.h"
#include "jpeg_decoder.h"
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// Static host decoration only. The file format and decoded memory are bounded
// independently of file contents. Reuse the existing bounded JPEG decoder.
namespace host_skin {
constexpr size_t pixel_count = size_t(host_menu::width) * host_menu::height;
constexpr size_t header_size = 20;
constexpr size_t jpeg_limit = 512 * 1024;
constexpr int half_width = host_menu::width / 2;
using Pixels = std::vector<uint32_t>;
using Loader = std::function<bool(const char*, Pixels&, std::string&)>;
using Logger = std::function<void(const std::string&)>;

inline const char* path(int bezel) {
    switch (bezel) {
    case 1: return "romfs:/skins/phone-blue.skin";
    case 2: return "romfs:/skins/phone-black.skin";
    case 3: return "romfs:/skins/phone-ivory.skin";
    case 4: return "romfs:/skins/controller-ivory.skin";
    case 5: return "romfs:/skins/controller-blue.skin";
    case 6: return "romfs:/skins/controller-black.skin";
    default: return nullptr;
    }
}
inline uint16_t le16(const unsigned char* bytes) {
    return uint16_t(bytes[0]) | (uint16_t(bytes[1]) << 8);
}
inline uint32_t le32(const unsigned char* bytes) {
    return uint32_t(le16(bytes)) | (uint32_t(le16(bytes + 2)) << 16);
}
inline void release(Pixels& pixels) { Pixels{}.swap(pixels); }

inline bool load_file(const char* filename, Pixels& pixels, std::string& error) {
    release(pixels);
    error.clear();
    if (!filename || !*filename) { error = "invalid path"; return false; }
    std::FILE* input = std::fopen(filename, "rb");
    if (!input) { error = "open failed"; return false; }
    unsigned char header[header_size];
    bool ok = std::fread(header, 1, sizeof(header), input) == sizeof(header);
    if (!ok) error = "short header";
    uint32_t lengths[2] = {};
    if (ok) {
        lengths[0] = le32(header + 8);
        lengths[1] = le32(header + 12);
        if (header[0] != 'J' || header[1] != '2' || header[2] != 'S' || header[3] != 'J' ||
            le16(header + 4) != host_menu::width || le16(header + 6) != host_menu::height ||
            le32(header + 16) != 0 || lengths[0] == 0 || lengths[0] > jpeg_limit ||
            lengths[1] == 0 || lengths[1] > jpeg_limit) {
            ok = false;
            error = "invalid header";
        }
    }
    // One persistent 3.52 MiB texture; decode one bounded 640x720 JPEG at a
    // time. Each JPEG/input/RGB/alpha temporary dies before the next half.
    if (ok) pixels.resize(pixel_count);
    for (int half = 0; ok && half < 2; ++half) {
        std::vector<uint8_t> bytes(lengths[half]);
        if (std::fread(bytes.data(), 1, bytes.size(), input) != bytes.size()) {
            ok = false;
            error = "short jpeg payload";
            break;
        }
        // Unlike generic Java JPEG resources, skin streams end exactly at EOI.
        if (bytes.size() < 4 || bytes[bytes.size() - 2] != 0xff || bytes.back() != 0xd9) {
            ok = false;
            error = "jpeg missing terminal EOI";
            break;
        }
        std::vector<uint8_t> rgb, alpha;
        int w = 0, h = 0;
        if (!j2me_image::decode_jpeg(bytes, rgb, alpha, w, h, error)) {
            ok = false;
            break;
        }
        if (w != half_width || h != host_menu::height) {
            ok = false;
            error = "invalid jpeg half dimensions";
            break;
        }
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            const int dst_x = half * half_width + x;
            const bool game = dst_x >= host_menu::game_x && dst_x < host_menu::game_x + host_menu::game_width &&
                              y >= host_menu::game_y && y < host_menu::game_y + host_menu::game_height;
            const size_t src = (size_t(y) * w + x) * 3;
            pixels[size_t(y) * host_menu::width + dst_x] = game ? host_menu::color(0) :
                (0xff000000u | uint32_t(rgb[src]) | (uint32_t(rgb[src + 1]) << 8) | (uint32_t(rgb[src + 2]) << 16));
        }
    }
    if (ok && std::fgetc(input) != EOF) { ok = false; error = "trailing bytes"; }
    if (std::ferror(input)) { ok = false; error = "read failed"; }
    if (std::fclose(input) != 0) { ok = false; error = "close failed"; }
    if (!ok) release(pixels);
    return ok;
}

class Cache {
    int attempted_ = -1;
    Pixels pixels_;
public:
    // Call only when clearing the framebuffer ring. A repeated game start or
    // its three buffers reuses the active texture, including a failed attempt.
    bool select(int bezel, const Logger& log = Logger{}, const Loader& loader = load_file) {
        const int asset = bezel;
        if (asset == attempted_) return !pixels_.empty();
        attempted_ = asset;
        release(pixels_);
        const char* filename = path(bezel);
        if (!filename) return false;
        std::string error;
        if (!loader(filename, pixels_, error) || pixels_.size() != pixel_count) {
            release(pixels_);
            if (log) log("SKIN load failed path=" + std::string(filename) + " reason=" +
                         (error.empty() ? "invalid decoded size" : error) +
                         (bezel == j2me_present::adaptive_bezel ? " fallback=black" : " fallback=native"));
            return false;
        }
        return true;
    }
    const uint32_t* data() const { return pixels_.empty() ? nullptr : pixels_.data(); }
    size_t size() const { return pixels_.size(); }
};
} // namespace host_skin
