// P2.1-b: runtime proof that every VMState facade field aliases its legacy global.
// Compile-time proof lives in src/vm_state.h (static_assert per field).
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cstdio>
using namespace j2me_vmstate;
int main() {
    int fails = 0;
    const VMState& s = vm_state();
    if (&s != &kVMState) ++fails;
    // Address identity, sampled across all 7 modules (the full list is static_asserted).
#define SAME(a, b) do { if ((const void*)&(a) != (const void*)&(b)) { std::printf("FAIL %s\n", #a); ++fails; } else std::printf("PASS %s\n", #a); } while (0)
    SAME(s.diag.g_p1, VMS_DIAG(g_p1));
    SAME(s.diag.g_city_diag, VMS_DIAG(g_city_diag));
    SAME(s.natives.g_native_jit, VMS_NATIVE(g_native_jit));
    SAME(s.classes.g_class_objects, VMS_CL(g_class_objects));
    SAME(s.heap.g_root_stacks, VMS_HEAP(g_root_stacks));
    SAME(s.heap.g_frame_buffer_pool, VMS_HEAP(g_frame_buffer_pool));
    SAME(s.exec.ctx.g_running_thread, VMS_EXEC(g_running_thread));  // c4: lives in ExecutionContext
    SAME(s.exec.g_threads, VMS_EXEC(g_threads));
    SAME(s.exec.g_monitors, VMS_EXEC(g_monitors));
    SAME(s.platform.g_audio, VMS_PLAT(g_audio));
    SAME(s.render.g_fb, VMS_RS(g_fb));
    SAME(s.render.g_screen_w, VMS_RS(g_screen_w));
    // Write through facade, observe in global (and back) — same object, not a copy.
    int old_w = VMS_RS(g_screen_w);
    s.render.g_screen_w = old_w + 7;
    if (VMS_RS(g_screen_w) != old_w + 7) { std::puts("FAIL write-through facade->global"); ++fails; }
    VMS_RS(g_screen_w) = old_w;
    if (s.render.g_screen_w != old_w) { std::puts("FAIL write-through global->facade"); ++fails; }
    // Facade adds no storage of its own: the object is only references.
    std::printf("fields desktop=%d switch=%d sizeof(VMState)=%zu\n", kFacadeFieldsDesktop, kFacadeFieldsSwitch, sizeof(VMState));
    if (sizeof(VMState) != sizeof(void*) * (size_t)kFacadeFieldsDesktop) { std::puts("FAIL sizeof mismatch"); ++fails; }
    std::printf("VMSTATE_FACADE_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
