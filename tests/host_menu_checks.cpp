#include "../src/host_menu.h"

#include <cassert>
#include <climits>
#include <cstdio>

namespace {
constexpr uint32_t guard = 0x12345678u;
constexpr size_t pitch = host_menu::width + 17;
constexpr size_t prefix = 31;

struct Frame {
    std::vector<uint32_t> pixels = std::vector<uint32_t>(
        prefix + pitch * host_menu::height + prefix, guard);
    uint32_t* data() { return pixels.data() + prefix; }
    void check_guards() const {
        for (size_t i = 0; i < prefix; ++i) assert(pixels[i] == guard);
        for (size_t i = prefix + pitch * host_menu::height; i < pixels.size(); ++i)
            assert(pixels[i] == guard);
        for (int y = 0; y < host_menu::height; ++y)
            for (size_t x = host_menu::width; x < pitch; ++x)
                assert(pixels[prefix + size_t(y) * pitch + x] == guard);
    }
    void check_opaque() {
        for (int y = 0; y < host_menu::height; ++y)
            for (int x = 0; x < host_menu::width; ++x)
                assert((data()[size_t(y) * pitch + x] >> 24) == 255);
    }
};

struct TextCall {
    int x, y, height, max_width;
    std::string text;
};
}

int main() {
    using namespace host_menu;
    assert(color(0x123456) == 0xff563412u);
    assert(visible_rows == 9 && bezel_count == 7);
    assert(game_x == 400 && game_y == 40 && game_width == 480 && game_height == 640);
    assert(Settings{}.bezel == 4 && Settings{}.show_fps);
    assert(std::string(bezel_name(0)) == "无边框");
    assert(std::string(bezel_name(1)) == "银蓝塑料");
    assert(std::string(bezel_name(2)) == "黑银金属");
    assert(std::string(bezel_name(3)) == "复古米灰");
    assert(std::string(bezel_name(4)) == "手柄米白");
    assert(std::string(bezel_name(5)) == "手柄蓝");
    assert(std::string(bezel_name(6)) == "手柄黑");
    assert(std::string(bezel_name(7)) == "无边框");

    Frame clipped;
    rect(clipped.data(), pitch, -10, -20, 15, 25, color(0x112233));
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
        assert(clipped.data()[size_t(y) * pitch + x] ==
               (x < 5 && y < 5 ? color(0x112233) : guard));
    rect(clipped.data(), pitch, width - 3, height - 2, INT_MAX, INT_MAX, color(0x334455));
    assert(clipped.data()[(height - 1) * pitch + width - 1] == color(0x334455));
    const auto unchanged = clipped.pixels;
    rect(clipped.data(), pitch, INT_MAX, INT_MAX, INT_MAX, INT_MAX, 0);
    rect(clipped.data(), pitch, INT_MIN, INT_MIN, INT_MAX, INT_MAX, 0);
    rect(clipped.data(), pitch, 0, 0, -1, 1, 0);
    rect(clipped.data(), width - 1, 0, 0, 1, 1, 0);
    rect(nullptr, pitch, 0, 0, 1, 1, 0);
    assert(clipped.pixels == unchanged);
    clipped.check_guards();

    std::vector<TextCall> calls;
    const Text text = [&](int x, int y, const std::string& s, uint32_t packed,
                          int px, int max_width) {
        assert(x >= 0 && y >= 0 && x + max_width <= width && y + px <= height);
        assert(px > 0 && max_width > 0 && (packed >> 24) == 255);
        calls.push_back({x, y, px, max_width, s});
    };
    std::vector<std::string> labels;
    for (int i = 0; i < 23; ++i) labels.push_back("游戏 " + std::to_string(i));
    labels[12] = std::string(4000, 'W') + "中文很长的名字";
    Frame menu;
    render_menu(menu.data(), pitch, labels, 12, Focus::Games, Settings{}, "就绪", text);
    menu.check_guards();
    menu.check_opaque();
    int game_rows = 0;
    bool saw_selected = false, saw_count = false, saw_disabled_speed = false;
    bool saw_games_footer = false, saw_continue = false, saw_bezel = false, saw_fps = false;
    for (const auto& call : calls) {
        if (call.x == 82 && call.y >= 124 && call.y < 574) {
            ++game_rows;
            assert(call.max_width == 754 && call.height == 24);
            if (call.text == labels[12]) saw_selected = true;
        }
        if (call.text == "13 / 23") saw_count = true;
        if (call.text == "原生") saw_disabled_speed = true;
        if (call.text == "开始") saw_games_footer = true;
        if (call.text == "继续") saw_continue = true;
        if (call.text == "遮罩") saw_bezel = true;
        if (call.text == "显示帧数") saw_fps = true;
        assert(call.text.find("导入") == std::string::npos);
    }
    assert(game_rows == visible_rows && saw_selected && saw_count && saw_disabled_speed);
    assert(saw_games_footer && saw_continue && saw_bezel && saw_fps);
    assert(menu.data()[0] == color(0xd91526));
    assert(menu.data()[6 * pitch] == color(0xf3f3f3));
    assert(menu.data()[124 * pitch + 892] == color(0xd5d5d5));
    const int selected_y = 124 + (12 % visible_rows) * 50;
    assert(menu.data()[(selected_y + 23) * pitch + 50] == color(0xf4dddd));
    assert(menu.data()[(selected_y + 23) * pitch + 68] == color(0xd91526));
    assert(menu.data()[665 * pitch + 1102] == color(0xd91526));
    // Rounded corners blend at the boundary instead of producing square edges.
    bool saw_coverage = false;
    for (int y = selected_y; y < selected_y + 12; ++y)
        for (int x = 48; x < 60; ++x) {
            const auto c = menu.data()[size_t(y) * pitch + x];
            if (c != color(0xf3f3f3) && c != color(0xf4dddd)) saw_coverage = true;
        }
    assert(saw_coverage);

    std::vector<TextCall> centered_calls;
    const CenteredText centered = [&](int x, int y, const std::string& s,
                                     uint32_t packed, int px, int max_width) {
        assert((packed >> 24) == 255 && px == 18 && max_width == 24);
        centered_calls.push_back({x, y, px, max_width, s});
    };
    calls.clear();
    render_menu(menu.data(), pitch, labels, 12, Focus::Games, Settings{}, "错误：无法启动", text, centered);
    assert(centered_calls.empty());
    bool saw_error = false;
    for (const auto& call : calls) {
        if (call.text == "错误：无法启动") saw_error = true;
        assert(call.text != "X" && call.text != "A");
    }
    assert(saw_error);

    calls.clear();
    render_menu(menu.data(), pitch, {}, -1, Focus::Speed, Settings{}, "", text);
    bool saw_empty = false, saw_settings_footer = false;
    saw_count = false;
    for (const auto& call : calls) {
        if (call.text == "暂无游戏") saw_empty = true;
        if (call.text == "0 / 0") saw_count = true;
        if (call.text == "← → 游戏 / 选项") saw_settings_footer = true;
    }
    assert(saw_empty && saw_count && saw_settings_footer);
    assert(menu.data()[344 * pitch + 924] == color(0xf4dddd));
    calls.clear();
    centered_calls.clear();
    render_menu(menu.data(), pitch, labels, 12, Focus::Bezel, Settings{}, "", text, centered);
    bool saw_raw_filename = false;
    for (const auto& call : calls)
        if (call.text == labels[12] && call.y == 586) saw_raw_filename = true;
    assert(saw_raw_filename);
    assert(centered_calls.empty());
    assert(menu.data()[128 * pitch + 924] == color(0xf4dddd));
    assert(menu.data()[(selected_y + 23) * pitch + 50] == color(0xf3f3f3));
    menu.check_guards();
    menu.check_opaque();
    const auto rendered = menu.pixels;
    render_menu(nullptr, pitch, labels, 0, Focus::Games, Settings{}, "", text);
    render_menu(menu.data(), width - 1, labels, 0, Focus::Games, Settings{}, "", text);
    assert(menu.pixels == rendered);
    for (int selection : {-10, 22, INT_MAX}) {
        calls.clear();
        Settings settings;
        settings.bezel = 100;
        settings.show_fps = false;
        render_menu(menu.data(), pitch, labels, selection, Focus::Fps, settings, "", text);
        bool saw_off = false;
        for (const auto& call : calls) if (call.text == "关闭") saw_off = true;
        assert(saw_off);
        menu.check_guards();
    }

    for (int bezel = -1; bezel <= bezel_count; ++bezel) {
        Frame phone;
        calls.clear();
        render_bezel(phone.data(), pitch, bezel, text);
        assert(calls.empty());
        phone.check_guards();
        phone.check_opaque();
        for (int y = game_y; y < game_y + game_height; ++y)
            for (int x = game_x; x < game_x + game_width; ++x)
                assert(phone.data()[size_t(y) * pitch + x] == color(0));
        if (bezel <= 0 || bezel >= bezel_count)
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
                assert(phone.data()[size_t(y) * pitch + x] == color(0));
        else {
            // Every pixel outside the game has a skin color, including output
            // corners and the former 16-pixel top/bottom strips.
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
                if (x < game_x || x >= game_x + game_width || y < game_y || y >= game_y + game_height)
                    assert(phone.data()[size_t(y) * pitch + x] != color(0));
            assert(phone.data()[0] == color(bezel_palette(bezel).background));
            bool left_detail = false, right_detail = false;
            for (int y = 300; y < 430; ++y) {
                for (int x = 110; x < 240; ++x)
                    left_detail |= phone.data()[size_t(y) * pitch + x] != phone.data()[0];
                for (int x = 1040; x < 1170; ++x)
                    right_detail |= phone.data()[size_t(y) * pitch + x] != phone.data()[0];
            }
            assert(left_detail && right_detail);
        }
        const auto before_fps = phone.pixels;
        render_fps(phone.data(), pitch, bezel, "60.0", text);
        const FpsPlace box = place_fps("60.0", 0, 0, 0, 0, 0, 0, 0);
        assert(calls.size() == 2 && calls[0].text == "FPS " && calls[1].text == "60.0");
        assert(calls[0].x == box.prefix_x && calls[1].x == box.number_x);
        assert(calls[0].y == box.text_y && calls[1].y == box.text_y);
        assert(calls[0].height == fps_font && calls[1].height == fps_font);
        assert(box.x == fps_margin && box.y == fps_margin);
        const FpsPlace one = place_fps("5", 0, 0, 0, 0, 0, 0, 0);
        const FpsPlace two = place_fps("25", 0, 0, 0, 0, 0, 0, 0);
        assert(one.w == two.w && one.h == two.h);
        assert(one.prefix_x == two.prefix_x && one.text_y == two.text_y);
        assert(one.number_x == two.number_x + fps_fallback_width("8"));
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            if (!(x >= box.x && x < box.x + box.w && y >= box.y && y < box.y + box.h))
                assert(phone.data()[size_t(y) * pitch + x] == before_fps[prefix + size_t(y) * pitch + x]);
        phone.check_guards();
    }
    {
        calls.clear();
        Frame marked;
        std::vector<uint8_t> marks(labels.size(), 0);
        marks[0] = 1;
        render_menu(marked.data(), pitch, labels, 0, Focus::Games, Settings{}, "", text, {}, "原生", marks);
        marked.check_guards();
        bool saw_mark = false;
        for (const auto& call : calls) {
            if (call.text == "存") {
                saw_mark = true;
                assert(call.x == 82 + text_advance(labels[0], 24) + 8 + 6);
                assert(call.y == 124 + 15);
            }
            if (call.y == 586) assert(call.text == labels[0]);
        }
        assert(saw_mark);
        calls.clear();
        Frame noted;
        render_exit_sheet(noted.data(), pitch, text, {}, "已保存即时存档");
        noted.check_guards();
        constexpr int card_w = 360, card_h = 266;
        const int card_x = (width - card_w) / 2, card_y = (height - card_h) / 2;
        assert(noted.data()[size_t(card_y) * pitch + card_x] == color(0xf7f7f7));
        assert(noted.data()[size_t(card_y + card_h - 1) * pitch + card_x + 20] == color(0xf7f7f7));
        assert(noted.data()[size_t(card_y - 1) * pitch + card_x] != color(0xf7f7f7));
        bool saw_note = false, saw_delete = false;
        for (const auto& call : calls) {
            if (call.text == "已保存即时存档") {
                saw_note = true;
                assert(call.y == card_y + card_h - 24 && call.height == 16);
            }
            if (call.text == "删除即时存档") {
                saw_delete = true;
                assert(call.y == card_y + 72 + 46 - 12 && call.height == 24);
            }
        }
        assert(saw_note && saw_delete);
        calls.clear();
        Frame plain;
        render_exit_sheet(plain.data(), pitch, text, {}, "");
        assert(plain.data()[size_t(card_y) * pitch + card_x] == color(0xf7f7f7));
        assert(plain.data()[size_t(card_y + card_h - 1) * pitch + card_x + 20] == color(0xf7f7f7));
        for (const auto& call : calls) assert(call.text != "已保存即时存档");
    }
    std::puts("PASS HostMenuChecks: clipped buffers, menu, paging, centered keys, full-height skins, colored side motifs, unobstructed viewport and bounded matching FPS badge");
}
