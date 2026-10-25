#pragma once
// P8.3 (current build 0.2.77-p8.11-r2): speed.cfg keyed by JAR SHA256 ("sha256:<hex>") instead of full path.
// Old path keys are migrated once (jar hashed if it still exists); unresolvable keys are kept
// verbatim (preserved, ignored). speed.cfg.bak is written once before the first rewrite.
// Hash cache (speed-hash.cache next to speed.cfg): path+size+mtime unchanged -> reuse hash.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <sys/stat.h>
#include "host_speed.h"
#include "suite_identity.h"

namespace speed_keys {

struct CacheEntry { uint64_t size = 0; int64_t mtime = 0; std::string sha; };
struct State {
    std::string dir;                       // e.g. sdmc:/switch/j2me
    std::map<std::string, CacheEntry> cache;
    bool cache_dirty = false;
    std::map<std::string, std::string> memo; // per-session path->key, avoids per-frame stat
    int hashed = 0, hits = 0;              // stats for logging/tests
    std::string cfg() const { return dir + "/speed.cfg"; }
    std::string bak() const { return dir + "/speed.cfg.bak"; }
    std::string cache_path() const { return dir + "/speed-hash.cache"; }
};

inline bool is_hash_key(const std::string& k) {
    if (k.size() != 71 || k.compare(0, 7, "sha256:") != 0) return false;
    for (size_t i = 7; i < k.size(); ++i) { char c = k[i]; if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false; }
    return true;
}

inline void load_cache(State& s) {
    s.cache.clear();
    std::ifstream in(s.cache_path().c_str(), std::ios::binary);
    std::string line;
    if (!in || !std::getline(in, line) || line != "J2ME_HASHCACHE 1") return;
    while (std::getline(in, line)) {
        size_t a = line.rfind('\t'); if (a == std::string::npos || a == 0) continue;
        size_t b = line.rfind('\t', a - 1); if (b == std::string::npos || b == 0) continue;
        size_t c = line.rfind('\t', b - 1); if (c == std::string::npos || c == 0) continue;
        CacheEntry e; e.sha = line.substr(a + 1);
        if (e.sha.size() != 64) continue;
        e.size = std::strtoull(line.substr(c + 1, b - c - 1).c_str(), nullptr, 10);
        e.mtime = std::strtoll(line.substr(b + 1, a - b - 1).c_str(), nullptr, 10);
        s.cache[line.substr(0, c)] = e;
    }
}

inline bool save_cache(State& s) {
    if (!s.cache_dirty) return true;
    const std::string tmp = s.cache_path() + ".tmp";
    { std::ofstream out(tmp.c_str(), std::ios::binary | std::ios::trunc);
      if (!out) return false;
      out << "J2ME_HASHCACHE 1\n";
      for (auto& kv : s.cache) out << kv.first << '\t' << kv.second.size << '\t' << kv.second.mtime << '\t' << kv.second.sha << '\n';
      out.close(); if (out.fail()) { std::remove(tmp.c_str()); return false; } }
    if (std::rename(tmp.c_str(), s.cache_path().c_str()) != 0) {
        std::remove(s.cache_path().c_str());
        if (std::rename(tmp.c_str(), s.cache_path().c_str()) != 0) { std::remove(tmp.c_str()); return false; }
    }
    s.cache_dirty = false;
    return true;
}

// Returns "" if the jar cannot be read.
inline std::string jar_sha(State& s, const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return "";
    auto it = s.cache.find(path);
    if (it != s.cache.end() && it->second.size == (uint64_t)st.st_size && it->second.mtime == (int64_t)st.st_mtime) { s.hits++; return it->second.sha; }
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "";
    rms_suite_identity_detail::Sha256 h;
    static uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) h.update(buf, n);
    const bool err = std::ferror(f) != 0;
    std::fclose(f);
    if (err) return "";
    CacheEntry e; e.size = (uint64_t)st.st_size; e.mtime = (int64_t)st.st_mtime; e.sha = h.finish();
    s.cache[path] = e; s.cache_dirty = true; s.hashed++;
    return e.sha;
}

inline std::string key_for(State& s, const std::string& path) {
    auto m = s.memo.find(path); if (m != s.memo.end()) return m->second;
    const std::string h = jar_sha(s, path);
    const std::string k = h.empty() ? std::string() : "sha256:" + h;
    if (!k.empty()) s.memo[path] = k;
    return k;
}

inline bool copy_file(const std::string& from, const std::string& to) {
    std::ifstream in(from.c_str(), std::ios::binary); if (!in) return false;
    std::ofstream out(to.c_str(), std::ios::binary | std::ios::trunc); if (!out) return false;
    out << in.rdbuf(); out.close(); return !out.fail();
}
inline bool exists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }

struct MigrateResult { int migrated = 0, kept = 0; bool rewritten = false, bak_written = false, save_ok = true; };

// Write speed.cfg; speed.cfg.bak is created once (only if it does not exist yet) from the pre-rewrite file.
inline bool rewrite(State& s, const std::map<std::string, int>& speeds, bool* bak_written = nullptr) {
    if (exists(s.cfg()) && !exists(s.bak())) { bool ok = copy_file(s.cfg(), s.bak()); if (bak_written) *bak_written = ok; }
    return host_speed::save(s.cfg(), speeds);
}

inline MigrateResult migrate(State& s, std::map<std::string, int>& speeds) {
    MigrateResult r;
    std::map<std::string, int> out;
    for (auto& kv : speeds) if (is_hash_key(kv.first)) out[kv.first] = kv.second;
    for (auto& kv : speeds) {
        if (is_hash_key(kv.first)) continue;
        const std::string k = key_for(s, kv.first);
        if (k.empty()) { out[kv.first] = kv.second; r.kept++; continue; }   // unresolvable: preserved
        if (!out.count(k)) out[k] = kv.second;                             // existing hash entry wins
        r.migrated++;
    }
    if (r.migrated) { r.rewritten = true; r.save_ok = rewrite(s, out, &r.bak_written); }
    speeds.swap(out);
    return r;
}

inline int lookup(State& s, std::map<std::string, int>& speeds, const std::string& path) {
    if (speeds.empty()) return 0; // No per-game overrides: no JAR I/O just to display Native.
    const std::string k = key_for(s, path);
    auto it = k.empty() ? speeds.end() : speeds.find(k);
    return it == speeds.end() ? 0 : host_speed::normalize(it->second);
}

} // namespace speed_keys
