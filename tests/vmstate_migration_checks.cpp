// P2.1-c batch 1: runtime proof that migrated module state has exactly ONE storage location
// (inside j2me_vmstate::g_vm_storage) and that every access path reaches it.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cstdio>
static int fails = 0;
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
template <class T> static bool inside(const T& x) {
    auto* lo = reinterpret_cast<const char*>(&j2me_vmstate::g_vm_storage);
    auto* p = reinterpret_cast<const char*>(&x);
    return p >= lo && p + sizeof(T) <= lo + sizeof(j2me_vmstate::g_vm_storage);
}
int main() {
    using namespace j2me_vmstate;
    CHECK(&VMS_DIAG(g_p1) == &g_vm_storage.diag.g_p1 && inside(VMS_DIAG(g_p1)), "diag.g_p1 single storage");
    CHECK(&VMS_DIAG(g_city_diag) == &g_vm_storage.diag.g_city_diag && inside(VMS_DIAG(g_city_diag)), "diag.g_city_diag single storage");
    CHECK(&VMS_DIAG(g_p1_profile) == &g_vm_storage.diag.g_p1_profile, "diag.g_p1_profile single storage");
    CHECK(&VMS_DIAG(g_p1_m3g_base) == &g_vm_storage.diag.g_p1_m3g_base, "diag.g_p1_m3g_base single storage");
    CHECK(&VMS_DIAG(g_exception_traces) == &g_vm_storage.diag.g_exception_traces, "diag.g_exception_traces single storage");
    CHECK(&VMS_DIAG(g_log_draw) == &g_vm_storage.diag.g_log_draw && VMS_DIAG(g_log_draw) == true, "diag.g_log_draw single storage, initial value true");
    CHECK(VMS_DIAG(g_p1).enabled && VMS_DIAG(g_p1).session_id != 0, "diag.g_p1 initialisers ran (enabled=1, session_id set)");
    CHECK(&vm_state().diag == &g_vm_storage.diag, "facade diag = one reference to VMStorage.diag");
    VMS_DIAG(g_p1).events = 4242; CHECK(g_vm_storage.diag.g_p1.events == 4242, "write through accessor visible in storage");
    VMS_DIAG(g_p1).events = 0;
    using namespace j2me_vmstate;
    CHECK(&VMS_NATIVE(g_native_jit) == &g_vm_storage.natives.g_native_jit && inside(VMS_NATIVE(g_native_jit)), "natives.g_native_jit single storage");
    CHECK(&VMS_NATIVE(g_library_indexes) == &g_vm_storage.natives.g_library_indexes, "natives.g_library_indexes single storage");
    CHECK(&VMS_NATIVE(g_index_key_epoch) == &g_vm_storage.natives.g_index_key_epoch && VMS_NATIVE(g_index_key_epoch) == 0, "natives.g_index_key_epoch single storage, initial 0");
    CHECK(&VMS_NATIVE(g_library_leaf_calls) == &g_vm_storage.natives.g_library_leaf_calls && VMS_NATIVE(g_library_leaf_calls) == 0, "natives.g_library_leaf_calls single storage, initial 0");
    CHECK(&vm_state().natives == &g_vm_storage.natives, "facade natives = one reference to VMStorage.natives");
    // reset semantics unchanged: stop_thread() clears epoch + index + JIT, reset_runtime_stats() clears leaf calls
    VMS_NATIVE(g_index_key_epoch) = 77; VMS_NATIVE(g_library_leaf_calls) = 5;
    stop_thread();
    CHECK(VMS_NATIVE(g_index_key_epoch) == 0 && VMS_NATIVE(g_library_indexes).retained_bytes() == 0, "stop_thread clears epoch + library index");
    reset_runtime_stats();
    CHECK(VMS_NATIVE(g_library_leaf_calls) == 0, "reset_runtime_stats clears leaf calls");
    {
    using namespace j2me_vmstate;
    CHECK(VMS_HEAP(g_heap).size() == 1 && &VMS_HEAP(g_heap) == &g_vm_storage.heap.g_heap && inside(VMS_HEAP(g_heap)), "heap.g_heap single storage, starts with slot 0 (null) only");
#define H1(n) CHECK(&VMS_HEAP(n) == &g_vm_storage.heap.n && inside(VMS_HEAP(n)), "heap." #n " single storage");
    H1(g_alloc_since) H1(g_gc_logs) H1(g_gc_mark_scratch) H1(g_gc_work_scratch) H1(g_array_alloc_since_gc) H1(g_array_payload_live)
    H1(g_array_payload_peak) H1(g_gc_array_refs_scanned) H1(g_gc_cycles) H1(g_free) H1(g_root_stacks) H1(g_frame_buffer_pool)
#undef H1
    CHECK(VMS_HEAP(g_alloc_since) == 0 && VMS_HEAP(g_gc_cycles) == 0 && VMS_HEAP(g_free).empty(), "heap counters start at 0");
    CHECK(&vm_state().heap == &g_vm_storage.heap, "facade heap = one reference to VMStorage.heap");
    VMS_CL(g_classpath) = {"romfs/classes"};
    const int r = alloc_obj(ensure_class("java/lang/Object"));
    CHECK(r == 1 && VMS_HEAP(g_heap).size() == 2 && g_vm_storage.heap.g_heap.size() == 2, "alloc_obj grows the migrated heap");
    gc_collect("c3-unit");
    CHECK(VMS_HEAP(g_gc_cycles) == 1 && g_vm_storage.heap.g_free.size() == 1, "gc_collect updates counters / free list in VMStorage.heap");
    }
    {
    using namespace j2me_vmstate;
    CHECK(&VMS_CL(g_jar_files) == &g_vm_storage.classes.g_jar_files && inside(VMS_CL(g_jar_files)), "classes.g_jar_files single storage");
    CHECK(&VMS_CL(g_props) == &g_vm_storage.classes.g_props, "classes.g_props single storage");
    CHECK(&VMS_CL(g_classes) == &g_vm_storage.classes.g_classes && inside(VMS_CL(g_classes)), "classes.g_classes single storage");
    CHECK(&VMS_CL(g_class_index) == &g_vm_storage.classes.g_class_index, "classes.g_class_index single storage");
    CHECK(&VMS_CL(g_classpath) == &g_vm_storage.classes.g_classpath, "classes.g_classpath single storage");
    CHECK(&VMS_CL(g_class_objects) == &g_vm_storage.classes.g_class_objects, "classes.g_class_objects single storage");
    CHECK(&vm_state().classes == &g_vm_storage.classes, "facade classes = one reference to VMStorage.classes");
    // class-init state: one owner. Load a class, its init_state lives in the Class inside g_vm_storage only.
    VMS_CL(g_classpath) = {"romfs/classes"};
    const int c = ensure_class("java/lang/Object");
    CHECK(c >= 0 && VMS_CL(g_class_index).at("java/lang/Object") == c, "ensure_class registers in the migrated index");
    const Class& k = g_vm_storage.classes.g_classes[size_t(c)];
    CHECK(&k == &VMS_CL(g_classes)[size_t(c)] && &k.init_state == &VMS_CL(g_classes)[size_t(c)].init_state, "class init_state reached only through the deque owned by VMStorage.classes (single owner)");
    stop_thread();
    }
    std::printf("VMSTATE_MIGRATION_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
