#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <sys/statvfs.h>
#endif

namespace j2me_rms {
// Keep filename decoding exact: storage encodes every byte of the VM's
// UTF-16/CESU-8 string. In particular, preserve surrogate code units and NUL.
inline bool decode_name(const std::string& file, std::string& name, std::string& primary) {
    primary = file;
    const std::string journal = ".previous";
    if (primary.size() >= journal.size() && primary.compare(primary.size() - journal.size(), journal.size(), journal) == 0)
        primary.resize(primary.size() - journal.size());
    if (primary.size() < 7 || primary[0] != 'r' || primary.compare(primary.size() - 4, 4, ".bin") != 0) return false;
    size_t end = primary.size() - 4;
    if ((end - 1) % 2 != 0 || end - 1 > 32 * 3 * 2) return false;
    auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    name.clear();
    for (size_t i = 1; i < end; i += 2) {
        int a = digit(primary[i]), b = digit(primary[i + 1]);
        if (a < 0 || b < 0) return false;
        name.push_back(char(a * 16 + b));
    }
    unsigned units = 0;
    for (size_t i = 0; i < name.size(); ++units) {
        unsigned char c = name[i++];
        if (c < 128) continue;
        unsigned extra = c >= 0xc0 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : 0;
        if (extra == 0 || name.size() - i < extra) return false;
        unsigned cp = c & (extra == 1 ? 31 : 15);
        for (unsigned j = 0; j < extra; ++j) {
            unsigned char b = name[i++];
            if ((b & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (b & 63);
        }
        // Modified UTF-8 NUL is also used by class-file string constants.
        if (extra == 1 ? cp < 128 && cp != 0 : cp < 2048) return false;
    }
    return units >= 1 && units <= 32;
}

inline bool valid_snapshot(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0 || uint64_t(st.st_size) > INT32_MAX) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    uint64_t position = 0, size = uint64_t(st.st_size);
    auto read_word = [&](uint32_t& value) {
        unsigned char bytes[4];
        if (position > size || size - position < 4 || !in.read(reinterpret_cast<char*>(bytes), 4)) return false;
        position += 4;
        value = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | bytes[3];
        return true;
    };
    if (size == 0) return true; // The old format allows an empty record store.
    uint32_t first;
    if (!read_word(first)) return false;
    bool modern = first == 0xd24d5332;
    uint32_t slots = 0;
    if (modern) {
        uint32_t format, next, ignored;
        if (!read_word(format) || format != 1 || !read_word(next) || !read_word(ignored)
            || !read_word(ignored) || !read_word(ignored) || !read_word(slots)) return false;
        if (next < 1 || next > INT32_MAX || slots != next - 1 || slots > (size - position) / 4) return false;
    }
    uint32_t seen = 0;
    bool pending_first = !modern;
    while (pending_first || position < size) {
        uint32_t length;
        if (pending_first) { length = first; pending_first = false; }
        else if (!read_word(length)) return false;
        if (modern && seen++ >= slots) return false;
        if (modern && length == UINT32_MAX) continue;
        if (length > INT32_MAX || length > size - position) return false;
        position += length;
        in.seekg(std::streamoff(position), std::ios::beg);
        if (!in) return false;
    }
    return position == size && (!modern || seen == slots) && in.peek() == std::char_traits<char>::eof() && !in.bad();
}

inline bool list_stores(const std::string& directory, std::vector<std::string>& names) {
    names.clear();
    DIR* dir = opendir(directory.c_str());
    if (!dir) return false;
    std::vector<std::string> candidates;
    bool ok = true;
    for (;;) {
        errno = 0;
        dirent* entry = readdir(dir);
        if (!entry) { ok = errno == 0; break; }
        candidates.emplace_back(entry->d_name);
    }
    if (closedir(dir) != 0) ok = false;
    if (!ok) return false;
    for (const auto& file : candidates) {
        std::string name, primary;
        if (!decode_name(file, name, primary)) continue;
        const std::string path = directory + "/" + primary;
        struct stat st;
        // A primary, even if invalid, takes precedence over an old journal.
        int status = stat(path.c_str(), &st);
        if (status != 0 && errno != ENOENT) continue;
        const std::string source = status == 0 ? path : path + ".previous";
        if (!valid_snapshot(source)) continue;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return true;
}

inline bool free_bytes(const std::string& directory, uint64_t& bytes, uint64_t& block) {
#ifdef __SWITCH__
    FsFileSystem* fs = nullptr;
    char path[FS_MAX_PATH];
    if (fsdevTranslatePath(directory.c_str(), &fs, path) != 0 || !fs) return false;
    s64 available = 0;
    if (R_FAILED(fsFsGetFreeSpace(fs, path, &available)) || available < 0) return false;
    bytes = uint64_t(available);
    block = 1; // libnx reports free bytes, without an allocation-unit query.
#else
    struct statvfs space;
    if (statvfs(directory.c_str(), &space) != 0 || (space.f_flag & ST_RDONLY)) return false;
    block = space.f_frsize ? space.f_frsize : space.f_bsize;
    if (block == 0) return false;
    uint64_t blocks = uint64_t(space.f_bavail);
    bytes = blocks > UINT64_MAX / block ? UINT64_MAX : blocks * block;
#endif
    return true;
}
inline int growth_available(uint64_t bytes, uint64_t block, int current_size, uint64_t legacy_backup = 0) {
    if (current_size < 0 || block == 0) return 0;
    auto round = [block](uint64_t size) { return size / block * block + (size % block ? block : 0); };
    // Atomic writes need a full new snapshot alongside the old one. First
    // legacy migration also preserves an exact rollback copy of the old file.
    uint64_t reserve = round(uint64_t(current_size)) + round(legacy_backup);
    uint64_t growth = bytes > reserve ? bytes - reserve : 0;
    uint64_t format_limit = uint64_t(INT32_MAX) - uint64_t(current_size);
    return int(std::min(growth, format_limit));
}
}
