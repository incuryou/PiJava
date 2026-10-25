#pragma once

#include <cstdio>
#include <fstream>
#include <map>
#include <string>

// Per-game drawing target. 0 uses the game's own pace. 30/60 share one
// presentation grid and can shorten eligible Java frame sleeps.
namespace host_speed {

inline int normalize(int value) { return value == 30 || value == 60 ? value : 0; }

inline int next(int value) {
    value = normalize(value);
    if (value == 0) return 30;
    if (value == 30) return 60;
    return 0;
}

inline const char* label(int value) {
    if (value == 30) return "30帧";
    if (value == 60) return "60帧";
    return "原生";
}

inline const char* log_name(int value) {
    if (value == 30) return "30";
    if (value == 60) return "60";
    return "native";
}

inline bool load(const std::string& path, std::map<std::string, int>& out) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) return false;
    std::string line;
    if (!std::getline(input, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "J2ME_SPEED 1") return false;
    std::map<std::string, int> loaded;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const std::size_t tab = line.rfind('\t');
        if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size()) return false;
        const std::string key = line.substr(0, tab);
        const std::string value = line.substr(tab + 1);
        if (key.find('\n') != std::string::npos || key.find('\t') != std::string::npos) return false;
        if (value != "0" && value != "30" && value != "60") return false;
        loaded[key] = value == "30" ? 30 : value == "60" ? 60 : 0;
    }
    if (!input.eof()) return false;
    out.swap(loaded);
    return true;
}

inline bool save(const std::string& path, const std::map<std::string, int>& speeds) {
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << "J2ME_SPEED 1\n";
    for (const auto& item : speeds) {
        if (item.first.empty() || item.first.find('\t') != std::string::npos ||
            item.first.find('\n') != std::string::npos) return false;
        output << item.first << '\t' << normalize(item.second) << '\n';
    }
    output.close();
    if (output.fail()) {
        std::remove(temporary.c_str());
        return false;
    }
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(path.c_str());
        if (std::rename(temporary.c_str(), path.c_str()) != 0) {
            std::remove(temporary.c_str());
            return false;
        }
    }
    return true;
}

} // namespace host_speed
