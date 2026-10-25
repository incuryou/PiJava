// Run the real interpreter against Java RMS checks in an isolated directory.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <unistd.h>

static bool force_nonreplace = false;
static bool deny_commit = false;
extern "C" int __real_rename(const char*, const char*);
extern "C" int __wrap_rename(const char* source, const char* target) {
    struct stat st;
    if (force_nonreplace && stat(target, &st) == 0) { errno = EEXIST; return -1; }
    if (deny_commit && std::string(source).find("r526d733733496f46697874757265.bin.tmp") != std::string::npos) { errno = EIO; return -1; }
    return __real_rename(source, target);
}
static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL " << message << "\n"; std::exit(1); }
}
static void write_bytes(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), bytes.size()); out.close();
    require(!out.fail(), "write fixture");
}
static std::string read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    require((bool)in, "read fixture");
    return std::string((std::istreambuf_iterator<char>(in)), {});
}
int main(int argc, char** argv) {
    if (argc != 5) return 2;
    VMS_CL(g_classpath) = {argv[1], argv[2]};
    std::string directory = argv[3];
    const char* configured = std::getenv("J2ME_RMS_DIR");
    require(configured && directory == configured, "isolated RMS directory configured");
    force_nonreplace = std::string(argv[4]) == "nonreplace";
    VMS_PLAT(g_rms_suite) = rms_class_identity("RmsStorageChecks", {});
    require(mkdir((directory + "/suites").c_str(), 0777) == 0, "suite parent");
    const std::string suite_dir = directory + "/suites/" + VMS_PLAT(g_rms_suite);
    require(mkdir(suite_dir.c_str(), 0777) == 0, "suite directory");
    auto store_file = [&](const std::string& name) {
        static const char* hex = "0123456789abcdef";
        std::string out = suite_dir + "/r";
        for (unsigned char c : name) { out += hex[c >> 4]; out += hex[c & 15]; }
        return out + ".bin";
    };
    VMS_DIAG(g_log_draw) = false;
    int cls = ensure_class("RmsLifecycleChecks");
    auto invoke = [&](const char* method, int expected) {
        Method* entry = find_method(cls, method, "()I", false);
        require(entry != nullptr, "Java RMS entry");
        int checks = run(entry, {}).i;
        std::cout << "INFO RMS stage=" << method << " expected=" << expected << " actual=" << checks
                  << " thrown=" << VMS_EXEC(g_thrown) << "\n";
        require(!VMS_EXEC(g_thrown) && checks == expected, method);
        std::cout << "PASS " << method << " " << checks << "\n";
    };
    const char legacy_raw[] = {0,0,0,1,7,0,0,0,0,0,0,0,1,9};
    const std::string legacy(legacy_raw, sizeof(legacy_raw));
    const char single_raw[] = {0,0,0,1,6};
    const std::string single(single_raw, sizeof(single_raw));

    invoke("run", 50);
    write_bytes(store_file("Rms73Legacy_Fixture"), legacy); // legacy byte format in the isolated namespace
    invoke("legacyFixture", 3);
    require(read_bytes(store_file("Rms73Legacy_Fixture") + ".v72.bak") == legacy, "exact legacy rollback backup survives later writes");
    std::cout << "PASS exact legacy backup in isolated namespace 1\n";

    write_bytes(store_file("Rms73IoFixture"), single);
    require(mkdir((store_file("Rms73IoFixture") + ".v72.bak").c_str(), 0777) == 0, "create invalid backup fixture");
    invoke("prepareLegacyFailure", 1);
    invoke("ioFailure", 3);
    require(read_bytes(store_file("Rms73IoFixture")) == single, "failed backup preserves original legacy bytes");
    require(rmdir((store_file("Rms73IoFixture") + ".v72.bak").c_str()) == 0, "remove invalid backup fixture");
    invoke("verifyIoRecovery", 1);
    std::cout << "PASS backup failure refuses migration 1\n";

    invoke("prepareIoFailure", 1);
    require(mkdir((store_file("Rms73IoFixture") + ".tmp").c_str(), 0777) == 0, "create write failure fixture");
    invoke("ioFailure", 3);
    require(rmdir((store_file("Rms73IoFixture") + ".tmp").c_str()) == 0, "remove write failure fixture");
    invoke("verifyIoRecovery", 1);

    if (force_nonreplace) {
        invoke("prepareIoFailure", 1);
        deny_commit = true;
        invoke("ioFailure", 3);
        deny_commit = false;
        std::cout << "PASS non-replacing rename rollback 1\n";
        require(std::rename((store_file("Rms73IoFixture")).c_str(), (store_file("Rms73IoFixture") + ".previous").c_str()) == 0, "simulate interrupted replacement");
        invoke("verifyIoRecovery", 1);
        std::cout << "PASS interrupted journal recovery 1\n";
    }

    // Deleting a current file with an obsolete journal must not resurrect it.
    write_bytes(store_file("Rms73IoFixture"), single);
    write_bytes(store_file("Rms73IoFixture") + ".previous", single);
    invoke("deleteFixture", 1);
    struct stat deleted;
    require(stat((store_file("Rms73IoFixture") + ".previous").c_str(), &deleted) != 0 && errno == ENOENT, "deletion removes obsolete journal");
    std::cout << "PASS journal cannot resurrect deleted store 1\n";

    write_bytes(store_file("Rms73IoFixture"), single);
    require(mkdir((store_file("Rms73IoFixture") + ".previous").c_str(), 0777) == 0, "create invalid journal fixture");
    invoke("deleteJournalFailure", 2);
    require(read_bytes(store_file("Rms73IoFixture")) == single, "invalid journal leaves primary bytes unchanged");
    require(rmdir((store_file("Rms73IoFixture") + ".previous").c_str()) == 0, "remove invalid journal fixture");
    invoke("deleteFixture", 1);

    const char corrupt[] = {0,0,0,10,7};
    write_bytes(store_file("Rms73InvalidFixture"), std::string(corrupt, sizeof(corrupt)));
    invoke("loadFailure", 1);
    require(std::remove((store_file("Rms73InvalidFixture")).c_str()) == 0, "remove corrupt fixture");
    require(mkdir((store_file("Rms73InvalidFixture")).c_str(), 0777) == 0, "create non-file fixture");
    invoke("loadFailure", 1);
    require(rmdir((store_file("Rms73InvalidFixture")).c_str()) == 0, "remove non-file fixture");
    std::cout << "ALL RMS CHECKS COMPLETE " << (force_nonreplace ? "nonreplace" : "posix") << "\n";
}
