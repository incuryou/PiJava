// Exercise the real VM, including fresh Java state for each temporary JAR launch.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <unistd.h>

static bool force_nonreplace = false;
static std::string deny_rename_source;
extern "C" int __real_rename(const char*, const char*);
extern "C" int __wrap_rename(const char* source, const char* target) {
    struct stat st;
    if (force_nonreplace && stat(target, &st) == 0) { errno = EEXIST; return -1; }
    if (!deny_rename_source.empty() && source == deny_rename_source) { errno = EIO; return -1; }
    return __real_rename(source, target);
}
static unsigned checks = 0;
static void require(bool ok, const std::string& message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL " << message << " (assertion " << checks << ")\n"; std::exit(1); }
}
static void pass(const char* stage) { std::cout << "PASS " << stage << " assertions=" << checks << "\n"; }
static void write_bytes(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary); out.write(bytes.data(), bytes.size()); out.close();
    require(!out.fail(), "write temporary fixture " + path);
}
static std::string read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary); require((bool)in, "read fixture " + path);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}
static bool exists(const std::string& path) { struct stat st; return stat(path.c_str(), &st) == 0; }
static std::string hex_name(const std::string& name) {
    const char* hex = "0123456789abcdef"; std::string result;
    for (unsigned char c : name) { result += hex[c >> 4]; result += hex[c & 15]; }
    return result;
}
static std::string legacy(unsigned char value) {
    const char header[] = {0,0,0,1}; return std::string(header, 4) + (char)value;
}
int main(int argc, char** argv) {
    if (argc != 8) return 2;
    VMS_CL(g_classpath) = {argv[1], argv[2]};
    const std::string directory = argv[3], jar_a = argv[5], jar_b = argv[6], jar_renamed = argv[7];
    const char* configured = std::getenv("J2ME_RMS_DIR");
    require(configured && directory == configured, "isolated RMS directory configured");
    force_nonreplace = std::string(argv[4]) == "nonreplace"; VMS_DIAG(g_log_draw) = false;
    auto identity = [&](const std::string& jar) {
        std::vector<uint8_t> bytes; require(read_whole(jar, bytes), "read temporary JAR");
        return rms_jar_identity(bytes.data(), bytes.size());
    };
    const std::string suite_a = identity(jar_a), suite_b = identity(jar_b);
    require(suite_a != suite_b, "distinct raw JAR bytes have distinct identities");
    require(suite_a == identity(jar_renamed), "renaming identical raw bytes keeps identity");
    require(rms_jar_identity(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA256 empty known vector");
    const uint8_t abc[] = {'a','b','c'};
    require(rms_jar_identity(abc, sizeof(abc)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 abc known vector");
    auto launch = [&](const std::string& jar, bool import = false) {
        std::vector<uint8_t> bytes; require(read_whole(jar, bytes), "read launch JAR");
        VMS_CL(g_jar_files).clear(); VMS_CL(g_props).clear(); std::string error;
        require(unzip(bytes, VMS_CL(g_jar_files), error), "unzip launch JAR: " + error); load_props();
        require(VMS_CL(g_props)["MIDlet-Name"] == "Same metadata" && VMS_CL(g_props)["MIDlet-Vendor"] == "Same vendor", "identical manifest name and vendor");
        require(manifest_midlet() == "RmsIsolationChecks", "identical main class");
        launch_class(manifest_midlet(), rms_jar_identity(bytes.data(), bytes.size()), import);
        require(!VMS_EXEC(g_thrown), "MIDlet launches without Java exception");
    };
    auto invoke = [&](const char* method, const std::string& name, int value = 0) {
        int cls = ensure_class("RmsIsolationChecks");
        bool takes_value = std::string(method) == "put" || std::string(method) == "hold";
        Method* entry = find_method(cls, method, takes_value ? "(Ljava/lang/String;I)I" : "(Ljava/lang/String;)I", false);
        require(entry != nullptr, "Java RMS method exists");
        std::vector<Value> args = {Value::of_ref(make_string(name))};
        if (takes_value) args.push_back(Value::of_i(value));
        int result = run(entry, args).i; require(!VMS_EXEC(g_thrown), std::string("uncaught Java exception in ") + method); return result;
    };
    auto expect = [&](const char* method, const std::string& name, int result, int value = 0) {
        int actual = invoke(method, name, value);
        require(actual == result, std::string(method) + " namehex=" + hex_name(name) + " expected=" + std::to_string(result) + " actual=" + std::to_string(actual));
    };
    auto path = [&](const std::string& suite, const std::string& name) { return directory + "/suites/" + suite + "/r" + hex_name(name) + ".bin"; };
    auto old = [&](const std::string& name) { return directory + "/" + name + ".bin"; };
    auto restart = [&](bool import = true) { launch(jar_a, import); };
    launch(jar_a); expect("read", "Save", -1); expect("put", "Save", 11, 11);
    launch(jar_b); expect("read", "Save", -1); expect("put", "Save", 22, 22);
    launch(jar_renamed); expect("read", "Save", 11); expect("put", "Save", 13, 13);
    launch(jar_b); expect("read", "Save", 22); expect("erase", "Save", 1);
    restart(false); expect("read", "Save", 13);
    launch(jar_b); expect("read", "Save", -1);
    pass("distinct JARs with identical metadata isolate read write delete; renamed JAR preserves save");

    restart(false); expect("hold", "Held", 31, 31);
    launch(jar_b); expect("read", "Held", -1); expect("put", "Held", 32, 32); expect("erase", "Held", 1);
    restart(false); expect("read", "Held", 31); expect("erase", "Held", 1);
    pass("game switch discards unclosed handles and Java static open-store registry");

    restart(false);
    const std::vector<std::string> names = {"Save", "save", "a/b", "a_b", "../escape", "..\\escape", "%612f62", std::string("a\xc0\x80" "b", 4), "a", std::string("a\0b", 3), std::string("\xed\xa0\x80", 3), std::string("\xed\xb0\x80", 3)};
    for (size_t i = 0; i < names.size(); ++i) expect("put", names[i], 40 + i, 40 + i);
    restart(false);
    for (size_t i = 0; i < names.size(); ++i) { expect("read", names[i], 40 + i); require(exists(path(suite_a, names[i])), "full-byte lowercase-hex filename exists"); }
    require(path(suite_a, "Save") != path(suite_a, "save"), "case names are hex-distinct on FAT");
    require(!exists(directory + "/escape.bin"), "path-style name stays in namespace");
    pass("case separator traversal NUL CESU8 surrogate and percent names do not collide");

    write_bytes(old("LegacyPlain"), legacy(71)); restart(false); expect("read", "LegacyPlain", -1);
    require(!exists(path(suite_a, "LegacyPlain")), "default launch never copies global legacy");
    restart(); expect("read", "LegacyPlain", 71); require(read_bytes(path(suite_a, "LegacyPlain")) == legacy(71), "explicit import copies exact legacy bytes");
    expect("put", "LegacyPlain", 72, 72); require(read_bytes(old("LegacyPlain")) == legacy(71), "new writes leave global source unchanged");
    launch(jar_b); expect("read", "LegacyPlain", -1);
    restart(); expect("read", "LegacyPlain", 72);
    require(read_bytes(old("LegacyPlain")) == legacy(71), "existing new progress never overwritten by legacy");
    pass("legacy import is explicit copy-once per suite and preserves new progress and old source");

    expect("erase", "LegacyPlain", 1); require(exists(path(suite_a, "LegacyPlain") + ".deleted"), "delete persists import barrier");
    restart(); expect("read", "LegacyPlain", -1); expect("put", "LegacyPlain", 73, 73);
    require(exists(path(suite_a, "LegacyPlain") + ".deleted"), "recreate retains import barrier");
    expect("erase", "LegacyPlain", 1); restart(); expect("read", "LegacyPlain", -1);
    write_bytes(old("NeverOpened"), legacy(74)); restart(); expect("erase", "NeverOpened", -1);
    require(exists(path(suite_a, "NeverOpened") + ".deleted"), "missing delete also persists barrier");
    restart(); expect("read", "NeverOpened", -1); require(read_bytes(old("NeverOpened")) == legacy(74), "missing deletion does not alter legacy source");
    pass("delete restart repeated import and reset-before-first-open never resurrect legacy progress");

    write_bytes(old("v72_name"), legacy(81)); restart(); expect("read", "v72/name", 81);
    write_bytes(old("%" + hex_name("v73/name")), legacy(82)); restart(); expect("read", "v73/name", 82);
    write_bytes(old("%" + hex_name(std::string("nul\xc0\x80" "name", 9))), legacy(83)); restart(); expect("read", std::string("nul\xc0\x80" "name", 9), 83);
    write_bytes(old("%" + hex_name("amb/name")), legacy(84)); write_bytes(old("amb_name"), legacy(85));
    restart(); expect("read", "amb/name", -2); require(!exists(path(suite_a, "amb/name")), "ambiguous import creates no target");
    require(read_bytes(old("amb_name")) == legacy(85) && read_bytes(old("%" + hex_name("amb/name"))) == legacy(84), "ambiguous sources retained");
    pass("v72 sanitized v73 plain fullhex CESU8 import and ambiguity rejection");

    write_bytes(old("OldPrevious") + ".previous", legacy(91)); restart(); expect("read", "OldPrevious", 91);
    require(!exists(old("OldPrevious")) && read_bytes(old("OldPrevious") + ".previous") == legacy(91), "global previous read-only source remains untouched");
    write_bytes(old("NewPrevious"), legacy(92)); write_bytes(path(suite_a, "NewPrevious") + ".previous", legacy(93));
    restart(); expect("read", "NewPrevious", 93); require(!exists(path(suite_a, "NewPrevious") + ".previous"), "new namespace journal recovered first");
    require(read_bytes(old("NewPrevious")) == legacy(92), "preferred namespace recovery never changes global");
    write_bytes(old("OldMainFirst"), legacy(94)); write_bytes(old("OldMainFirst") + ".previous", legacy(95));
    restart(); expect("read", "OldMainFirst", 94);
    require(read_bytes(old("OldMainFirst") + ".previous") == legacy(95), "old main preferred without modifying old previous");
    pass("read-only global journal source and preferred new namespace recovery");

    write_bytes(old("ImportTmpDir"), legacy(101)); require(mkdir((path(suite_a, "ImportTmpDir") + ".tmp").c_str(), 0777) == 0, "import temp directory fixture");
    restart(); expect("read", "ImportTmpDir", -2);
    require(read_bytes(old("ImportTmpDir")) == legacy(101) && !exists(path(suite_a, "ImportTmpDir")), "temp directory import failure preserves source and no target");
    require(rmdir((path(suite_a, "ImportTmpDir") + ".tmp").c_str()) == 0, "remove import temp fixture");
    restart(); expect("read", "ImportTmpDir", 101);
    write_bytes(old("ImportEio"), legacy(102)); deny_rename_source = path(suite_a, "ImportEio") + ".tmp";
    restart(); expect("read", "ImportEio", -2); deny_rename_source.clear();
    require(read_bytes(old("ImportEio")) == legacy(102) && !exists(path(suite_a, "ImportEio")) && !exists(path(suite_a, "ImportEio") + ".tmp"), "EIO import cleans temp and preserves source");
    restart(); expect("read", "ImportEio", 102);
    pass("import temporary-directory and rename EIO failures preserve source then recover");

    restart(false); expect("put", "MarkerDir", 111, 111); std::string snapshot = read_bytes(path(suite_a, "MarkerDir"));
    require(mkdir((path(suite_a, "MarkerDir") + ".deleted.tmp").c_str(), 0777) == 0, "marker temp directory fixture");
    expect("erase", "MarkerDir", -2); require(read_bytes(path(suite_a, "MarkerDir")) == snapshot, "marker write failure cannot delete primary");
    require(rmdir((path(suite_a, "MarkerDir") + ".deleted.tmp").c_str()) == 0, "remove marker failure fixture");
    expect("read", "MarkerDir", 111); expect("erase", "MarkerDir", 1);
    expect("put", "MarkerEio", 112, 112); snapshot = read_bytes(path(suite_a, "MarkerEio"));
    deny_rename_source = path(suite_a, "MarkerEio") + ".deleted.tmp"; expect("erase", "MarkerEio", -2); deny_rename_source.clear();
    require(read_bytes(path(suite_a, "MarkerEio")) == snapshot, "marker commit EIO cannot delete primary"); expect("erase", "MarkerEio", 1);
    expect("put", "DeleteJournal", 113, 113); write_bytes(path(suite_a, "DeleteJournal") + ".previous", legacy(114));
    expect("erase", "DeleteJournal", 1); require(!exists(path(suite_a, "DeleteJournal") + ".previous"), "delete removes obsolete namespace journal");
    restart(); expect("read", "DeleteJournal", -1);
    pass("deletion requires durable marker before primary removal and clears stale journal");

    const std::string corrupt("\0\0\0\x0a\x07", 5); write_bytes(old("BadSource"), corrupt);
    restart(); expect("read", "BadSource", -2); require(read_bytes(old("BadSource")) == corrupt, "malformed source bytes retained");
    require(mkdir(old("SourceDirectory").c_str(), 0777) == 0, "legacy source directory fixture");
    restart(); expect("read", "SourceDirectory", -2); require(!exists(path(suite_a, "SourceDirectory")), "directory source creates no target");
    require(mkdir(path(suite_a, "TargetDirectory").c_str(), 0777) == 0, "namespace target directory fixture");
    write_bytes(old("TargetDirectory"), legacy(121)); restart(); expect("read", "TargetDirectory", -2);
    require(read_bytes(old("TargetDirectory")) == legacy(121), "target directory error preserves source");
    require(mkdir((path(suite_a, "PreviousDirectory") + ".previous").c_str(), 0777) == 0, "namespace previous directory fixture");
    write_bytes(old("PreviousDirectory"), legacy(122)); restart(); expect("read", "PreviousDirectory", -2);
    require(!exists(path(suite_a, "PreviousDirectory")), "invalid namespace journal never authorizes global import");
    require(mkdir((path(suite_a, "MarkerDirectory") + ".deleted").c_str(), 0777) == 0, "marker directory fixture");
    write_bytes(old("MarkerDirectory"), legacy(123)); restart(); expect("read", "MarkerDirectory", -2);
    require(!exists(path(suite_a, "MarkerDirectory")), "invalid marker never authorizes global import");
    pass("malformed source and non-file source target journal marker errors preserve legacy");

    const std::string invalid_root = directory + "/NotDirectory"; write_bytes(invalid_root, "root fixture");
    require(setenv("J2ME_RMS_DIR", invalid_root.c_str(), 1) == 0, "set invalid root"); restart(); expect("read", "LegacyPlain", -2); expect("put", "CannotWrite", -2, 1);
    require(read_bytes(invalid_root) == "root fixture", "invalid configured root is not replaced");
    require(setenv("J2ME_RMS_DIR", directory.c_str(), 1) == 0, "restore isolated root");
    const std::vector<std::string> invalid_suites = {"", "abc", std::string(64, 'G'), std::string(63, 'a') + "/", std::string(64, 'A')};
    for (const auto& invalid : invalid_suites) {
        restart(); VMS_PLAT(g_rms_suite) = invalid; expect("read", "LegacyPlain", -2); expect("put", "InvalidScope", -2, 1); expect("erase", "LegacyPlain", -2);
        require(read_bytes(old("LegacyPlain")) == legacy(71), "invalid suite never falls back to global source");
    }
    stop_thread(); require(VMS_PLAT(g_rms_suite).empty() && !VMS_PLAT(g_rms_import_legacy) && VMS_EXEC(g_threads).empty(), "stop clears scope import and thread state");
    expect("read", "LegacyPlain", -2);
    pass("bad root empty invalid stopped suite fails closed without global fallback");
    std::cout << "ALL RMS ISOLATION CHECKS COMPLETE " << argv[4] << " assertions=" << checks << "\n";
}
