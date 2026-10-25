#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/literal-check.bin"; }
#include "../src/instant_state.inc"

static void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
static Value invoke(const char* name, const char* desc = "()I") {
    int c = ensure_class("LiteralIdentityChecks");
    Method* m = find_method(c, name, desc, false);
    check(m != nullptr, "fixture method present");
    return run(m, {});
}
int main(int argc, char** argv) {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"};
    VMS_DIAG(g_log_draw) = false;
    check(sizeof(Value) == 16, "Value ABI");
    check(invoke("run").i >= 7, "real ldc cross-class, empty, Unicode, growth and dynamic identity");
    int c = ensure_class("LiteralIdentityConstant");
    int ref = VMS_CL(g_classes)[c].statics[find_field(c, "VALUE", "Ljava/lang/String;")->slot].ref;
    check(ref == invoke("constant", "()Ljava/lang/String;").ref, "ConstantValue and ldc share identity");
    int retained = invoke("unheld", "()Ljava/lang/String;").ref;
    int victim = make_string("r1-dynamic-garbage");
    gc_collect("literal identity roots");
    check(VMS_HEAP(g_heap)[victim].cls < 0, "ordinary dynamic strings are not hidden roots");
    check(invoke("unheld", "()Ljava/lang/String;").ref == retained, "literal class-static ownership survives GC");
    resver::bump_epoch();
    check(invoke("unheld", "()Ljava/lang/String;").ref == retained, "derived cache rebuild preserves identity");
    check(invoke("setupSaved").i == 1, "saved fixture initialized");
    c = ensure_class("InstantStateCanvas");
    VMS_RS(g_current) = alloc_obj(c);
    run(find_method(c, "<init>", "()V", false), {Value::of_ref(VMS_RS(g_current))});
    VMS_EXEC(g_midlet) = alloc_obj(ensure_class("java/lang/Object"));
    VMS_EXEC(g_midlet_state) = MidletState::Active;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    SavState saved;
    check(capture_state(saved, "literal-fixture.jar"), "capture J2S5");
    std::string bytes;
    check(encode_state(saved, bytes) && bytes.substr(0, 4) == "J2S5", "unchanged J2S5 wire format");
    SavState decoded;
    check(decode_state(bytes, decoded) && apply_state(decoded), "restore new literal roots");
    check(invoke("verifySaved").i == 1, "restore preserves canonical and distinct dynamic references");
    gc_collect("restored literal roots");
    check(invoke("verifySaved").i == 1, "restored literal GC roots");
    SavState bad = decoded;
    for (auto& sc : bad.classes) if (sc.name == kLiteralRootsClass) sc.statics[0].ref = bad.midlet;
    const int old_midlet = VMS_EXEC(g_midlet);
    check(!apply_state(bad) && VMS_EXEC(g_midlet) == old_midlet, "invalid pool rejected before live-state replacement");
    check(invoke("verifySaved").i == 1, "failed preflight preserves live literal identity");
    SavState legacy = decoded;
    legacy.classes.erase(std::remove_if(legacy.classes.begin(), legacy.classes.end(),
        [](const SavClass& sc) { return sc.name == kLiteralRootsClass; }), legacy.classes.end());
    check(apply_state(legacy), "legacy J2S5 without literal-root metadata remains loadable");
    check(invoke("verifyLegacyValue").i == 1, "legacy heap contents and dynamic reference separation preserved");
    check(invoke("run").i >= 7, "fresh literals canonical after legacy restore");
    if (argc == 2) {
        std::ifstream input(argv[1],std::ios::binary);
        std::string old_bytes((std::istreambuf_iterator<char>(input)),{});
        SavState old_state;
        check(decode_state(old_bytes,old_state) && apply_state(old_state), "actual pre-fix J2S5 payload accepted");
        check(invoke("verifyLegacyValue").i == 1, "pre-fix J2S5 heap values and dynamic references preserved");
        check(invoke("run").i >= 7, "new literals canonical after actual pre-fix J2S5 restore");
    }
    std::cout << "PASS LiteralIdentityChecks: real bytecode, GC, ConstantValue, J2S5 and failed restore\n";
}
