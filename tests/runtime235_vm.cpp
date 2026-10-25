#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include <iostream>
int main(){
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    int c=ensure_class("Runtime235Checks");int result=run(find_method(c,"run","()I",false),{}).i;
    assert(!VMS_EXEC(g_thrown)&&result>30000);
#if J2ME_OPT_LIBRARY_INDEX
    assert(VMS_NATIVE(g_library_indexes).stats().hits>1000 && VMS_NATIVE(g_library_leaf_calls)>1000);
#endif
    int ch=ensure_class("java/lang/Character");int ht=ensure_class("java/util/Hashtable");
#if J2ME_OPT_LIBRARY_INDEX
    assert(VMS_CL(g_classes)[ch].trusted_library==LibCharacter && VMS_CL(g_classes)[ht].trusted_library==LibHashtable);
    std::vector<uint8_t> bytes;assert(read_whole("romfs/classes/java/lang/Character.class",bytes));
    VMS_CL(g_classes)[ch].trusted_library=0;bytes.back()^=1;bind_library_layout(ch,bytes);assert(!VMS_CL(g_classes)[ch].trusted_library);
    bytes.back()^=1;bind_library_layout(ch,bytes);assert(VMS_CL(g_classes)[ch].trusted_library==LibCharacter);
    const int holder=alloc_obj(ht),arr=alloc_array("[Ljava/lang/Object;",3),k=alloc_obj(ch),v=alloc_obj(ch);
    const auto& klass=VMS_CL(g_classes)[ht];VMS_HEAP(g_heap)[holder].fields[klass.library_keys_slot]=Value::of_ref(arr);VMS_HEAP(g_heap)[holder].fields[klass.library_count_slot]=Value::of_i(1);
    VMS_HEAP(g_heap)[k].fields[VMS_CL(g_classes)[ch].library_value_slot]=Value::of_i(4);VMS_HEAP(g_heap)[v].fields[VMS_CL(g_classes)[ch].library_value_slot]=Value::of_i(4);VMS_HEAP(g_heap)[arr].elems.set(0,Value::of_ref(k));
    auto* method=find_method(ht,"find","(Ljava/lang/Object;)I",false);Value args[2]={Value::of_ref(holder),Value::of_ref(v)},out;
    assert(library_intrinsic_dispatch(LibFind,method,args,out)&&out.i==0);
    // Array version invalidates stale dense positions; weak sweep allows handle reuse.
    VMS_HEAP(g_heap)[arr].elems.set(0,Value::of_ref(0));assert(!library_intrinsic_dispatch(LibFind,method,args,out));
    VMS_HEAP(g_heap)[arr].elems.set(0,Value::of_ref(k));assert(library_intrinsic_dispatch(LibFind,method,args,out)&&out.i==0);
    VMS_HEAP(g_heap)[k].fields[VMS_CL(g_classes)[ch].library_value_slot]=Value::of_i(8);++VMS_NATIVE(g_index_key_epoch);
    assert(library_intrinsic_dispatch(LibFind,method,args,out)&&out.i==-1);
    VMS_NATIVE(g_library_indexes).sweep([](int){return false;});assert(VMS_NATIVE(g_library_indexes).retained_bytes()==0);
#endif
    stop_thread();assert(VMS_NATIVE(g_library_indexes).retained_bytes()==0);
    std::cout<<"PASS 235 VM library checks="<<result<<" reviewed-class hash guard, fallback, null, key epoch, array revision, clear\n";
}
