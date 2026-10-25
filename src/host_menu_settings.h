#pragma once

#include "host_menu.h"
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>

namespace host_menu {

namespace settings_detail {

enum class FileStatus { missing, readable, error };

// stdio reports ENOENT separately from an unreadable path. A one-byte probe
// also rejects a directory on platforms where fopen itself permits it.
inline FileStatus file_status(const std::string& path) {
    errno = 0;
    std::FILE* input = std::fopen(path.c_str(), "rb");
    if (!input) return errno == ENOENT ? FileStatus::missing : FileStatus::error;
    std::fgetc(input);
    const bool failed = std::ferror(input) != 0;
    const bool close_failed = std::fclose(input) != 0;
    return failed || close_failed ? FileStatus::error : FileStatus::readable;
}

inline int rename_file(const std::string& source, const std::string& target) {
#ifdef HOST_MENU_SETTINGS_RENAME
    // Standalone checks can inject filesystem failures without a runtime hook.
    return HOST_MENU_SETTINGS_RENAME(source.c_str(), target.c_str());
#else
    return std::rename(source.c_str(), target.c_str());
#endif
}

inline bool read_settings_file(const std::string& path, Settings& out, bool& missing) {
    missing = false;
    errno = 0;
    std::FILE* input = std::fopen(path.c_str(), "rb");
    if (!input) { missing = errno == ENOENT; return false; }
    // Read one byte beyond the limit without allocating by file size.
    char buffer[1025];
    const std::size_t size = std::fread(buffer, 1, sizeof(buffer), input);
    const bool failed = std::ferror(input) != 0;
    const bool close_failed = std::fclose(input) != 0;
    if (failed || close_failed || size > 1024) return false;
    const std::string record(buffer, size);
    const bool legacy = record.compare(0, 12, "J2ME_MENU 1\n") == 0;
    const std::string prefix = legacy ? "J2ME_MENU 1\nbezel=" : "J2ME_MENU 2\nbezel=";
    const int record_count = legacy ? 7 : bezel_count;
    if (record.compare(0, prefix.size(), prefix) != 0) return false;
    const std::size_t bezel_end = record.find('\n', prefix.size());
    if (bezel_end == std::string::npos || bezel_end == prefix.size()) return false;
    // Decimal digits only; cap each step before it can overflow.
    int bezel = 0;
    for (std::size_t i = prefix.size(); i < bezel_end; ++i) {
        if (record[i] < '0' || record[i] > '9') return false;
        if (bezel > (record_count - 1) / 10) return false;
        bezel = bezel * 10 + (record[i] - '0');
        if (bezel >= record_count) return false;
    }
    if (bezel < 0 || bezel >= record_count) return false;
    const std::string suffix = record.substr(bezel_end + 1);
    const std::string fps=suffix.substr(0,11);
    if (fps != "show_fps=0\n" && fps != "show_fps=1\n") return false;
    const std::string extra=suffix.substr(11);
    display_filter::Filter filter=display_filter::Filter::Original;
    if(!extra.empty()){
        if(legacy||extra.compare(0,7,"filter=")!=0)return false;
        // A damaged optional filter never discards already validated old
        // bezel/FPS preferences. Incomplete/unknown values select original.
        bool valid=extra.size()>8&&extra.back()=='\n';int value=0;
        for(size_t i=7;valid&&i+1<extra.size();++i){
            if(extra[i]<'0'||extra[i]>'9'){valid=false;break;}
            value=value*10+(extra[i]-'0');if(value>=display_filter::count)valid=false;
        }
        if(valid)filter=static_cast<display_filter::Filter>(value);
    }
    Settings loaded = out;
    // Retired v1 skins have no identity in the new catalog. Preserve FPS
    // but migrate the bezel to none instead of assigning a different phone.
    loaded.bezel = legacy ? 0 : bezel;
    loaded.show_fps = fps == "show_fps=1\n";
    loaded.filter = filter;
    out = loaded;
    return true;
}

} // namespace settings_detail

// Only a missing main file can recover the previous complete snapshot.
// Malformed, future-version, or unreadable main files preserve the caller.
inline bool load_settings(const std::string& path, Settings& out) {
    if (path.empty() || path.find('\0') != std::string::npos) return false;
    bool missing = false;
    if (settings_detail::read_settings_file(path, out, missing)) return true;
    if (!missing) return false;
    return settings_detail::read_settings_file(path + ".previous", out, missing);
}

// POSIX replacement is atomic. FAT/libnx can report EEXIST instead: journal
// the old file before committing, and restore it if the new commit fails.
// An interruption after the move is recovered by load_settings on next launch.
inline bool save_record(const std::string& path, const std::string& record) {
    if (path.empty() || path.find('\0') != std::string::npos) return false;
    using settings_detail::FileStatus;
    const std::string previous = path + ".previous";
    FileStatus main_status = settings_detail::file_status(path);
    if (main_status == FileStatus::error) return false;
    if (main_status == FileStatus::missing) {
        const FileStatus prior_status = settings_detail::file_status(previous);
        if (prior_status == FileStatus::error) return false;
        if (prior_status == FileStatus::readable &&
            settings_detail::rename_file(previous, path) != 0) return false;
    }
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(record.data(), static_cast<std::streamsize>(record.size()));
    output.close();
    if (output.fail()) {
        std::remove(temporary.c_str());
        return false;
    }
    if (settings_detail::rename_file(temporary, path) != 0) {
        const int failure = errno;
        bool committed = false;
        if (failure == EEXIST &&
            settings_detail::file_status(path) == FileStatus::readable) {
            const FileStatus prior_status = settings_detail::file_status(previous);
            const bool ready = prior_status == FileStatus::missing ||
                (prior_status == FileStatus::readable &&
                 std::remove(previous.c_str()) == 0);
            if (ready && settings_detail::rename_file(path, previous) == 0) {
                if (settings_detail::rename_file(temporary, path) == 0) {
                    committed = true;
                } else {
                    // Even if rollback fails, .previous remains a complete
                    // readable record that the missing-main loader recovers.
                    settings_detail::rename_file(previous, path);
                }
            }
        }
        if (!committed) {
            std::remove(temporary.c_str());
            return false;
        }
    }
    // A complete main snapshot supersedes the journal. Never remove an
    // unreadable/directory path here; it was not our previous snapshot.
    if (settings_detail::file_status(previous) == FileStatus::readable)
        std::remove(previous.c_str());
    return true;
}

inline bool save_settings(const std::string& path, const Settings& settings) {
    if (settings.bezel < 0 || settings.bezel >= bezel_count || !display_filter::valid(settings.filter)) return false;
    std::string record = "J2ME_MENU 2\nbezel=" +
        std::to_string(settings.bezel) + "\nshow_fps=" +
        (settings.show_fps ? "1\n" : "0\n");
    if(settings.filter!=display_filter::Filter::Original)record+="filter="+std::to_string(int(settings.filter))+"\n";
    return save_record(path, record);
}

} // namespace host_menu
