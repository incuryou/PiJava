#include "../src/host_skin.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <unistd.h>

namespace {
void write_file(const std::string& filename, const std::vector<unsigned char>& bytes) {
    std::FILE* out = std::fopen(filename.c_str(), "wb");
    assert(out);
    if (!bytes.empty()) assert(std::fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size());
    assert(std::fclose(out) == 0);
}
std::vector<uint8_t> fixture(const char* name) {
    std::ifstream in(std::string("tests/fixtures/skin/") + name + ".jpg", std::ios::binary);
    assert(in);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
void put32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (i * 8));
}
std::vector<uint8_t> container(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right) {
    std::vector<uint8_t> bytes = {'J', '2', 'S', 'J', 0, 5, 0xd0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    put32(bytes, 8, uint32_t(left.size()));
    put32(bytes, 12, uint32_t(right.size()));
    bytes.insert(bytes.end(), left.begin(), left.end());
    bytes.insert(bytes.end(), right.begin(), right.end());
    return bytes;
}
constexpr uint32_t guard = 0x12345678;
constexpr size_t pitch = host_menu::width + 23;
constexpr size_t prefix = 17;
struct Frame {
    std::vector<uint32_t> pixels = std::vector<uint32_t>(prefix + pitch * host_menu::height + prefix, guard);
    uint32_t* data() { return pixels.data() + prefix; }
    void guards() const {
        for (size_t i = 0; i < prefix; ++i) assert(pixels[i] == guard);
        for (size_t i = prefix + pitch * host_menu::height; i < pixels.size(); ++i) assert(pixels[i] == guard);
        for (int y = 0; y < host_menu::height; ++y)
            for (size_t x = host_menu::width; x < pitch; ++x)
                assert(pixels[prefix + size_t(y) * pitch + x] == guard);
    }
};
}
int main() {
    using namespace host_skin;
    using namespace host_menu;
    static_assert(pixel_count == 921600 && header_size == 20 && jpeg_limit == 524288, "fixed texture bounds");
    assert(path(0) == nullptr && path(-1) == nullptr && path(7) == nullptr);
    assert(std::string(path(1)) == "romfs:/skins/phone-blue.skin");
    assert(std::string(path(2)) == "romfs:/skins/phone-black.skin");
    assert(std::string(path(3)) == "romfs:/skins/phone-ivory.skin");
    char pattern[] = "/tmp/j2me-skin-XXXXXX";
    const int fd = mkstemp(pattern);
    assert(fd >= 0 && close(fd) == 0);
    const std::string filename = pattern;
    const auto red = fixture("uniform-red"), blue = fixture("uniform-blue");
    const auto valid = container(red, blue);
    write_file(filename, valid);
    Pixels decoded;
    std::string error;
    assert(load_file(filename.c_str(), decoded, error));
    assert(error.empty() && decoded.size() == pixel_count);
    // JPEG color can differ by one from solid input. Check each half's color,
    // channel order and opaque alpha independently; viewport must be exact.
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const bool game = x >= game_x && x < game_x + game_width && y >= game_y && y < game_y + game_height;
        const uint32_t c = decoded[size_t(y) * width + x];
        if (game) assert(c == color(0));
        else {
            assert((c >> 24) == 255 && ((c >> 8) & 255) <= 1);
            if (x < half_width) assert((c & 255) >= 254 && ((c >> 16) & 255) <= 1);
            else assert(((c >> 16) & 255) >= 254 && (c & 255) <= 1);
        }
    }
    const auto reject = [&](const std::vector<unsigned char>& bytes, const char* expected_error = nullptr) {
        write_file(filename, bytes);
        decoded.assign(100, guard);
        assert(!load_file(filename.c_str(), decoded, error));
        assert(!error.empty() && decoded.empty() && decoded.capacity() == 0);
        if (expected_error) assert(error == expected_error);
    };
    reject({}, "short header");
    reject(std::vector<unsigned char>(valid.begin(), valid.begin() + 19), "short header");
    reject(std::vector<unsigned char>(valid.begin(), valid.begin() + 20), "short jpeg payload");
    reject(std::vector<unsigned char>(valid.begin(), valid.begin() + 20 + red.size() - 1), "short jpeg payload");
    reject(std::vector<unsigned char>(valid.begin(), valid.end() - 1), "short jpeg payload");
    auto changed = valid;
    changed.push_back(0);
    reject(changed, "trailing bytes");
    // Strict magic/dimensions/reserved, including all high bytes. No bz16
    // support remains; length values cannot trigger an unbounded allocation.
    for (size_t index : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(4), size_t(5),
                         size_t(6), size_t(7), size_t(16), size_t(17), size_t(18), size_t(19)}) {
        changed = valid;
        changed[index] ^= 0x80;
        reject(changed, "invalid header");
    }
    changed = valid;
    changed[2] = 'B'; changed[3] = 'Z';
    reject(changed, "invalid header");
    for (size_t offset : {size_t(8), size_t(12)}) {
        for (uint32_t length : {uint32_t(0), uint32_t(jpeg_limit + 1), uint32_t(0xffffffff)}) {
            changed = valid;
            put32(changed, offset, length);
            reject(changed, "invalid header");
        }
        // Bounds 1 and 512 KiB pass the header but fail the actual data check.
        changed = valid;
        put32(changed, offset, 1);
        reject(changed, "jpeg missing terminal EOI");
        changed = valid;
        put32(changed, offset, jpeg_limit);
        reject(changed, "short jpeg payload");
        changed = valid;
        put32(changed, offset, uint32_t(offset == 8 ? red.size() - 1 : blue.size() - 1));
        reject(changed, "jpeg missing terminal EOI");
    }
    // A successful left half must be discarded when the right half fails.
    for (const char* wrong : {"wrong-width", "wrong-height"}) {
        const auto half = fixture(wrong);
        reject(container(half, blue), "invalid jpeg half dimensions");
        reject(container(red, half), "invalid jpeg half dimensions");
    }
    auto broken = red;
    broken[0] = 0;
    reject(container(broken, blue), "not jpeg");
    reject(container(blue, broken), "not jpeg");
    broken = red;
    broken.pop_back();
    reject(container(broken, blue), "jpeg missing terminal EOI");
    reject(container(blue, broken), "jpeg missing terminal EOI");
    broken = red;
    broken.insert(broken.end(), {1, 2, 3});
    reject(container(broken, blue), "jpeg missing terminal EOI");
    reject(container(blue, broken), "jpeg missing terminal EOI");
    reject(container({0xff, 0xd8, 0xff, 0xd9}, blue));
    reject(container(red, {0xff, 0xd8, 0xff, 0xd9}));
    assert(!load_file(nullptr, decoded, error) && decoded.empty());
    assert(!load_file("", decoded, error) && decoded.empty());
    assert(std::remove(filename.c_str()) == 0);
    assert(!load_file(filename.c_str(), decoded, error) && error == "open failed");

    int reads = 0, logs = 0;
    bool fail = false, wrong_size = false;
    const Loader loader = [&](const char* name, Pixels& pixels, std::string& why) {
        ++reads;
        assert(std::string(name).find("romfs:/skins/phone-") == 0);
        assert(pixels.empty() && pixels.capacity() == 0);
        if (fail) {
            // Also model a decoder that produced a partial left texture.
            pixels.assign(pixel_count, guard);
            why = "injected missing asset";
            return false;
        }
        pixels.assign(wrong_size ? 10 : pixel_count, color(0x123456));
        return true;
    };
    const Logger logger = [&](const std::string& message) {
        ++logs;
        assert(message.find("SKIN load failed path=romfs:/skins/") == 0);
        assert(message.find("fallback=native") != std::string::npos);
    };
    Cache cache;
    assert(!cache.select(0, logger, loader) && reads == 0 && cache.data() == nullptr);
    assert(cache.select(1, logger, loader) && reads == 1 && cache.size() == pixel_count);
    const uint32_t* active = cache.data();
    for (int game = 0; game < 3; ++game) for (int buffer = 0; buffer < 3; ++buffer)
        assert(cache.select(1, logger, loader) && reads == 1 && cache.data() == active);
    assert(cache.select(2, logger, loader) && reads == 2);
    fail = true;
    assert(!cache.select(3, logger, loader) && reads == 3 && logs == 1);
    assert(cache.data() == nullptr && cache.size() == 0);
    for (int i = 0; i < 3; ++i) assert(!cache.select(3, logger, loader) && reads == 3 && logs == 1);
    assert(!cache.select(0, logger, loader) && reads == 3 && cache.data() == nullptr);
    fail = false;
    wrong_size = true;
    assert(!cache.select(1, logger, loader) && reads == 4 && logs == 2 && cache.size() == 0);
    assert(!cache.select(1, logger, loader) && reads == 4 && logs == 2);
    wrong_size = false;
    assert(cache.select(2, logger, loader) && reads == 5 && cache.size() == pixel_count);
    assert(cache.select(1, logger, loader) && reads == 6);

    // Renderer must sanitize even a caller-supplied texture, respect pitch,
    // and ignore invalid texture lengths without reading past them.
    for (int bezel = 1; bezel < bezel_count; ++bezel) {
        Frame frame;
        render_bezel(frame.data(), pitch, bezel, {}, cache.data(), cache.size());
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            const bool game = x >= game_x && x < game_x + game_width && y >= game_y && y < game_y + game_height;
            assert(frame.data()[size_t(y) * pitch + x] == color(game ? 0 : 0x123456));
        }
        frame.guards();
        const auto rendered = frame.pixels;
        render_bezel(frame.data(), width - 1, bezel, {}, cache.data(), cache.size());
        render_bezel(nullptr, pitch, bezel, {}, cache.data(), cache.size());
        assert(frame.pixels == rendered);
        render_bezel(frame.data(), pitch, bezel, {}, cache.data(), pixel_count - 1);
        assert(frame.data()[0] == color(bezel_palette(bezel).background));
        for (int y = game_y; y < game_y + game_height; ++y)
            for (int x = game_x; x < game_x + game_width; ++x)
                assert(frame.data()[size_t(y) * pitch + x] == color(0));
        frame.guards();
    }
    Frame none;
    for (int bezel : {-1, 0, bezel_count}) {
        render_bezel(none.data(), pitch, bezel, {}, cache.data(), cache.size());
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            assert(none.data()[size_t(y) * pitch + x] == color(0));
        none.guards();
    }
    std::puts("PASS HostSkinChecks: strict bounded split-JPEG assets, black viewport, single active cache, one-shot failures, pitch guards and native fallback");
}
