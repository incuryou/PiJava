#pragma once
// Host menu metadata only; never serialized into a Java instant state.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "game_names.h"

namespace menu_catalog {
struct State {
    game_names::Cache names;
    std::vector<std::string> details;
    std::vector<uint8_t> saved_known;
    bool dirty = true, name_pending = false;
    int selected = -1;

    template<class Entries>
    void rebuild(const Entries& items, std::vector<std::string>& labels,
                 std::vector<std::string>& paths, std::vector<uint8_t>& saved) {
        labels.clear(); paths.clear();
        labels.reserve(items.size()); paths.reserve(items.size());
        for (const auto& item : items) { labels.push_back(item.label); paths.push_back(item.path); }
        details.assign(items.size(), {}); saved.assign(items.size(), 0);
        saved_known.assign(items.size(), 0);
        names = game_names::Cache{}; // A rescan may replace a JAR at the same path.
        selected = -1; name_pending = false; dirty = false;
    }
    template<class Exists>
    void prepare_page(const std::vector<std::string>& paths, std::vector<uint8_t>& saved,
                      int sel, size_t rows, const Exists& exists) {
        if (paths.empty() || rows == 0) return;
        const size_t index = std::min<size_t>(std::max(0, sel), paths.size() - 1);
        const size_t first = index / rows * rows, end = std::min(paths.size(), first + rows);
        for (size_t i = first; i < end; ++i) if (!saved_known[i]) {
            saved[i] = exists(paths[i]) ? 1 : 0; saved_known[i] = 1;
        }
    }
    void invalidate_saved() { std::fill(saved_known.begin(), saved_known.end(), 0); }
    void select(const std::vector<std::string>& paths, int sel, int64_t /*now*/) {
        const int next = paths.empty() ? -1 : std::min<int>(std::max(0, sel), int(paths.size()) - 1);
        if (next == selected) return;
        selected = next; name_pending = false;
        if (selected < 0) return;
        const auto found = names.details.find(paths[size_t(selected)]);
        if (found != names.details.end()) details[size_t(selected)] = found->second;
        else name_pending = true;
    }
    bool poll_name(const std::vector<std::string>& paths, int64_t /*now*/, bool browsing = false) {
        // Continuous navigation defers only uncached I/O; stationary selection is immediate.
        if (!name_pending || browsing) return false;
        details[size_t(selected)] = names.get_detail(paths[size_t(selected)]);
        name_pending = false; return true;
    }
};
}
