#include <cerrno>
#include <cstdio>
#include <string>
#include <sys/stat.h>
static bool nonreplacing_rename = false;
static bool deny_new_commit = false;
static bool deny_rollback = false;
static bool deny_backup_move = false;
static int settings_test_rename(const char* source, const char* target);
#define HOST_MENU_SETTINGS_RENAME settings_test_rename
#include "../src/host_menu_settings.h"
#undef HOST_MENU_SETTINGS_RENAME
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>

static bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}
static int settings_test_rename(const char* source, const char* target) {
    struct stat existing;
    const bool target_exists = stat(target, &existing) == 0;
    if (nonreplacing_rename && target_exists) { errno = EEXIST; return -1; }
    if ((deny_new_commit && ends_with(source, ".tmp") && !target_exists) ||
        (deny_rollback && ends_with(source, ".previous")) ||
        (deny_backup_move && ends_with(target, ".previous"))) {
        errno = EIO;
        return -1;
    }
    return std::rename(source, target);
}

static int checks = 0;
static void require(bool ok, const char* reason) {
    ++checks;
    if (!ok) { std::cerr << "FAIL HostMenuSettingsChecks " << reason << '\n'; std::exit(1); }
}
static void write_bytes(const std::string& path, const std::string& bytes) {
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    require(!output.fail(), "write fixture");
}
static std::string read_bytes(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    require(static_cast<bool>(input), "read fixture");
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
static void reject_record(const std::string& path, const std::string& record) {
    write_bytes(path, record);
    host_menu::Settings settings;
    settings.bezel = host_menu::bezel_count - 1;
    settings.show_fps = false;
    require(!host_menu::load_settings(path, settings), "malformed record rejected");
    require(settings.bezel == host_menu::bezel_count - 1 && !settings.show_fps,
            "failed load preserves caller settings");
}

int main() {
    char pattern[] = "/tmp/j2me-menu-settings-XXXXXX";
    char* directory = mkdtemp(pattern);
    require(directory != nullptr, "isolated temporary directory");
    const std::string path = std::string(directory) + "/menu.cfg";
    host_menu::Settings defaults;
    require(defaults.bezel == 4 && defaults.show_fps, "new settings default to ivory Pro and visible FPS");
    require(!host_menu::load_settings(path, defaults), "missing record rejected");
    require(defaults.bezel == 4 && defaults.show_fps, "missing record preserves source defaults");

    for (int bezel = 0; bezel < host_menu::bezel_count; ++bezel) {
        for (bool visible : {false, true}) {
            host_menu::Settings expected;
            expected.bezel = bezel;
            expected.show_fps = visible;
            require(host_menu::save_settings(path, expected), "save and overwrite record");
            host_menu::Settings actual;
            require(host_menu::load_settings(path, actual), "load saved record");
            require(actual.bezel == bezel && actual.show_fps == visible, "every setting combination round trips");
            require(access((path + ".tmp").c_str(), F_OK) != 0, "successful commit consumes temporary file");
        }
    }
    require(read_bytes(path) == "J2ME_MENU 2\nbezel=6\nshow_fps=1\n", "canonical versioned serialization");

    // Every retired v1 identifier migrates to none without changing the FPS
    // preference or writing during a read. A later explicit save upgrades v2.
    for (int bezel = 0; bezel <= 6; ++bezel) for (bool visible : {false, true}) {
        const std::string legacy = "J2ME_MENU 1\nbezel=" + std::to_string(bezel) +
            "\nshow_fps=" + (visible ? "1\n" : "0\n");
        write_bytes(path, legacy);
        host_menu::Settings migrated;
        migrated.bezel = 3;
        require(host_menu::load_settings(path, migrated), "strict v1 record accepted");
        require(migrated.bezel == 0 && migrated.show_fps == visible, "v1 retires skins and preserves FPS");
        require(read_bytes(path) == legacy, "v1 migration read is side effect free");
        require(host_menu::save_settings(path, migrated), "explicit save upgrades legacy preferences");
        require(read_bytes(path) == "J2ME_MENU 2\nbezel=0\nshow_fps=" + std::string(visible ? "1\n" : "0\n"),
                "migrated serialization uses v2");
    }
    for (const std::string& invalid : {std::string("J2ME_MENU 1\nbezel=7\nshow_fps=0\n"),
            std::string("J2ME_MENU 1\nbezel=-1\nshow_fps=1\n"),
            std::string("J2ME_MENU 1\nbezel=6\nshow_fps=1\ntrailing"),
            std::string("J2ME_MENU 1\nbezel=99999999999999\nshow_fps=1\n")})
        reject_record(path, invalid);

    const std::string valid = "J2ME_MENU 2\nbezel=2\nshow_fps=0\n";
    for (const std::string& invalid : {
            std::string(), std::string("J2ME_MENU 2\n"),
            std::string("J2ME_MENU 2\nbezel=2\nshow_fps="),
            std::string("J2ME_MENU 2\nbezel=2\nshow_fps=0"),
            std::string("J2ME_MENU 3\nbezel=2\nshow_fps=0\n"),
            std::string("J2ME_MENU 2\nbezel=-1\nshow_fps=0\n"),
            std::string("J2ME_MENU 2\nbezel=7\nshow_fps=0\n"),
            std::string("J2ME_MENU 2\nbezel=999999999999999999999\nshow_fps=0\n"),
            std::string("J2ME_MENU 2\nbezel=2x\nshow_fps=0\n"),
            std::string("J2ME_MENU 2\nbezel=2\nshow_fps=2\n"),
            std::string("J2ME_MENU 2\nbezel=2\nshow_fps=true\n"),
            valid + "bezel=3\n", valid + std::string(1025, 'x')}) {
        reject_record(path, invalid);
    }
    reject_record(path, valid + std::string("\0", 1));

    write_bytes(path, valid);
    for (int bezel : {-1, host_menu::bezel_count}) {
        host_menu::Settings invalid;
        invalid.bezel = bezel;
        require(!host_menu::save_settings(path, invalid), "invalid settings cannot be saved");
        require(read_bytes(path) == valid, "invalid save preserves existing file");
        require(access((path + ".tmp").c_str(), F_OK) != 0, "invalid save creates no temporary file");
    }
    require(mkdir((path + ".tmp").c_str(), 0700) == 0, "directory prevents temporary write");
    require(!host_menu::save_settings(path, defaults), "failed temporary write reported");
    require(read_bytes(path) == valid, "failed write retains existing file");
    require(rmdir((path + ".tmp").c_str()) == 0, "failed open leaves temporary directory intact");

    const std::string target_directory = std::string(directory) + "/target-directory";
    require(mkdir(target_directory.c_str(), 0700) == 0, "directory prevents rename commit");
    require(!host_menu::save_settings(target_directory, defaults), "failed rename reported");
    struct stat target_stat;
    require(stat(target_directory.c_str(), &target_stat) == 0 && S_ISDIR(target_stat.st_mode),
            "failed commit never deletes its destination");
    require(access((target_directory + ".tmp").c_str(), F_OK) != 0, "failed commit removes temporary file");
    require(read_bytes(path) == valid, "failed commit leaves existing preference file untouched");

    host_menu::Settings unchanged = defaults;
    const std::string nul_path = path + std::string("\0ignored", 8);
    require(!host_menu::load_settings(nul_path, unchanged), "embedded NUL read path rejected");
    require(!host_menu::save_settings(nul_path, defaults), "embedded NUL write path rejected");
    require(read_bytes(path) == valid, "invalid path cannot overwrite prefix file");
    require(!host_menu::save_settings("", defaults), "empty write path rejected");
    require(!host_menu::load_settings("", unchanged), "empty read path rejected");

    const std::string previous = path + ".previous";
    nonreplacing_rename = true;
    host_menu::Settings changed;
    changed.bezel = 3;
    changed.show_fps = false;
    for (int i = 0; i < 4; ++i) {
        changed.show_fps = !changed.show_fps;
        require(host_menu::save_settings(path, changed), "EEXIST filesystem permits repeated setting saves");
        host_menu::Settings reloaded;
        require(host_menu::load_settings(path, reloaded), "reload EEXIST replacement");
        require(reloaded.bezel == changed.bezel && reloaded.show_fps == changed.show_fps,
                "EEXIST replacement persists each toggle");
        require(access(previous.c_str(), F_OK) != 0, "successful replacement removes journal");
    }

    write_bytes(path, valid);
    write_bytes(previous, "obsolete previous snapshot");
    require(host_menu::save_settings(path, changed), "EEXIST replacement supersedes stale journal while main remains safe");
    require(access(previous.c_str(), F_OK) != 0, "stale journal removed after commit");

    write_bytes(path, valid);
    deny_backup_move = true;
    require(!host_menu::save_settings(path, changed), "failed backup move reported");
    require(read_bytes(path) == valid, "failed backup move preserves original main file");
    require(access((path + ".tmp").c_str(), F_OK) != 0, "failed backup move removes temporary file");
    deny_backup_move = false;

    deny_new_commit = true;
    require(!host_menu::save_settings(path, changed), "failed second rename reported");
    require(read_bytes(path) == valid, "failed commit rolls back exact original bytes");
    require(access(previous.c_str(), F_OK) != 0, "successful rollback consumes journal");
    require(access((path + ".tmp").c_str(), F_OK) != 0, "failed commit removes uncommitted settings");

    deny_rollback = true;
    require(!host_menu::save_settings(path, changed), "failed commit and rollback reported");
    require(access(path.c_str(), F_OK) != 0, "failed rollback leaves main missing");
    require(read_bytes(previous) == valid, "failed rollback retains exact original in journal");
    host_menu::Settings recovered;
    require(host_menu::load_settings(path, recovered), "missing main recovers journal after failed rollback");
    require(recovered.bezel == 2 && !recovered.show_fps, "recovery returns old preferences");
    require(access(path.c_str(), F_OK) != 0, "loader recovery is read-only");
    deny_rollback = false;
    deny_new_commit = false;
    require(host_menu::save_settings(path, changed), "next explicit save restores journal then commits new preferences");
    require(access(previous.c_str(), F_OK) != 0, "recovered save consumes journal");

    write_bytes(path, valid);
    require(std::rename(path.c_str(), previous.c_str()) == 0, "simulate interruption after moving old main");
    recovered = defaults;
    require(host_menu::load_settings(path, recovered), "missing main loads interrupted snapshot");
    require(recovered.bezel == 2 && !recovered.show_fps, "interrupted snapshot remains exact");
    require(mkdir((path + ".tmp").c_str(), 0700) == 0, "prevent recovered save temporary write");
    require(!host_menu::save_settings(path, changed), "failed write after recovery reported");
    require(read_bytes(path) == valid, "restore previous before failed write preserves old main");
    require(rmdir((path + ".tmp").c_str()) == 0, "remove recovery failure fixture");

    write_bytes(previous, valid);
    for (const std::string& bad_main : {std::string("truncated"),
            std::string("J2ME_MENU 2\nbezel=7\nshow_fps=1\n")}) {
        write_bytes(path, bad_main);
        recovered = defaults;
        require(!host_menu::load_settings(path, recovered), "present malformed or future-version main never falls back");
        require(recovered.bezel == defaults.bezel && recovered.show_fps == defaults.show_fps,
                "present invalid main preserves source preferences");
        require(read_bytes(path) == bad_main, "load never rewrites malformed or future main");
        require(read_bytes(previous) == valid, "load leaves previous snapshot intact");
    }
    require(std::remove(path.c_str()) == 0, "prepare missing main for legacy backup");
    const std::string legacy_backup = "J2ME_MENU 1\nbezel=6\nshow_fps=0\n";
    write_bytes(previous, legacy_backup);
    recovered.bezel = 3;
    require(host_menu::load_settings(path, recovered), "legacy journal recovery accepted");
    require(recovered.bezel == 0 && !recovered.show_fps, "legacy backup migrates safely and preserves FPS");
    require(read_bytes(previous) == legacy_backup && access(path.c_str(), F_OK) != 0,
            "legacy backup recovery remains read only");
    require(std::remove(previous.c_str()) == 0, "remove journal fixture");
    write_bytes(path, valid);
    require(mkdir(previous.c_str(), 0700) == 0, "invalid journal directory fixture");
    require(!host_menu::save_settings(path, changed), "EEXIST replacement refuses unreadable journal path");
    require(read_bytes(path) == valid, "invalid journal path cannot lose original main");
    require(rmdir(previous.c_str()) == 0, "journal directory remains untouched");
    nonreplacing_rename = false;

    require(std::remove(path.c_str()) == 0, "remove fixture");
    require(rmdir(target_directory.c_str()) == 0, "remove directory fixture");
    require(rmdir(directory) == 0, "remove isolated directory");
    std::cout << "ALL HOST MENU SETTINGS CHECKS COMPLETE " << checks << '\n';
}
