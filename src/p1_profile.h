#pragma once

#include <cerrno>
#include <cstdio>
#include <string>

// A P1 file is parsed completely before any flags can become effective. This
// header performs no VM operations, Java allocations, logging, or exceptions.
namespace p1 {

enum Field : unsigned {
    Geometry = 1u << 0, Boundary = 1u << 1, Handoff = 1u << 2,
    Sprite = 1u << 3, Prep = 1u << 4, Float = 1u << 5,
    Diagnostics = 1u << 6
};

struct Profile {
    bool active = false;
    std::string id, error;
    bool geometry_native = false, host_boundary = false;
    bool completed_handoff = false, sprite_native = false;
    bool prep_native = false, float_batch = false, diagnostics = true;
    // Set only for accepted explicit fields; useful for requested/source logs.
    unsigned override_mask = 0;
};

constexpr std::size_t max_profile_bytes = 4096;
constexpr const char* switch_profile_path = "sdmc:/switch/j2me/p1-profile.cfg";

namespace detail {

inline Profile reject(const std::string& reason, unsigned line = 0) {
    Profile result;
    result.error = reason;
    if (line) result.error += " at line " + std::to_string(line);
    return result;
}

inline std::string trim(const std::string& value) {
    const std::size_t begin = value.find_first_not_of(" \t\r");
    if (begin == std::string::npos) return {};
    const std::size_t end = value.find_last_not_of(" \t\r");
    return value.substr(begin, end - begin + 1);
}

inline bool boolean(const std::string& value, bool& out) {
    if (value == "1" || value == "true") { out = true; return true; }
    if (value == "0" || value == "false") { out = false; return true; }
    return false;
}

inline bool builtin(const std::string& id, Profile& out) {
    struct Builtin {
        const char* id;
        bool geometry, boundary, handoff, sprite, prep, floating;
    };
    static const Builtin builtins[] = {
        {"P1-R", true, true, false, false, false, false},
        {"P1-C", false, false, true, true, true, true},
        {"P1-M00", true, false, false, true, true, true},
        {"P1-M01", true, false, true, true, true, true},
        {"P1-M10", true, true, false, true, true, true},
        {"P1-M11", true, true, true, true, true, true}
    };
    for (const Builtin& value : builtins) {
        if (id != value.id) continue;
        out = Profile{};
        out.id = id;
        out.geometry_native = value.geometry;
        out.host_boundary = value.boundary;
        out.completed_handoff = value.handoff;
        out.sprite_native = value.sprite;
        out.prep_native = value.prep;
        out.float_batch = value.floating;
        out.diagnostics = true;
        return true;
    }
    return false;
}

} // namespace detail

// Public pure parser also supports desktop checks without touching VM state.
inline Profile parse_profile(const std::string& record) {
    if (record.size() > max_profile_bytes) return detail::reject("profile exceeds 4096 bytes");
    for (unsigned char value : record) {
        if (value == 0 || (value < 32 && value != '\n' && value != '\r' && value != '\t') || value == 127)
            return detail::reject("profile contains invalid control byte");
    }
    static const char* keys[] = {
        "profile", "geometry_native", "host_boundary", "completed_handoff",
        "sprite_native", "prep_native", "float_batch", "diagnostics",
        "schema_version", "texture_native", "span_native", "row_native",
        "jit_requested", "renderer", "speed"
    };
    bool seen[15] = {};
    bool overrides[7] = {};
    std::string id;
    unsigned line_number = 0;
    std::size_t offset = 0;
    while (offset < record.size()) {
        ++line_number;
        std::size_t end = record.find('\n', offset);
        if (end == std::string::npos) end = record.size();
        std::string line = record.substr(offset, end - offset);
        offset = end + 1;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        line = detail::trim(line);
        if (line.empty()) continue;
        const std::size_t equal = line.find('=');
        if (equal == std::string::npos || line.find('=', equal + 1) != std::string::npos)
            return detail::reject("expected one key=value pair", line_number);
        const std::string key = detail::trim(line.substr(0, equal));
        const std::string value = detail::trim(line.substr(equal + 1));
        if (key.empty() || value.empty()) return detail::reject("empty key or value", line_number);
        unsigned index = 0;
        while (index < 15 && key != keys[index]) ++index;
        if (index == 15) return detail::reject("unknown profile key", line_number);
        if (seen[index]) return detail::reject("duplicate profile key", line_number);
        seen[index] = true;
        if (index == 0) id = value;
        else if (index <= 7) {
            if (!detail::boolean(value, overrides[index - 1]))
                return detail::reject("expected boolean 0/1/true/false", line_number);
        } else if (index == 8) {
            if (value != "1") return detail::reject("unsupported profile schema", line_number);
        } else if (index <= 12) {
            bool fixed = false;
            if (!detail::boolean(value, fixed) || fixed != (index != 12))
                return detail::reject("P1 fixed texture/span/row=1 and jit_requested=0", line_number);
        } else if ((index == 13 && value != "cpu") || (index == 14 && value != "native"))
            return detail::reject("P1 fixed renderer=cpu and speed=native", line_number);
    }
    if (!seen[0]) return detail::reject("missing required profile key");
    Profile result;
    if (!detail::builtin(id, result)) return detail::reject("unknown P1 profile id");
    bool* fields[] = {
        &result.geometry_native, &result.host_boundary, &result.completed_handoff,
        &result.sprite_native, &result.prep_native, &result.float_batch, &result.diagnostics
    };
    for (unsigned i = 0; i < 7; ++i) if (seen[i + 1]) {
        *fields[i] = overrides[i];
        result.override_mask |= 1u << i;
    }
    result.active = true;
    return result;
}

// A missing file leaves legacy configuration active (empty error). Any other
// failure returns an explicit error for the caller's BOOT rejection log.
inline Profile read_profile(const std::string& path) {
    if (path.empty() || path.find('\0') != std::string::npos)
        return detail::reject("invalid profile path");
    errno = 0;
    std::FILE* input = std::fopen(path.c_str(), "rb");
    if (!input) {
        if (errno == ENOENT) return Profile{};
        return detail::reject("cannot open profile errno=" + std::to_string(errno));
    }
    char buffer[max_profile_bytes + 1];
    const std::size_t size = std::fread(buffer, 1, sizeof(buffer), input);
    const bool failed = std::ferror(input) != 0;
    const bool close_failed = std::fclose(input) != 0;
    if (failed || close_failed) return detail::reject("profile read/close failed");
    if (size > max_profile_bytes) return detail::reject("profile exceeds 4096 bytes");
    return parse_profile(std::string(buffer, size));
}

} // namespace p1
