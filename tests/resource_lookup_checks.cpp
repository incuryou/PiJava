// Build: gcc -O1 -c src/puff.c -o build/resource-puff.o
// g++ -O1 -std=c++17 -fno-exceptions tests/resource_lookup_checks.cpp build/resource-puff.o -pthread -ldl -o build/resource_lookup_checks
// Run: build/resource_lookup_checks <fixture-classes-dir> <api-classes-dir> [JavaClass]
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: resource_lookup_checks <fixture-classes-dir> <api-classes-dir> [JavaClass]\n";
        return 2;
    }
    std::cout << std::unitbuf;
    VMS_CL(g_classpath) = {argv[1], argv[2]};
    VMS_DIAG(g_log_draw) = false;
    VMS_PLAT(g_boot_ms) = now_ms();
    configure_runtime_diagnostics();
    std::cout << "RESOURCE_CONFIG jit_enabled=" << VMS_NATIVE(g_native_jit).enabled
              << " jit_available=" << VMS_NATIVE(g_native_jit).available
              << " p7_enabled=" << p7::pool().enabled() << '\n';
    // Every byte below is an owned fixture. Duplicate names deliberately distinguish
    // receiver package, caller package and JAR root; no commercial JAR is loaded.
    auto& files = VMS_CL(g_jar_files);
    files["resourceprobe/pkg/collision.bin"] = {65};
    files["collision.bin"] = {66};
    files["root-only.bin"] = {82};
    files["resourceprobe/pkg/package-only.bin"] = {67};
    files["resourceprobe/pkg/nested/item.bin"] = {68};
    files["nested/item.bin"] = {88};
    files["item.bin"] = {89};
    files["resourceprobe/other/receiver.bin"] = {69};
    files["resourceprobe/pkg/receiver.bin"] = {70};
    files["receiver.bin"] = {90};
    files["default.bin"] = {71};
    files["empty.bin"] = {};
    files["stream.bin"] = {17, 34, 51, 68, 255};
    files["gc-stream.bin"] = {33, 67, 101, 135};

    if (argc == 4) {
        const int fixture = ensure_class(argv[3]);
        Method* entry = find_method(fixture, "run", "()I", false);
        int result = 0;
        if (entry && !VMS_EXEC(g_thrown)) result = run(entry, {}).i;
        const bool ok = entry && !VMS_EXEC(g_thrown) && result > 0;
        std::cout << (ok ? "PASS " : "FAIL ") << argv[3] << " result=" << result
                  << " thrown=" << VMS_EXEC(g_thrown) << '\n';
        stop_thread();
        p7::pool().shutdown();
        return ok ? 0 : 1;
    }

    int failures = 0;
    int cls = ensure_class("resourceprobe/pkg/ResourceLookupChecks");
    auto invoke = [&](const char* method) {
        Method* entry = find_method(cls, method, "()I", false);
        if (!entry) {
            std::cerr << "FAIL missing fixture method " << method << '\n';
            ++failures;
            return 0;
        }
        return run(entry, {}).i;
    };
    auto check = [&](bool ok, const char* label, int result) {
        std::cout << (ok ? "PASS " : "FAIL ") << label << " result=" << result << '\n';
        if (!ok) ++failures;
    };
    const char* methods[] = {"relativeCollision", "relativeMissing", "relativePackageOnly",
        "relativeNested", "foreignClassLiteral", "foreignObjectClass", "arrayClassReceivers",
        "absoluteRoot", "defaultPackage", "emptyAndMissing", "preservedBoundaries",
        "independentStreams", "run"};
    for (const char* method : methods) {
        int result = invoke(method);
        check(result > 0, method, result);
    }
    int setup = invoke("setupRetained");
    check(setup == 1, "static resource stream before GC", setup);
    const uint64_t before = VMS_HEAP(g_gc_cycles);
    gc_collect("owned resource stream static root");
    check(VMS_HEAP(g_gc_cycles) > before, "real gc_collect increments cycles", int(VMS_HEAP(g_gc_cycles) - before));
    int retained = invoke("verifyRetained");
    check(retained == 1, "static resource stream bytes and position survive GC", retained);
    stop_thread();
    p7::pool().shutdown();
    std::cout << (failures ? "FAIL" : "PASS") << " ResourceLookupChecks failures=" << failures << '\n';
    return failures ? 1 : 0;
}
