#pragma once
// 0.2.112 F2: one neutral host-menu line after a MIDlet ends itself (notifyDestroyed, System.exit, failed start).
// Text only: nothing here touches the game canvas, clocks or the MIDlet's behaviour.
#include <functional>
#include <string>
#include <vector>
namespace self_exit_hint {
inline const char* prefix() { return "游戏已自行退出"; }
inline const char* separator() { return "："; }
inline const char* ellipsis() { return "…"; }
constexpr size_t max_message_chars = 24;
// UTF-8 code points; a malformed byte counts as one unit so text is never split inside a valid sequence.
inline std::vector<std::string> code_points(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
        if (i + n > s.size()) n = 1;
        for (size_t k = 1; k < n; ++k) if ((static_cast<unsigned char>(s[i + k]) & 0xc0) != 0x80) { n = 1; break; }
        out.push_back(s.substr(i, n)); i += n;
    }
    return out;
}
// Trim, turn control characters into spaces and keep at most max_chars code points (… marks a cut).
inline std::string clean(const std::string& raw, size_t max_chars = max_message_chars) {
    std::string t = raw;
    for (char& ch : t) { const unsigned char c = static_cast<unsigned char>(ch); if (c < 0x20 || c == 0x7f) ch = ' '; }
    const size_t b = t.find_first_not_of(' '), e = t.find_last_not_of(' ');
    if (b == std::string::npos) return std::string();
    const auto cps = code_points(t.substr(b, e - b + 1));
    std::string out;
    for (size_t i = 0; i < cps.size() && i < max_chars; ++i) out += cps[i];
    if (cps.size() > max_chars) out += ellipsis();
    return out;
}
// The status line, shortened further (… appended) until measure(line) <= max_px.
inline std::string status(const std::string& last_message, const std::function<int(const std::string&)>& measure, int max_px) {
    const std::string base = prefix();
    std::string message = clean(last_message);
    if (message.empty()) return base;
    std::string line = base + separator() + message;
    if (!measure || measure(line) <= max_px) return line;
    auto cps = code_points(message);
    if (!cps.empty() && cps.back() == ellipsis()) cps.pop_back();
    while (!cps.empty()) {
        cps.pop_back();
        std::string cut; for (const auto& cp : cps) cut += cp;
        line = base + separator() + cut + ellipsis();
        if (measure(line) <= max_px) return line;
    }
    return base;
}
}  // namespace self_exit_hint
