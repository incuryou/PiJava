// P2.1-c3: direct test for the two roots added in c3 (VMS_EXEC(g_repaint_owner_ref), VMS_EXEC(g_repaint_yield_canvas)).
// Force a GC while the object is referenced ONLY by that global; it must survive. Then clear the global,
// force another GC; it must be reclaimed (proves the root, not something else, kept it alive).
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cstdio>
static int fails = 0;
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
static bool alive(int ref) { return ref > 0 && size_t(ref) < VMS_HEAP(g_heap).size() && VMS_HEAP(g_heap)[ref].cls >= 0; }
int main() {
    VMS_DIAG(g_log_draw) = false; VMS_CL(g_classpath) = {"romfs/classes"};
    const int cls = ensure_class("java/lang/Object");
    struct Case { const char* name; int* slot; } cases[] = {{"VMS_EXEC(g_repaint_owner_ref)", &VMS_EXEC(g_repaint_owner_ref)}, {"VMS_EXEC(g_repaint_yield_canvas)", &VMS_EXEC(g_repaint_yield_canvas)}};
    for (auto& c : cases) {
        const int ref = alloc_obj(cls);
        *c.slot = ref;
        gc_collect("c3-suspect-root-held");
        char what[160];
        std::snprintf(what, sizeof what, "%s: object referenced only by this global survives forced GC", c.name);
        CHECK(alive(ref), what);
        *c.slot = 0;
        gc_collect("c3-suspect-root-cleared");
        std::snprintf(what, sizeof what, "%s: after clearing the global the object is reclaimed", c.name);
        CHECK(!alive(ref), what);
    }
    std::printf("GC_SUSPECT_ROOTS_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
