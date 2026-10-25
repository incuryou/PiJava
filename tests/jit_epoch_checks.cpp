#define main emulator_main
#ifndef J2ME_TEST_VM
#define J2ME_TEST_VM "../src/j2mevm.cpp"
#endif
#include J2ME_TEST_VM
#undef main
#include <cassert>
int main() {
    auto& engine=VMS_NATIVE(g_native_jit);assert(engine.configure(true));
    // Fill the previous suite's accounting exactly as the hardware log did.
    // The previous implementation frees executables but retains these totals.
    g_jit_region_stats.bytes=JitRegionMaxBytes;g_jit_region_stats.compiled=490;
    stop_thread();
    Class c;c.name="EpochFixture";VMS_CL(g_classes).push_back(std::move(c));
    Method m;m.class_id=0;m.name="work";m.desc="()I";m.max_locals=1;m.max_stack=2;
    m.code={3,59,132,0,1,26,17,3,232,161,255,249,26,172};
    Frame f;f.method=&m;f.locals={Value::of_i(0)};
    for(unsigned i=0;i<70;i++){f.pc=0;f.stack.clear();f.locals[0]=Value::of_i(0);jit_region_try(f,10000);}
    std::cout<<"compiled="<<g_jit_region_stats.compiled<<" bytes="<<g_jit_region_stats.bytes<<" entries="<<g_jit_region_stats.entries<<" full_skipped="<<g_jit_region_stats.full_skipped<<"\n";
    assert(g_jit_region_stats.compiled>0 && g_jit_region_stats.entries>0);
    assert(g_jit_region_stats.bytes<JitRegionMaxBytes && f.locals[0].i==1000);
    const auto bytes=g_jit_region_stats.bytes,compiled=g_jit_region_stats.compiled;
    auto& entry=g_jit_regions[&m].at[0];entry.stale=true;
    f.pc=0;f.stack.clear();f.locals[0]=Value::of_i(0);jit_region_try(f,10000);
    assert(g_jit_region_stats.compiled==compiled+1 && g_jit_region_stats.resident==1);
    assert(g_jit_region_stats.bytes==bytes && f.locals[0].i==1000);
    // Failed replacement preserves the interpreter's PC/locals and releases
    // the old allocation exactly once. Do not repeatedly ask an exhausted OS.
    entry.stale=true;jit236::Executable::fault()=1;
    f.pc=0;f.stack.clear();f.locals[0]=Value::of_i(0);
    assert(!jit_region_try(f,10000) && f.pc==0 && f.locals[0].i==0);
    assert(engine.admission_closed && !g_jit_region_stats.bytes && !g_jit_region_stats.resident);
    jit236::Executable::fault()=0;
    const auto epoch=engine.epoch();stop_thread();assert(engine.epoch()!=epoch);
    assert(g_jit_regions.empty() && g_jitr_casts.empty() && !g_jit_region_last && !g_jit_region_last_m);
    assert(!g_jit_region_stats.bytes && !g_jit_region_stats.compiled && !g_jit_region_stats.entries);
    assert(!engine.admission_closed);
    // A direct engine reset is also guarded on the next admission.
    g_jit_region_stats.bytes=JitRegionMaxBytes;engine.clear();f.pc=0;f.stack.clear();
    jit_region_try(f,10000);assert(!g_jit_region_stats.bytes);
    std::cout<<"PASS JIT budget reclaimed across suite/snapshot and direct engine epochs\n";
    // Production iastore helper: exact write barrier, live read-after-write,
    // and a failing write returns the untouched operands at the same bytecode.
    const int array=alloc_array("[I",4);
    Method write;write.class_id=0;write.name="write";write.desc="([III)I";
    write.max_locals=3;write.max_stack=3;write.code={42,27,28,79,42,27,46,172};
    Frame wf;wf.method=&write;
    for(int i=0;i<70;++i){wf.pc=0;wf.stack.clear();wf.locals={Value::of_ref(array),Value::of_i(2),Value::of_i(0x81234567)};jit_region_try(wf,100);}
    assert(wf.pc==7 && wf.stack.size()==1 && wf.stack[0].i==int32_t(0x81234567));
    const auto writes=resver::g_stats.array_content;
    wf.pc=0;wf.stack.clear();assert(jit_region_try(wf,4)==4 && wf.pc==4 && wf.stack.empty());
    assert(resver::g_stats.array_content==writes+1);
    for(int idx:{-1,4,INT32_MAX}) {
        wf.pc=0;wf.stack.clear();wf.locals[1]=Value::of_i(idx);
        assert(jit_region_try(wf,100)==3 && wf.pc==3 && wf.stack.size()==3);
        assert(wf.stack[0].ref==array && wf.stack[1].i==idx && wf.stack[2].i==int32_t(0x81234567));
        assert(resver::g_stats.array_content==writes+1);
    }
    wf.pc=0;wf.stack.clear();wf.locals[0]=Value::of_ref(0);wf.locals[1]=Value::of_i(0);
    assert(jit_region_try(wf,100)==3 && wf.pc==3 && wf.stack.size()==3);
    const int refs=alloc_array("[Ljava/lang/Object;",1);
    const auto version=VMS_HEAP(g_heap)[refs].elems.content_version();
    assert(jitr_h_iastore(refs,0,123)==jitr::Bail && VMS_HEAP(g_heap)[refs].elems.content_version()==version);
    assert(!VMS_HEAP(g_heap)[refs].elems.get(0).ref);
    std::cout<<"PASS primitive iastore: write barrier, budget boundary, null/bounds same-PC fallback, reference-array exclusion\n";
}
