// P2.1-c6: 2D -> 3D -> 2D -> getRGB ordering on a real Image (Java fixture RenderOrderChecks) + C09 stays CPU_VALID.
#include <cassert>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};
    VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();int fx=ensure_class("RenderOrderChecks");
    VMS_EXEC(g_midlet_state)=MidletState::Active;
    auto* m=find_method(fx,"run","()I",false);assert(m);
    int n=run(m,{}).i; bool ok=!VMS_EXEC(g_thrown)&&n==7;
    ok = ok && j2me_vmstate::g_vm_storage.render.surface.state()==j2me_render::SurfaceState::CPU_VALID
            && j2me_vmstate::g_vm_storage.render.surface.illegal_transitions()==0;
    std::printf("RENDER_ORDER_RESULT %s checks=%d thrown=%d\n",ok?"PASS":"FAIL",n,VMS_EXEC(g_thrown)?1:0);
    return ok?0:1;
}
