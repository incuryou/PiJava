#define main emulator_main
#include PROBE_VM
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/baseline-literal.bin"; }
#include PROBE_STATE
int main() {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"};
    auto value = [](const char* owner, const char* name) {
        int c = ensure_class(owner);
        return run(find_method(c, name, "()Ljava/lang/String;", false), {}).ref;
    };
    int a = value("LiteralIdentityChecks", "literal");
    int b = value("LiteralIdentityOther", "value");
    std::cout << "literal_equal=" << (a == b) << '\n';
    int fixture = ensure_class("LiteralIdentityChecks");
    run(find_method(fixture, "setupSaved", "()I", false), {});
    int canvas = ensure_class("InstantStateCanvas");
    VMS_RS(g_current) = alloc_obj(canvas);
    run(find_method(canvas, "<init>", "()V", false), {Value::of_ref(VMS_RS(g_current))});
    VMS_EXEC(g_midlet) = alloc_obj(ensure_class("java/lang/Object"));
    VMS_EXEC(g_midlet_state) = MidletState::Active;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    SavState saved; std::string bytes;
    if (!capture_state(saved, "literal-fixture.jar") || !encode_state(saved, bytes)) return 19;
    std::ofstream out("build/baseline-literal.bin",std::ios::binary);out.write(bytes.data(),bytes.size());out.close();
    return a == b ? 0 : 17;
}
