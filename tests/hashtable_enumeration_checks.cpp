#define main emulator_main
#ifndef CHECK_VM
#define CHECK_VM "../src/j2mevm.cpp"
#endif
#include CHECK_VM
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/hash-state.bin"; }
#ifndef CHECK_STATE
#define CHECK_STATE "../src/instant_state.inc"
#endif
#include CHECK_STATE

static void check(bool ok, const char* text) {
    if (!ok) { std::cerr << "FAIL " << text << '\n'; std::exit(1); }
}
static Value invoke(const char* name, const char* desc = "()I") {
    int c = ensure_class("HashtableEnumerationChecks");
    Method* m = find_method(c, name, desc, false);
    check(m != nullptr, "fixture method"); return run(m, {});
}
static SavState capture() {
    int c = ensure_class("InstantStateCanvas");
    VMS_RS(g_current) = alloc_obj(c);
    run(find_method(c, "<init>", "()V", false), {Value::of_ref(VMS_RS(g_current))});
    VMS_EXEC(g_midlet) = alloc_obj(ensure_class("java/lang/Object"));
    VMS_EXEC(g_midlet_state) = MidletState::Active;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    SavState state; check(capture_state(state, "hash-fixture.jar"), "capture J2S5"); return state;
}
int main(int argc, char** argv) {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    check(sizeof(Value) == 16, "Value ABI");
#ifdef BASELINE_PROBE
    int result = invoke("cleanupProbe").i;
    std::cout << "baseline_cleanup_rc=" << result << '\n';
    check(result == 17, "old real VM reproduces null-key NumberFormatException");
    check(invoke("setupLegacy").i == 1, "legacy table setup");
    std::string bytes; check(encode_state(capture(), bytes), "old J2S5 encode");
    check(argc == 2, "fixture output required");
    std::ofstream out(argv[1], std::ios::binary); out.write(bytes.data(), bytes.size()); out.close();
    check(bool(out), "old self-authored state written"); return 17;
#else
    check(invoke("cleanupProbe").i == 1, "game-equivalent cleanup removes every transient key");
    check(invoke("run").i >= 20, "real keys/elements bytecode mutations and exception contracts");
    int held = invoke("setupHeld", "()Ljava/lang/String;").ref;
    gc_collect("enumeration owns snapshot references");
    check(VMS_HEAP(g_heap)[held].cls >= 0, "snapshot is a visible Java GC root");
    check(invoke("held", "()Ljava/lang/String;").ref == held, "same dynamic key after GC");
    invoke("dropHeld", "()V"); gc_collect("released enumeration has no hidden roots");
    check(VMS_HEAP(g_heap)[held].cls < 0, "snapshot released without native hidden roots");
    check(invoke("setupSaved").i == 1, "save partially consumed independent enumerations");
    SavState state = capture(), decoded; std::string bytes;
    check(encode_state(state, bytes) && bytes.substr(0, 4) == "J2S5", "unchanged state wire format");
    check(decode_state(bytes, decoded) && apply_state(decoded), "restore snapshot");
    gc_collect("restored enumeration snapshot");
    check(invoke("verifySaved").i == 1, "restored keys values and cursor");
    if (argc == 2) {
        std::ifstream in(argv[1], std::ios::binary); bytes.assign(std::istreambuf_iterator<char>(in), {});
        check(decode_state(bytes, decoded) && apply_state(decoded), "actual old J2S5 table accepted");
        check(invoke("verifyLegacy").i == 1, "fresh enumeration on restored legacy table");
    }
    std::cout << "PASS HashtableEnumerationChecks: cleanup, mutations, GC and J2S5\n";
#endif
}
