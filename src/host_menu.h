#pragma once
#include "menu_actions.h"
#include "presentation/filters.h"
#include "presentation/viewport.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "switch_buttons.h"

// Native host UI only. Pitch is measured in uint32_t pixels, not bytes.
// The text callback owns glyph drawing and must honor maxWidth (in pixels).
namespace host_menu {

constexpr int width = 1280;
constexpr int height = 720;
constexpr int visible_rows = 9;
constexpr int bezel_count = 8;
// ID 0 remains reserved for old records, but is no longer selectable.
inline constexpr std::array<int,4> bezel_choices={{4,5,6,j2me_present::adaptive_bezel}};
inline int next_bezel(int current){
    if(current==0)return j2me_present::adaptive_bezel;
    for(size_t i=0;i<bezel_choices.size();++i)if(bezel_choices[i]==current)return bezel_choices[(i+1)%bezel_choices.size()];
    return bezel_choices[0];
}
constexpr int game_x = 400;
constexpr int game_y = 40;
constexpr int game_width = 480;
constexpr int game_height = 640;

struct Settings {
    int bezel = j2me_present::adaptive_bezel;
    bool show_fps = true;
    display_filter::Filter filter = display_filter::Filter::Original;
    int rotation = 0; // Clockwise quarter turns, independent of the bezel.
    int canvas = 0; // Selected game only; persisted by the host sidecar.
};

enum class Focus { Games = 0, Bezel = 1, Filter = 2, Speed = 3, Fps = 4, Rotation = 5 };

inline Focus option_step(Focus focus,int direction){
    constexpr Focus order[]={Focus::Bezel,Focus::Filter,Focus::Rotation,Focus::Speed,Focus::Fps};
    int index=0;
    for(int i=0;i<5;++i)if(order[i]==focus){index=i;break;}
    return order[std::max(0,std::min(4,index+(direction>0?1:direction<0?-1:0)))];
}
inline bool change_filter(Settings& settings,int direction){
    if(!direction)return false;
    const int current=int(display_filter::normalized(settings.filter));
    settings.filter=static_cast<display_filter::Filter>((current+(direction>0?1:display_filter::count-1))%display_filter::count);
    return true;
}

inline bool change_rotation(Settings& settings, int direction) {
    if (!direction) return false;
    const int current = settings.rotation >= 0 && settings.rotation < 4 ? settings.rotation : 0;
    settings.rotation = (current + (direction > 0 ? 1 : 3)) % 4;
    return true;
}
inline const char* rotation_name(int rotation) {
    constexpr const char* names[] = {"0°", "90°", "180°", "270°"};
    return names[rotation >= 0 && rotation < 4 ? rotation : 0];
}

inline constexpr int canvas_count = 17;
inline std::pair<int, int> canvas_size(int canvas) {
    constexpr std::pair<int, int> sizes[] = {{0,0},{128,128},{128,160},{176,205},{176,208},{176,220},{208,208},{240,320},{240,400},{320,240},{320,480},{352,416},{360,360},{360,640},{480,800},{640,360},{800,480}};
    return sizes[canvas >= 0 && canvas < canvas_count ? canvas : 0];
}
inline const char* canvas_name(int canvas) {
    if (canvas == -1) return "自定义（现有配置）";
    constexpr const char* names[] = {"自动", "128×128", "128×160", "176×205", "176×208", "176×220", "208×208", "240×320", "240×400", "320×240", "320×480", "352×416", "360×360", "360×640", "480×800", "640×360", "800×480"};
    return names[canvas >= 0 && canvas < canvas_count ? canvas : 0];
}
inline bool change_canvas(Settings& settings, int direction) {
    if (!direction) return false;
    if (settings.canvas == -1) {
        settings.canvas = direction > 0 ? 0 : canvas_count - 1;
        return true;
    }
    const int current = settings.canvas >= 0 && settings.canvas < canvas_count ? settings.canvas : 0;
    settings.canvas = (current + (direction > 0 ? 1 : canvas_count - 1)) % canvas_count;
    return true;
}

using Text = std::function<void(int x, int y, const std::string& text,
                               uint32_t packed, int pixelHeight, int maxWidth)>;
// Same signature, but x/y identify the center of the rendered glyph ink.
using CenteredText = Text;

inline uint32_t color(uint32_t rgb) {
    return 0xff000000u | ((rgb & 0xffu) << 16) | (rgb & 0xff00u) |
           ((rgb >> 16) & 0xffu);
}

inline void rect(uint32_t* frame, size_t pitch, int x, int y, int w, int h,
                 uint32_t packed) {
    if (!frame || pitch < size_t(width) || w <= 0 || h <= 0) return;
    const int64_t right = int64_t(x) + w;
    const int64_t bottom = int64_t(y) + h;
    const int left = int(std::max<int64_t>(0, x));
    const int top = int(std::max<int64_t>(0, y));
    const int clipped_right = int(std::min<int64_t>(width, right));
    const int clipped_bottom = int(std::min<int64_t>(height, bottom));
    if (left >= clipped_right || top >= clipped_bottom) return;
    for (int row = top; row < clipped_bottom; ++row) {
        uint32_t* begin = frame + size_t(row) * pitch + size_t(left);
        std::fill(begin, begin + (clipped_right - left), packed);
    }
}

inline const char* bezel_name(int bezel) {
    switch (bezel) {
    case 4: return "手柄米白";
    case 5: return "手柄蓝";
    case 6: return "手柄黑";
    case j2me_present::adaptive_bezel:return "自适应屏幕";
    default: return "自适应屏幕";
    }
}

// Status line under the list (P8.11-r2 geometry): same right edge as the list text (82 + 754 = 836), 17 px.
constexpr int status_px = 17;
constexpr int status_width = 82 + 754 - 48;

inline int text_advance(const std::string& text, int size) {
    int width = 0;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        int step = 1;
        if (c < 128) width += size / 2;
        else {
            width += size;
            if ((c & 0xe0) == 0xc0) step = 2;
            else if ((c & 0xf0) == 0xe0) step = 3;
            else if ((c & 0xf8) == 0xf0) step = 4;
        }
        if (i + size_t(step) > text.size()) break;
        i += size_t(step);
    }
    return width;
}

// P8.11: ping-pong scroll offset for a line wider than its box. Pauses 1.2 s at
// each end, moves 60 px/s. Returns 0 when the text fits. Pure function of time.
constexpr int scroll_pause_ms = 1200;
constexpr int scroll_px_per_s = 60;
inline int scroll_offset(int text_width, int box_width, int64_t ms) {
    const int over = text_width - box_width;
    if (over <= 0 || ms < 0) return 0;
    const int64_t move = int64_t(over) * 1000 / scroll_px_per_s;
    const int64_t cycle = 2 * (scroll_pause_ms + move);
    int64_t t = ms % cycle;
    if (t < scroll_pause_ms) return 0;
    t -= scroll_pause_ms;
    if (t < move) return int(t * scroll_px_per_s / 1000);
    t -= move;
    if (t < scroll_pause_ms) return over;
    t -= scroll_pause_ms;
    return over - int(t * scroll_px_per_s / 1000);
}

inline void render_menu(uint32_t* frame, size_t pitch,
                        const std::vector<std::string>& labels, int sel,
                        Focus focus, const Settings& settings,
                        const std::string& status, const Text& text,
                        const CenteredText& centered = CenteredText{},
                        const std::string& speed_label = "原生",
                        const std::vector<uint8_t>& saved = {},
                        const std::function<int(const std::string&, int)>& advance = {},
                        const std::vector<std::string>& details = {},
                        int64_t scroll_ms = 0,
                        bool* scrolling = nullptr,
                        bool status_error = false,
                        bool status_neutral = true) {
    if (scrolling) *scrolling = false;
    if (!frame || pitch < size_t(width)) return;
    const uint32_t background = color(0xf3f3f3);
    const uint32_t selected = color(0xf4dddd);
    const uint32_t foreground = color(0x262626);
    const uint32_t secondary = color(0x696969);
    const uint32_t line_color = color(0xd5d5d5);
    const uint32_t accent = color(0xd91526);
    const uint32_t on_accent = color(0xffffff);
    rect(frame, pitch, 0, 0, width, height, background);
    rect(frame, pitch, 0, 0, width, 6, accent);
    const auto label = [&](int x, int y, const std::string& s, uint32_t c,
                           int px, int max_width) {
        if (text && !s.empty()) text(x, y, s, c, px, max_width);
    };
    const auto measure = [&](const std::string& s, int px) {
        int adv = advance ? advance(s, px) : text_advance(s, px);
        return adv < 0 ? 0 : adv;
    };
    // Draws s inside [x, x+box) scrolled by the time-based offset. Pixels the
    // shifted text would put left of the box are restored, so nothing leaves
    // the line; the right edge is honoured through maxWidth as before.
    const auto scrolled = [&](int x, int y, const std::string& s, uint32_t c, int px, int box) {
        if (!text || s.empty() || box <= 0) return;
        const int w = measure(s, px);
        if (w > box && scrolling) *scrolling = true;  // keep repainting through the end pauses
        const int off = scroll_offset(w, box, scroll_ms);
        if (off <= 0) { text(x, y, s, c, px, box); return; }
        const int x0 = std::max(0, x - off), y0 = std::max(0, y - px), y1 = std::min(height, y + 2 * px);
        std::vector<uint32_t> keep(size_t(std::max(0, x - x0)) * size_t(std::max(0, y1 - y0)));
        for (int yy = y0, k = 0; yy < y1; ++yy) for (int xx = x0; xx < x; ++xx) keep[size_t(k++)] = frame[size_t(yy) * pitch + size_t(xx)];
        text(x - off, y, s, c, px, box + off);
        for (int yy = y0, k = 0; yy < y1; ++yy) for (int xx = x0; xx < x; ++xx) frame[size_t(yy) * pitch + size_t(xx)] = keep[size_t(k++)];
    };
    // Shapes use a one-pixel coverage boundary in the existing CPU buffer.
    const auto pixel = [&](int x, int y, uint32_t c, float coverage) {
        if (x < 0 || x >= width || y < 0 || y >= height || coverage <= 0) return;
        uint32_t& dst = frame[size_t(y) * pitch + size_t(x)];
        if (coverage >= 1) { dst = c; return; }
        uint32_t mixed = 0xff000000u;
        for (int shift = 0; shift < 24; shift += 8) {
            const float from = float((dst >> shift) & 255u);
            const float to = float((c >> shift) & 255u);
            mixed |= uint32_t(from + (to - from) * coverage + 0.5f) << shift;
        }
        dst = mixed;
    };
    const auto rounded = [&](int x, int y, int w, int h, float radius, uint32_t c) {
        const float cx = float(x) + float(w) * 0.5f;
        const float cy = float(y) + float(h) * 0.5f;
        const float inner_x = float(w) * 0.5f - radius;
        const float inner_y = float(h) * 0.5f - radius;
        for (int yy = y; yy < y + h; ++yy) {
            for (int xx = x; xx < x + w; ++xx) {
                const float dx = std::max(0.0f, std::fabs(float(xx) + 0.5f - cx) - inner_x);
                const float dy = std::max(0.0f, std::fabs(float(yy) + 0.5f - cy) - inner_y);
                const float coverage = std::min(1.0f, radius + 0.5f - std::sqrt(dx * dx + dy * dy));
                pixel(xx, yy, c, coverage);
            }
        }
    };
    // Kenney Nintendo Switch face buttons: one dark disk, white letter inside.
    const auto switch_key = [&](int cx, int cy, char key) {
        if(key=='+') {
            rounded(cx-15,cy-15,30,30,15.0f,color(0x2a2a2a));
            rect(frame,pitch,cx-7,cy-1,15,3,on_accent);
            rect(frame,pitch,cx-1,cy-7,3,15,on_accent);
            return;
        }
        constexpr int n = kSwitchButtonN;
        const uint32_t* icon = switch_button_icon(key);
        const int left = cx - n / 2;
        const int top = cy - n / 2;
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const uint32_t sample = icon[y * n + x];
                const unsigned alpha = sample >> 24;
                if (!alpha) continue;
                pixel(left + x, top + y, sample | 0xff000000u, alpha / 255.0f);
            }
        }
    };

    constexpr int header_center_y=57;
    if(centered)centered(48+measure("游戏",34)/2,header_center_y,"游戏",foreground,34,100);
    else label(48,40,"游戏",foreground,34,100);
    rect(frame, pitch, 164, 47, 1, 27, line_color);

    switch_key(1125, header_center_y, '+');
    if(centered)centered(1151+measure("更多设置",18)/2,header_center_y,"更多设置",secondary,18,95);
    else label(1151,header_center_y-9,"更多设置",secondary,18,95);
    rect(frame, pitch, 892, 124, 1, 466, line_color);

    const size_t count = labels.size();
    const size_t selected_index = count == 0 ? 0 :
        std::min<size_t>(sel < 0 ? 0 : size_t(sel), count - 1);
    const size_t first = (selected_index / visible_rows) * visible_rows;
    const std::string position = count == 0 ? "0 / 0" :
        std::to_string(selected_index + 1) + " / " + std::to_string(count);
    if(centered)centered(184+measure(position,20)/2,header_center_y,position,secondary,20,120);
    else label(184,header_center_y-10,position,secondary,20,120);

    if (count == 0) {
        label(82, 164, "暂无游戏", secondary, 24, 754);
        label(82, 211, "请将 JAR 游戏放入 /switch/j2me/games/", secondary, 20, 754);
    } else {
        for (int row = 0; row < visible_rows && first + size_t(row) < count; ++row) {
            const size_t index = first + size_t(row);
            const int y = 124 + row * 50;
            if (index == selected_index) {
                if (focus == Focus::Games) rounded(48, y, 812, 46, 12.0f, selected);
                rounded(64, y + 19, 8, 8, 4.0f, focus == Focus::Games ? accent : secondary);
            }
            const bool mark = index < saved.size() && saved[index];
            constexpr int badge = 28, gap = 8;
            const int name_limit = mark ? 754 - badge - gap : 754;
            if (index == selected_index) scrolled(82, y + 11, labels[index], foreground, 24, name_limit);
            else label(82, y + 11, labels[index], foreground, 24, name_limit);
            if (mark) {
                int adv = measure(labels[index], 24);
                if (adv > name_limit) adv = name_limit;
                const int bx = 82 + adv + gap;
                rounded(bx, y + 9, badge, 28, 8.0f, accent);
                label(bx + 6, y + 15, "存", on_accent, 16, badge - 8);
            }
        }
    }

    const auto option = [&](int y, Focus option_focus, const std::string& name,
                            const std::string& value, bool enabled, int max_width) {
        if (focus == option_focus) {
            rounded(908, y - 10, 332, 80, 12.0f, selected);
        }
        label(920, y, name, secondary, 20, 310);
        label(920, y + 37, value, enabled ? foreground : secondary, 24, max_width);
    };
    option(138, Focus::Bezel, "遮罩", bezel_name(settings.bezel), true, 272);
    rect(frame, pitch, 920, 214, 310, 1, line_color);
    option(226, Focus::Filter, "滤镜", display_filter::name(settings.filter), true, 272);
    rect(frame, pitch, 920, 302, 310, 1, line_color);
    option(314, Focus::Rotation, "画面旋转", rotation_name(settings.rotation), true, 272);
    rect(frame, pitch, 920, 390, 310, 1, line_color);
    option(402, Focus::Speed, "速度", speed_label.empty() ? "原生" : speed_label, true, 120);
    rect(frame, pitch, 920, 478, 310, 1, line_color);
    option(490, Focus::Fps, "显示帧数", settings.show_fps ? "开启" : "关闭", true, 190);
    rounded(1178, 526, 52, 30, 15.0f, settings.show_fps ? accent : secondary);
    rounded(settings.show_fps ? 1203 : 1182, 530, 22, 22, 11.0f, on_accent);

    // P8.11: no status -> the selected JAR's manifest name (details), else its label.
    // P8.11-r2: same right edge as the list text (82 + 754 = 836), left of the divider at 892.
    constexpr int detail_width = status_width;
    if (!status.empty()) label(48, 586, status, color(status_error?0xc81e41:status_neutral?0x718094:0x208351), status_px, detail_width);
    else if (count != 0)
        scrolled(48, 586, selected_index < details.size() && !details[selected_index].empty()
                              ? details[selected_index] : labels[selected_index], secondary, 20, detail_width);
    rect(frame, pitch, 48, 621, 1184, 1, line_color);
    // Match the approved preview: three compact hints, one confirm action.
    // Keep the same footer in either pane; X/B remain optional shortcuts.
    label(48, 653, "↑ ↓ 选择", secondary, 18, 90);
    label(160, 653, "← → 游戏 / 选项", secondary, 18, 148);
    label(326, 653, "L / R 翻页", secondary, 18, 148);

    rounded(936, 638, 140, 55, 27.5f, accent);
    switch_key(969, 665, 'X');
    label(995, 653, "继续", on_accent, 24, 65);
    rounded(1092, 638, 140, 55, 27.5f, accent);
    switch_key(1125, 665, 'A');
    label(1151, 653, "开始", on_accent, 24, 65);
}

inline void render_delete(uint32_t* frame,size_t pitch,const menu_actions::Deletion& deletion,
                          const Text& text,const CenteredText& centered) {
    if(!frame || pitch<size_t(width) || !deletion.open)return;
    // Reuse the launcher's rounded cards and controller icons.
    rect(frame,pitch,292,180,696,360,color(0xe4e9ee));
    rect(frame,pitch,296,184,688,352,color(0xffffff));
    const auto ink=color(0x26313e),muted=color(0x718094),red=color(0xc81e41);
    if(text){
        text(330,208,deletion.confirming?"确认删除":"游戏选项",ink,26,620);
        text(330,250,deletion.label,muted,20,620);
        if(deletion.confirming){
            text(330,305,deletion.selection==0?"删除此游戏 JAR？":"删除此游戏的即时存档？",red,23,620);
            text(330,350,deletion.selection==0?"游戏存档、映射和设置将保留。":"删除后将无法继续此即时存档。",muted,17,620);
            text(330,383,"此操作无法撤销，请确认后执行。",muted,17,620);
        }else{
            for(int i=0;i<3;++i){
                rect(frame,pitch,326,293+i*62,628,50,color(i==deletion.selection?0xffe7ed:0xf2f4f7));
                text(344,306+i*62,i==0?"删除游戏":i==1?"删除即时存档":"继承存档",i==deletion.selection?red:ink,23,588);
            }
        }
        text(330,479,"A 确认    B 返回",ink,18,620);
    }
}

inline void render_exit_sheet(uint32_t* frame, size_t pitch, const Text& text,
                              const CenteredText& centered, const std::string& note) {
    if (!frame || pitch < size_t(width)) return;
    (void)centered;
    const uint32_t card = color(0xf7f7f7);
    const uint32_t ink = color(0x262626);
    const uint32_t muted = color(0x696969);
    constexpr int w = 360;
    constexpr int h = 266;
    const int x = (width - w) / 2;
    const int y = (height - h) / 2;
    rect(frame, pitch, x, y, w, h, card);
    if (text) text(x + 28, y + 18, "即时存档", ink, 26, w - 56);
    const char* names[] = {"保存即时存档", "删除即时存档", "返回游戏", "退出游戏"};
    const char keys[] = {'A', 'X', 'Y', 'B'};
    for (int i = 0; i < 4; ++i) {
        const int cy = y + 72 + i * 46;
        const int cx = x + 44;
        constexpr int n = kSwitchButtonN;
        const uint32_t* icon = switch_button_icon(keys[i]);
        const int left = cx - n / 2, top = cy - n / 2;
        for (int row = 0; row < n; ++row) for (int col = 0; col < n; ++col) {
            const uint32_t sample = icon[row * n + col];
            const unsigned alpha = sample >> 24;
            if (!alpha) continue;
            const int xx = left + col, yy = top + row;
            if ((unsigned)xx >= (unsigned)width || (unsigned)yy >= (unsigned)height) continue;
            uint32_t& dst = frame[size_t(yy) * pitch + xx];
            if (alpha >= 255) { dst = sample | 0xff000000u; continue; }
            uint32_t mixed = 0xff000000u;
            for (int shift = 0; shift < 24; shift += 8) {
                const float from = float((dst >> shift) & 255u);
                const float to = float((sample >> shift) & 255u);
                mixed |= uint32_t(from + (to - from) * (alpha / 255.0f) + 0.5f) << shift;
            }
            dst = mixed;
        }
        if (text) text(x + 72, cy - 12, names[i], ink, 24, w - 96);
    }
    if (text && !note.empty()) text(x + 28, y + h - 24, note, muted, 16, w - 56);
}

struct BezelPalette { uint32_t body, edge, accent, key, background, panel, detail, ink; };

inline const BezelPalette& bezel_palette(int bezel) {
    static constexpr BezelPalette palettes[] = {
        {0, 0, 0, 0, 0, 0, 0, 0xffffff},
        {0xb5c2d0, 0x6e839b, 0x365778, 0xd5dde4, 0x293e55, 0x304860, 0x8199b0, 0xe7edf4},
        {0x2e3339, 0xa4abb2, 0x707983, 0xc2c8cd, 0x202932, 0x2b3540, 0x8995a1, 0xe9eef3},
        {0xd4d0c1, 0x969589, 0x646962, 0xe8e4d6, 0x4c5048, 0x5a5e54, 0xa3a692, 0xf1eee1}
    };
    if(bezel>=4&&bezel<=6)bezel=bezel==4?3:bezel==5?1:2;
    return palettes[bezel > 0 && bezel < 4 ? bezel : 0];
}

// Cached projection of the unchanged black controller's two 352x720 side
// panes. The game remains aspect-fit; narrow sidebars deliberately stay black.
struct UniversalPane {
    int source_x=0,x=0,y=0,w=0,h=0;
    int project_x(int source)const{return x+(source-source_x)*w/352;}
    int project_y(int source)const{return y+source*w/352;}
};
struct UniversalLayout {
    int source_w=0,source_h=0;
    j2me_present::Viewport viewport{0,0,0,0};
    std::array<UniversalPane,2> panes{};
    UniversalLayout(){update(240,320);}
    bool update(int w,int h){
        if(w==source_w&&h==source_h)return false;
        source_w=w;source_h=h;
        viewport=j2me_present::game_viewport(w,h,true);
        panes={};
        if(!j2me_present::valid_viewport(viewport))return true;
        for(int side=0;side<2;++side){
            auto& pane=panes[size_t(side)];pane.source_x=side?928:0;
            const int start=side?viewport.x+viewport.w:0;
            const int sidebar=side?width-start:viewport.x;
            if(sidebar<96)continue;
            pane.w=std::min(352,sidebar);pane.h=pane.w*height/352;
            pane.x=start+(sidebar-pane.w)/2;pane.y=(height-pane.h)/2;
        }
        return true;
    }
};

// Static decoration is painted only when entering a game / clearing the
// framebuffer ring. The exact 480x640 game viewport remains unobstructed.
inline void render_bezel(uint32_t* frame, size_t pitch, int bezel, const Text& text,
                         const uint32_t* texture = nullptr, size_t texture_count = 0,
                         const UniversalLayout* universal = nullptr) {
    if (!frame || pitch < size_t(width)) return;
    (void)text;
    if(bezel==j2me_present::adaptive_bezel){
        rect(frame,pitch,0,0,width,height,color(0));
        return;
    }
    const BezelPalette& p = bezel_palette(bezel);
    // Only a complete active skin can use a texture. In particular, no-bezel
    // remains black even if a caller accidentally passes an earlier skin.
    if (bezel > 0 && bezel < bezel_count && texture &&
        texture_count == size_t(width) * height) {
        for (int y = 0; y < height; ++y) {
            uint32_t* dst = frame + size_t(y) * pitch;
            const uint32_t* src = texture + size_t(y) * width;
            if (y < game_y || y >= game_y + game_height) {
                std::copy(src, src + width, dst);
            } else {
                std::copy(src, src + game_x, dst);
                std::fill(dst + game_x, dst + game_x + game_width, color(0));
                std::copy(src + game_x + game_width, src + width, dst + game_x + game_width);
            }
        }
        return;
    }
    rect(frame, pitch, 0, 0, width, height, color(p.background));
    if (bezel <= 0 || bezel >= bezel_count) return;
    const auto block = [&](int x, int y, int w, int h, uint32_t rgb) {
        rect(frame, pitch, x, y, w, h, color(rgb));
    };
    const auto rounded = [&](int x, int y, int w, int h, float radius, uint32_t rgb) {
        const uint32_t c = color(rgb);
        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) {
            if (xx < 0 || xx >= width || yy < 0 || yy >= height) continue;
            const float dx = std::max(0.0f, std::fabs(xx + 0.5f - cx) - (w * 0.5f - radius));
            const float dy = std::max(0.0f, std::fabs(yy + 0.5f - cy) - (h * 0.5f - radius));
            const float a = std::min(1.0f, radius + 0.5f - std::sqrt(dx * dx + dy * dy));
            if (a <= 0) continue;
            uint32_t& dst = frame[size_t(yy) * pitch + size_t(xx)];
            uint32_t out = 0xff000000u;
            for (int shift = 0; shift < 24; shift += 8) {
                const float from = float((dst >> shift) & 255u), to = float((c >> shift) & 255u);
                out |= uint32_t(from + (to - from) * a + 0.5f) << shift;
            }
            dst = out;
        }
    };
    // Native geometry is the bounded fallback for a missing/malformed asset.
    // It uses only the three current palettes and keeps the game area black.
    block(348, 0, 584, height, p.edge);
    block(354, 0, 572, height, p.body);
    rounded(384, 25, 512, 670, 14, p.accent);
    block(396, 36, 488, 648, 0x151719);
    block(game_x, game_y, game_width, game_height, 0);
    rounded(604, 15, 72, 6, 3, p.edge);
    rounded(604, 701, 72, 6, 3, p.edge);
    rounded(366, 280, 12, 160, 6, p.key);
    rounded(902, 280, 12, 160, 6, p.key);
    for (int x : {40, 972}) {
        block(x, 152, 268, 1, p.panel);
        block(x, 568, 268, 1, p.panel);
        for (int i = 0; i < 3; ++i) rounded(x + 108 + i * 18, 588, 10, 3, 1.5f, p.detail);
    }
    rounded(115, 302, 118, 118, 24, p.edge);
    rounded(119, 306, 110, 110, 22, p.panel);
    rounded(158, 322, 32, 78, 6, p.key);
    rounded(135, 345, 78, 32, 6, p.key);
    rounded(164, 351, 20, 20, 4, p.accent);
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 3; ++col) {
        const int x = 1046 + col * 44, y = 310 + row * 30;
        rounded(x, y, 32, 20, 6, p.edge);
        rounded(x + 2, y + 2, 28, 16, 5, p.panel);
        block(x + 11, y + 8, 10, 2, p.detail);
    }
}

constexpr int fps_font = 24;
constexpr int fps_margin = 4;
constexpr int fps_pad = 2;

struct FpsPlace {
    int x = 0, y = 0, w = 0, h = 0;
    int prefix_x = 0, number_x = 0, text_y = 0, max_width = 1;
};

inline int fps_fallback_width(const std::string& label) {
    int w = 0;
    for (unsigned char ch : label) w += ch < 128 ? fps_font / 2 : fps_font;
    return w;
}

// Background width is the ink of "FPS 88" and does not change for one digit.
// "FPS" stays on that two-digit origin. A single digit is drawn in the ones column.
inline FpsPlace place_fps(const std::string& value, int value_bearing, int prefix_advance,
                          int tens_advance, int slot_w, int slot_h, int slot_x, int slot_y) {
    if (prefix_advance <= 0 || tens_advance <= 0 || slot_w <= 0 || slot_h <= 0) {
        prefix_advance = fps_fallback_width("FPS ");
        tens_advance = fps_fallback_width("8");
        slot_w = fps_fallback_width("FPS 88");
        slot_h = fps_font;
        slot_x = 0;
        slot_y = 0;
        value_bearing = 0;
    }
    FpsPlace p;
    p.x = fps_margin;
    p.y = fps_margin;
    p.w = slot_w + fps_pad * 2;
    p.h = slot_h + fps_pad * 2;
    p.prefix_x = p.x + fps_pad - slot_x;
    p.text_y = p.y + fps_pad - slot_y;
    const int ones = value.size() == 1 ? tens_advance : 0;
    p.number_x = p.prefix_x + prefix_advance + ones - value_bearing;
    const int left = std::min(p.prefix_x, p.number_x);
    p.max_width = std::max(1, width - std::max(p.prefix_x, p.number_x));
    (void)left;
    return p;
}

inline void render_fps(uint32_t* frame, size_t pitch, int bezel,
                       const std::string& value, const Text& text,
                       int value_bearing = 0, int prefix_advance = 0, int tens_advance = 0,
                       int slot_w = 0, int slot_h = 0, int slot_x = 0, int slot_y = 0) {
    if (!frame || pitch < size_t(width)) return;
    const BezelPalette& p = bezel_palette(bezel);
    const FpsPlace box = place_fps(value, value_bearing, prefix_advance, tens_advance,
                                   slot_w, slot_h, slot_x, slot_y);
    rect(frame, pitch, box.x, box.y, box.w, box.h, color(p.panel));
    if (!text) return;
    text(box.prefix_x, box.text_y, "FPS ", color(p.ink), fps_font, box.max_width);
    text(box.number_x, box.text_y, value, color(p.ink), fps_font, box.max_width);
}

} // namespace host_menu
