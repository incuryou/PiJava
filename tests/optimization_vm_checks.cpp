#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
int main() {
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(240*320*3,0);
    const int cls=ensure_class("OptimizationChecks");Method* method=find_method(cls,"run","()I",false);
    VMS_EXEC(g_execution_profile).enabled=true;VMS_EXEC(g_execution_profile).reset();
    const int value=run(method,{}).i;assert(!VMS_EXEC(g_thrown) && value==3537820);
    Method* rec=find_method(cls,"recursive","(I)I",false);assert(rec->cpu_profile.calls==8000);
    assert(rec->cpu_profile.self_cpu_ns>0 && method->cpu_profile.self_cpu_ns>0 && !VMS_EXEC(g_execution_profile).active());
    assert(VMS_HEAP(g_frame_buffer_pool).retained_bytes()<=VMS_HEAP(g_frame_buffer_pool).byte_budget());
#if J2ME_OPT_FRAME_POOL
    assert(VMS_HEAP(g_frame_buffer_pool).hits>8000 && VMS_HEAP(g_frame_buffer_pool).hits>VMS_HEAP(g_frame_buffer_pool).misses);
#endif
    const int stream_class=ensure_class("java/io/DataInputStream");
    Method* read_int=find_method(stream_class,"readInt","()I",false);
    read_int->cpu_profile={};
    int decoded=run(find_method(cls,"fallbackStream","()I",false),{}).i;
    assert(!VMS_EXEC(g_thrown) && decoded==0x01020304 && read_int->cpu_profile.calls==1);
    VMS_EXEC(g_execution_profile).enabled=false;
    // A buffer containing a former reference is not a GC root after Frame exit.
    const int obj=alloc_obj(ensure_class("java/lang/Object"));
    {Frame dead;VMS_HEAP(g_frame_buffer_pool).acquire(dead.locals,10);dead.locals.assign(10,Value::of_ref(obj));}
    gc_collect("frame-pool-roots");assert(VMS_HEAP(g_heap)[obj].cls<0);
    // Wrong return descriptors/static/synchronized methods must never quicken.
    const int gfx=ensure_class("javax/microedition/lcdui/Graphics");
    Method fake;fake.class_id=gfx;fake.native=true;fake.name="setColor";fake.desc="(I)I";
    assert(graphics_intrinsic(&fake)==0);fake.desc="(I)V";fake.flags=0x20;assert(graphics_intrinsic(&fake)==0);
    fake.flags=8;assert(graphics_intrinsic(&fake)==0);
    stop_thread();assert(VMS_EXEC(g_execution_profile).active()==nullptr);
    std::cout<<"PASS OptimizationVM Java recursion/locals, native string alias, pixel results, GC roots, profile scope and descriptor guards checksum="<<value<<"\n";
}
