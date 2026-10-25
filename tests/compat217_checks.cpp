// Focused API checks run on the emulator, including actual GC and filesystem I/O.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <unistd.h>

static int space_mode = 0;
extern "C" int __real_statvfs(const char*, struct statvfs*);
extern "C" int __wrap_statvfs(const char* path, struct statvfs* out) {
    if (space_mode == 0) return __real_statvfs(path, out);
    if (space_mode == 3) { errno = EIO; return -1; }
    *out = {};
    out->f_bsize = out->f_frsize = 4096;
    out->f_bavail = space_mode == 2 ? 0 : 8;
    return 0;
}
static unsigned checks = 0;
static void require(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL " << message << " assertion=" << checks << "\n"; std::exit(1); }
}
static void write_bytes(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary); out.write(bytes.data(), bytes.size()); out.close();
    require(!out.fail(), "fixture written");
}
static bool exists(const std::string& path) { struct stat st; return stat(path.c_str(), &st) == 0; }
static std::string encoded(const std::string& name) {
    const char* hex = "0123456789abcdef";
    std::string result = "r";
    for (unsigned char c : name) { result += hex[c >> 4]; result += hex[c & 15]; }
    return result + ".bin";
}
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    VMS_CL(g_classpath) = {argv[1], argv[2]};
    VMS_DIAG(g_log_draw) = false;
    require(setenv("J2ME_RMS_DIR", argv[3], 1) == 0, "RMS directory isolated");
    VMS_PLAT(g_rms_suite) = rms_jar_identity(reinterpret_cast<const uint8_t*>("suite A"), 7);
    const std::string suite_a = VMS_PLAT(g_rms_suite);
    const std::string directory = std::string(argv[3]) + "/suites/" + suite_a;
    launch_class("Compat217Checks", suite_a);
    int cls = ensure_class("Compat217Checks");
    auto invoke = [&](const char* method, const std::string& desc = "()I", const std::vector<Value>& args = std::vector<Value>{}) {
        Method* m = find_method(cls, method, desc, false);
        require(m != nullptr, "test method resolved");
        int result = run(m, args).i;
        require(!VMS_EXEC(g_thrown), method);
        return result;
    };
    auto string_call = [&](const char* method, const std::string& name) {
        return invoke(method, "(Ljava/lang/String;)I", {Value::of_ref(make_string(name))});
    };
    int java_collections = invoke("collections");
    require(java_collections == 67, "collection semantics checks completed");
    std::cout << "PASS Compat217 Java collections " << java_collections << "\n";
    int java_rms = invoke("rms");
    require(java_rms == 11, "RMS semantics checks completed");
    std::cout << "PASS Compat217 Java RMS " << java_rms << "\n";

    invoke("retainArray");
    Field* retained = find_field(cls, "retained", "Ljava/util/Vector;");
    int vector = VMS_CL(g_classes)[cls].statics[retained->slot].ref;
    Field* data = find_field(VMS_HEAP(g_heap)[vector].cls, "elementData", "[Ljava/lang/Object;");
    int array = VMS_HEAP(g_heap)[VMS_HEAP(g_heap)[vector].fields[data->slot].ref].elems.get(0).ref;
    gc_collect("Compat217 before Vector clear");
    require(VMS_HEAP(g_heap)[array].is_array && VMS_HEAP(g_heap)[array].elems.size() == 262144, "reachable Vector keeps array alive");
    require(invoke("clearArray") == 0, "Vector cleared");
    size_t before = VMS_HEAP(g_array_payload_live);
    gc_collect("Compat217 after Vector clear");
    require(!VMS_HEAP(g_heap)[array].is_array && before - VMS_HEAP(g_array_payload_live) >= 262144, "cleared Vector releases 256KiB array to GC");

    require(string_call("create", "Owned") == 1, "current-suite store created");
    const std::string bytes = std::string("\0\0\0\1\x2a", 5);
    // Ignored debris, invalid snapshots, invalid names, and directories.
    write_bytes(directory + "/" + encoded("Temp") + ".tmp", bytes);
    write_bytes(directory + "/" + encoded("Backup") + ".v72.bak", bytes);
    write_bytes(directory + "/" + encoded("Deleted") + ".deleted", bytes);
    write_bytes(directory + "/" + encoded("Broken"), std::string("\0\0\0\x0a\x2a", 5));
    write_bytes(directory + "/rzz.bin", bytes);
    write_bytes(directory + "/rc080ff.bin", bytes);
    require(mkdir((directory + "/" + encoded("Directory")).c_str(), 0777) == 0, "non-file fixture created");
    require(string_call("create", "Recovered") == 1, "recovery fixture created");
    require(std::rename((directory + "/" + encoded("Recovered")).c_str(), (directory + "/" + encoded("Recovered") + ".previous").c_str()) == 0, "simulate interrupted replacement");
    write_bytes(directory + "/" + encoded("Owned") + ".previous", bytes);
    write_bytes(directory + "/" + encoded("Legacy"), bytes);
    write_bytes(directory + "/" + encoded("ZeroLegacy"), std::string("\0\0\0\0\0\0\0\0", 8));
    require(invoke("countStores") == 4, "catalog contains valid stores once, including recovery and legacy");
    require(string_call("hasStore", "Recovered") == 1, "recoverable journal listed");
    require(exists(directory + "/" + encoded("Recovered") + ".previous"), "listing leaves recovery journal intact");

    space_mode = 1;
    require(string_call("available", "Owned") == 28672, "actual free blocks minus atomic snapshot reserve");
    require(string_call("available", "Legacy") == 24576, "first legacy write reserves rollback backup");
    space_mode = 2;
    require(string_call("available", "Owned") == 0, "full filesystem reports zero growth");
    space_mode = 3;
    require(string_call("available", "Owned") == 0, "failed filesystem query reports zero growth");
    space_mode = 0;
    uint64_t physical, block;
    require(j2me_rms::free_bytes(directory, physical, block), "real desktop free-space query succeeds");
    require(uint64_t(string_call("available", "Owned")) <= physical, "reported growth bounded by real free bytes");
    require(j2me_rms::growth_available(UINT64_MAX, 4096, 100) == INT32_MAX - 100, "snapshot format limit clamps capacity");
    require(j2me_rms::growth_available(4096, 4096, 100) == 0, "existing snapshot reserves full allocation block");

    // Exercise the same fresh Java state and suite selection as a real launch.
    launch_class("Compat217Checks", rms_jar_identity(reinterpret_cast<const uint8_t*>("suite B"), 7));
    cls = ensure_class("Compat217Checks");
    require(invoke("countStores") == 0, "second suite cannot enumerate first suite stores");
    require(string_call("create", "Other") == 1 && invoke("countStores") == 1, "second suite lists only its own store");
    launch_class("Compat217Checks", suite_a); cls = ensure_class("Compat217Checks");
    require(invoke("countStores") == 4 && string_call("hasStore", "Other") == 0, "first suite remains isolated on reload");
    require(string_call("erase", "Recovered") == 1 && string_call("hasStore", "Recovered") == 0, "delete recovered store removes catalog entry");
    std::cout << "PASS Compat217 native assertions " << checks << "\n";
}
