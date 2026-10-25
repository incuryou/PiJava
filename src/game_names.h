#pragma once
// Menu details from the JAR manifest and actual file size (display only).
// Reads only META-INF/MANIFEST.MF (central directory + that one entry), never
// the whole archive. Results are cached by path. Save/RMS/speed keys never use it.
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

extern "C" int puff(unsigned char* dest, unsigned long* destlen, const unsigned char* source, unsigned long* sourcelen);

namespace game_names {

inline uint32_t le16(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8); }
inline uint32_t le32(const uint8_t* p) { return le16(p) | (le16(p + 2) << 16); }

// Strict UTF-8 decode; returns false on any malformed sequence.
inline bool utf8_codepoints(const std::string& s, std::vector<uint32_t>* out) {
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        uint32_t cp; size_t n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; n = 2; }
        else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; n = 3; }
        else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; n = 4; }
        else return false;
        if (i + n > s.size()) return false;
        for (size_t k = 1; k < n; ++k) {
            const unsigned char d = (unsigned char)s[i + k];
            if ((d & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (d & 0x3f);
        }
        if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && (cp < 0x10000 || cp > 0x10ffff)) ||
            (cp >= 0xd800 && cp <= 0xdfff)) return false;
        if (out) out->push_back(cp);
        i += n;
    }
    return true;
}
inline bool has_cjk(const std::string& s) {
    std::vector<uint32_t> cps;
    if (!utf8_codepoints(s, &cps)) return false;
    for (uint32_t cp : cps)
        if ((cp >= 0x3400 && cp <= 0x9fff) || (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0x20000 && cp <= 0x3134f)) return true;
    return false;
}
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}
// Manifest attributes (BOM, CRLF and 72-byte continuation lines handled).
inline std::map<std::string, std::string> parse_manifest(const std::string& text) {
    std::map<std::string, std::string> props;
    std::string body = text;
    if (body.size() >= 3 && (unsigned char)body[0] == 0xef && (unsigned char)body[1] == 0xbb && (unsigned char)body[2] == 0xbf) body.erase(0, 3);
    std::vector<std::string> lines; std::string cur;
    for (size_t i = 0; i <= body.size(); ++i) {
        if (i == body.size() || body[i] == '\n') {
            if (!cur.empty() && cur.back() == '\r') cur.pop_back();
            if (!cur.empty() && cur[0] == ' ' && !lines.empty()) lines.back() += cur.substr(1);
            else lines.push_back(cur);
            cur.clear();
        } else cur += body[i];
    }
    for (const auto& l : lines) {
        const size_t c = l.find(':');
        if (c == std::string::npos || c == 0) continue;
        const std::string k = trim(l.substr(0, c));
        if (!props.count(k)) props[k] = trim(l.substr(c + 1));
    }
    return props;
}
inline std::string strip_jar(const std::string& path) {
    std::string base = path;
    const size_t slash = base.find_last_of('/');
    if (slash != std::string::npos) base = base.substr(slash + 1);
    if (base.size() > 4) {
        std::string ext = base.substr(base.size() - 4);
        for (char& ch : ext) ch = char(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
        if (ext == ".jar") base.resize(base.size() - 4);
    }
    return base;
}
// Rule: MIDlet-Name (UTF-8); if it has no CJK but MIDlet-1's first field does, use that;
// empty/invalid -> "" (caller falls back to the file name without .jar).
inline std::string choose_name(const std::map<std::string, std::string>& props) {
    auto get = [&](const char* k) { auto it = props.find(k); return it == props.end() ? std::string() : it->second; };
    std::string name = get("MIDlet-Name");
    if (!utf8_codepoints(name, nullptr)) name.clear();
    std::string m1 = get("MIDlet-1");
    const size_t comma = m1.find(',');
    if (comma != std::string::npos) m1 = m1.substr(0, comma);
    m1 = trim(m1);
    if (!has_cjk(name) && has_cjk(m1)) return m1;
    return name;
}

// Reads only the META-INF/MANIFEST.MF entry. Returns false when absent/unreadable.
// size_bytes is populated even if the ZIP/manifest is invalid; -1 means unavailable.
inline bool read_manifest_only(const std::string& path, std::string& out, uint64_t* size_bytes = nullptr) {
    if (size_bytes) *size_bytes = uint64_t(-1);
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    bool ok = false;
    std::vector<uint8_t> tail, central, local(30), comp;
    long size = 0;
    do {
        if (std::fseek(f, 0, SEEK_END) != 0) break;
        size = std::ftell(f);
        if (size >= 0 && size_bytes) *size_bytes = uint64_t(size);
        if (size < 22) break;
        const long tail_len = size < 65557 ? size : 65557;
        tail.resize(size_t(tail_len));
        if (std::fseek(f, size - tail_len, SEEK_SET) != 0 || std::fread(tail.data(), 1, tail.size(), f) != tail.size()) break;
        long eocd = -1;
        for (long i = tail_len - 22; i >= 0; --i)
            if (le32(&tail[size_t(i)]) == 0x06054b50u) { eocd = i; break; }
        if (eocd < 0) break;
        const uint32_t cd_size = le32(&tail[size_t(eocd) + 12]), cd_off = le32(&tail[size_t(eocd) + 16]);
        if (cd_off == 0xffffffffu || uint64_t(cd_off) + cd_size > uint64_t(size) || cd_size > (16u << 20)) break;
        central.resize(cd_size);
        if (std::fseek(f, long(cd_off), SEEK_SET) != 0 || std::fread(central.data(), 1, central.size(), f) != central.size()) break;
        size_t pos = 0; bool found = false; uint32_t method = 0, csize = 0, usize = 0, lofs = 0;
        while (pos + 46 <= central.size() && le32(&central[pos]) == 0x02014b50u) {
            const size_t nlen = le16(&central[pos + 28]), elen = le16(&central[pos + 30]), clen = le16(&central[pos + 32]);
            if (pos + 46 + nlen > central.size()) break;
            std::string name((const char*)&central[pos + 46], nlen);
            std::string up = name; for (char& ch : up) ch = char(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
            if (up == "META-INF/MANIFEST.MF") {
                if (le16(&central[pos + 8]) & 1) break; // encrypted
                method = le16(&central[pos + 10]); csize = le32(&central[pos + 20]);
                usize = le32(&central[pos + 24]); lofs = le32(&central[pos + 42]);
                found = true; break;
            }
            pos += 46 + nlen + elen + clen;
        }
        if (!found || usize > (1u << 20) || csize > (1u << 20) || uint64_t(lofs) + 30 > uint64_t(size)) break;
        if (std::fseek(f, long(lofs), SEEK_SET) != 0 || std::fread(local.data(), 1, 30, f) != 30 || le32(local.data()) != 0x04034b50u) break;
        const long data = long(lofs) + 30 + long(le16(&local[26])) + long(le16(&local[28]));
        if (uint64_t(data) + csize > uint64_t(size)) break;
        comp.resize(csize ? csize : 1);
        if (std::fseek(f, data, SEEK_SET) != 0 || (csize && std::fread(comp.data(), 1, csize, f) != csize)) break;
        if (method == 0) { out.assign((const char*)comp.data(), csize); ok = true; }
        else if (method == 8) {
            std::vector<uint8_t> raw(usize ? usize : 1);
            unsigned long dlen = raw.size(), slen = csize;
            if (puff(raw.data(), &dlen, comp.data(), &slen) != 0) break;
            out.assign((const char*)raw.data(), dlen); ok = true;
        }
    } while (false);
    std::fclose(f);
    return ok;
}

struct Cache {
    std::map<std::string, std::string> names, details;
    int reads = 0, hits = 0, misses = 0;
    // Display name for the menu detail line; file name without .jar if none.
    const std::string& get(const std::string& path) {
        auto it = names.find(path);
        if (it != names.end()) { ++hits; return it->second; }
        ++reads;
        std::string text, name, version;
        uint64_t bytes;
        if (read_manifest_only(path, text, &bytes)) {
            const auto props = parse_manifest(text);
            name = choose_name(props);
            const auto found = props.find("MIDlet-Version");
            if (found != props.end() && utf8_codepoints(found->second, nullptr)) version = found->second;
        }
        if (name.empty()) { name = strip_jar(path); ++misses; }
        details[path] = name + " · " + (version.empty() ? "版本未知" : "版本 " + version) + " · " +
            (bytes == uint64_t(-1) ? "大小未知" : std::to_string(bytes / 1024 + (bytes % 1024 != 0)) + " KB");
        return names[path] = name;
    }
    const std::string& get_detail(const std::string& path) {
        get(path);
        return details.find(path)->second;
    }
};

}  // namespace game_names
