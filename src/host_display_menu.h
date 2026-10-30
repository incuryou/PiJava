#pragma once
#include "host_menu.h"

namespace host_display_menu {

enum class Action { None, Inherit, Canvas, Rotation, Back, Manage, DeleteGame, DeleteInstant };

struct Model {
    bool open = false;
    int selection = 0;
    void begin() { open = true; selection = 0; }
    void move(int direction) {
        if (open) selection = std::max(0, std::min(2, selection + (direction > 0 ? 1 : direction < 0 ? -1 : 0)));
    }
    Action selected() const {
        constexpr Action actions[] = {Action::Inherit, Action::DeleteGame, Action::DeleteInstant};
        return open && selection >= 0 && selection < 3 ? actions[selection] : Action::None;
    }
};

inline void render(uint32_t* frame, size_t pitch, const Model& model,
                   const host_menu::Settings& settings, const host_menu::Text& text,
                   const host_menu::CenteredText& centered = {}) {
    if (!frame || pitch < size_t(host_menu::width) || !model.open) return;
    (void)centered;
    (void)settings;
    using host_menu::color;
    using host_menu::rect;
    rect(frame, pitch, 292, 190, 696, 340, color(0xe4e9ee));
    rect(frame, pitch, 296, 194, 688, 332, color(0xffffff));
    const auto ink = color(0x26313e), muted = color(0x718094), accent = color(0xc81e41);
    if (text) text(330, 216, "更多设置", ink, 26, 620);
    const char* names[] = {"继承存档", "删除游戏", "删除即时存档"};
    for (int row = 0; row < 3; ++row) {
        const int y = 272 + row * 62;
        const bool selected = row == model.selection;
        rect(frame, pitch, 326, y, 628, 50, color(selected ? 0xffe7ed : 0xf2f4f7));
        if (!text) continue;
        text(344, y + 13, names[row], selected ? accent : ink, 23, 260);
    }
    if (text) text(330, 493, "A 确认    B 返回", muted, 18, 620);
}

} // namespace host_display_menu
