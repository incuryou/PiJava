#include "../src/host_skin.h"
#include <fstream>
#include <iostream>
#include <array>

// Run from the project root after compilation. The same loader/render code
// and packed files are used by the Switch presenter; output is a host preview.
// Optional argument: a 240x320 RGB P6 PPM. It is copied at integer 2x into
// the unchanged Switch viewport, and is never saved into a skin resource.
int main(int argc, char** argv) {
    std::array<unsigned char, 240 * 320 * 3> game{};
    if (argc > 2) return 1;
    if (argc == 2) {
        std::ifstream in(argv[1], std::ios::binary);
        std::string magic;
        int width = 0, height = 0, maximum = 0;
        in >> magic >> width >> height >> maximum;
        if (magic != "P6" || width != 240 || height != 320 || maximum != 255 || in.get() != '\n') {
            std::cerr << "expected 240x320 RGB P6 PPM\n";
            return 1;
        }
        in.read(reinterpret_cast<char*>(game.data()), game.size());
        if (!in || in.peek() != std::char_traits<char>::eof()) return 1;
    }
    for (int bezel = 1; bezel < host_menu::bezel_count; ++bezel) {
        const std::string relative = std::string(host_skin::path(bezel)).substr(7);
        host_skin::Pixels pixels;
        std::string error;
        if (!host_skin::load_file(("romfs/" + relative).c_str(), pixels, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::vector<uint32_t> frame(host_skin::pixel_count);
        host_menu::render_bezel(frame.data(), 1280, bezel, {}, pixels.data(), pixels.size());
        for (int y = host_menu::game_y; y < host_menu::game_y + host_menu::game_height; ++y)
            for (int x = host_menu::game_x; x < host_menu::game_x + host_menu::game_width; ++x) {
                if (frame[size_t(y) * 1280 + x] != host_menu::color(0)) {
                    std::cerr << "skin leaked into the game viewport\n";
                    return 1;
                }
                if (argc == 2) {
                    const size_t src = (size_t((y - host_menu::game_y) / 2) * 240 +
                                        (x - host_menu::game_x) / 2) * 3;
                    frame[size_t(y) * 1280 + x] = 0xff000000u | uint32_t(game[src]) |
                        (uint32_t(game[src + 1]) << 8) | (uint32_t(game[src + 2]) << 16);
                }
            }
        std::ofstream out("build/phone89-native-" + std::to_string(bezel) +
                          (argc == 2 ? "-game.ppm" : ".ppm"), std::ios::binary);
        out << "P6\n1280 720\n255\n";
        for (uint32_t pixel : frame) {
            const char rgb[] = {char(pixel & 255), char((pixel >> 8) & 255), char((pixel >> 16) & 255)};
            out.write(rgb, 3);
        }
        if (!out) return 1;
    }
    std::cout << "PASS three production phone assets loaded and rendered\n";
}
