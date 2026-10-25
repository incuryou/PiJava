#pragma once

#include <cerrno>
#include <cstdio>
#include <string>

// Keep a complete old record until the replacement commits. FAT may reject
// replacement with EEXIST; a missing main file recovers the journal on load.
namespace instant_state_file {
enum class Status { missing, readable, error };
inline Status status(const std::string& path) {
    errno = 0;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return errno == ENOENT ? Status::missing : Status::error;
    std::fgetc(f);
    bool failed = std::ferror(f) != 0;
    failed = std::fclose(f) != 0 || failed;
    return failed ? Status::error : Status::readable;
}
inline int rename_file(const std::string& a, const std::string& b) {
#ifdef INSTANT_STATE_RENAME
    return INSTANT_STATE_RENAME(a.c_str(), b.c_str());
#else
    return std::rename(a.c_str(), b.c_str());
#endif
}
inline int close_output(FILE* f) {
#ifdef INSTANT_STATE_CLOSE
    return INSTANT_STATE_CLOSE(f);
#else
    return std::fclose(f);
#endif
}
inline size_t write_output(const std::string& bytes, FILE* f) {
#ifdef INSTANT_STATE_WRITE
    return INSTANT_STATE_WRITE(bytes.data(), 1, bytes.size(), f);
#else
    return std::fwrite(bytes.data(), 1, bytes.size(), f);
#endif
}
inline int flush_output(FILE* f) {
#ifdef INSTANT_STATE_FLUSH
    return INSTANT_STATE_FLUSH(f);
#else
    return std::fflush(f);
#endif
}
inline bool read(const std::string& path, std::string& bytes, bool& missing) {
    errno = 0;
    FILE* f = std::fopen(path.c_str(), "rb");
    missing = !f && errno == ENOENT;
    if (!f) return false;
    std::string result;
    char block[16384];
    size_t n;
    // Bound malformed files before allocating their full contents.
    constexpr size_t limit = 256u * 1024u * 1024u;
    bool too_large = false;
    while ((n = std::fread(block, 1, sizeof(block), f)) != 0) {
        if (n > limit - result.size()) { too_large = true; break; }
        result.append(block, n);
    }
    bool failed = std::ferror(f) != 0 || too_large;
    failed = std::fclose(f) != 0 || failed;
    if (failed) return false;
    bytes.swap(result);
    return true;
}
inline bool load(const std::string& path, std::string& bytes) {
    bool missing = false;
    if (read(path, bytes, missing)) return true;
    return missing && read(path + ".previous", bytes, missing);
}
inline bool save(const std::string& path, const std::string& bytes) {
    if (path.empty() || path.find('\0') != std::string::npos) return false;
    const std::string old = path + ".previous", tmp = path + ".tmp";
    Status main = status(path);
    if (main == Status::error) return false;
    if (main == Status::missing) {
        Status prior = status(old);
        if (prior == Status::error ||
            (prior == Status::readable && rename_file(old, path) != 0)) return false;
    }
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool failed = write_output(bytes, f) != bytes.size();
    failed = flush_output(f) != 0 || failed;
    failed = close_output(f) != 0 || failed;
    if (failed) { std::remove(tmp.c_str()); return false; }
    if (rename_file(tmp, path) != 0) {
        const int error = errno;
        bool committed = false;
        if (error == EEXIST && status(path) == Status::readable) {
            Status prior = status(old);
            bool ready = prior == Status::missing ||
                (prior == Status::readable && std::remove(old.c_str()) == 0);
            if (ready && rename_file(path, old) == 0) {
                if (rename_file(tmp, path) == 0) committed = true;
                else rename_file(old, path); // If rollback fails, load recovers old.
            }
        }
        if (!committed) { std::remove(tmp.c_str()); return false; }
    }
    if (status(old) == Status::readable) std::remove(old.c_str());
    return true;
}
} // namespace instant_state_file
