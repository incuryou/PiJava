#pragma once
#include "game_names.h"

namespace host_text_input {
constexpr int keyboard_limit = 500;
struct Config {
    std::string title, initial;
    int max_size = 0, constraints = 0;
    bool multiline = false;
    bool single_bmp_name = false;
};
// The OS keyboard and Java strings count UTF-16 units, not UTF-8 bytes.
// NUL cannot survive the host keyboard's C-string boundary.
inline bool utf16_units(const std::string& value, size_t& out) {
    std::vector<uint32_t> codepoints;
    if (!game_names::utf8_codepoints(value, &codepoints)) return false;
    size_t units = 0;
    for (uint32_t cp : codepoints) {
        if (cp == 0) return false;
        units += cp > 0xffff ? 2 : 1;
    }
    out = units;
    return true;
}

inline bool valid(const std::string& value, int max_size, int constraints) {
    const int mode = constraints & 0xffff;
    // Match TextField.checkConstraints, then require an editable field.
    if (max_size <= 0 || mode > 5 || (constraints & ~0x3f0007) != 0 ||
        (constraints & 0x20000) != 0) return false;
    size_t units;
    if (!utf16_units(value, units) || units > size_t(max_size)) return false;
    if (mode != 2 && mode != 3 && mode != 5) return true;
    bool point = false;
    for (size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch >= '0' && ch <= '9') continue;
        if (mode == 3 && (ch == '+' || ch == '*' || ch == '#' || ch == ' ' ||
                         ch == '-' || ch == '(' || ch == ')')) continue;
        if (mode != 3 && ch == '-' && i == 0 && value.size() > 1) continue;
        if (mode == 5 && ch == '.' && !point && value.size() > 1) { point = true; continue; }
        return false;
    }
    return true;
}

inline bool valid_config(const std::string& value,const Config& config) {
    const int limit = config.max_size < keyboard_limit ? config.max_size : keyboard_limit;
    if(!valid(value,limit,config.constraints))return false;
    if(config.single_bmp_name) {
        std::vector<uint32_t> cps;
        if(!game_names::utf8_codepoints(value,&cps))return false;
        for(uint32_t cp:cps)if(cp<=0x20||(cp>=0x7f&&cp<=0x9f)||cp>0xffff)return false;
    }
    return true;
}
inline bool supported(const Config& config) {
    // Let legacy names open for correction without silently altering them.
    // Adapter-specific restrictions apply to new accepted values, not cancel.
    const int limit=config.max_size<keyboard_limit?config.max_size:keyboard_limit;
    return valid(config.initial,limit,config.constraints);
}
} // namespace host_text_input
