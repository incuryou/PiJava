#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
int main() {
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_graphics_profile).reset(true);
    int cid=ensure_class("GraphicsImageChecks");
    int count=run(find_method(cid,"run","()I",false),{}).i;
    if(count!=42){std::cerr<<"FAIL GraphicsImageChecks count="<<count<<"\n";return 1;}
    const auto global=VMS_RS(g_graphics_profile).take_global();
    if(!global.native_calls || global.native_calls!=global.offscreen_calls || global.sampled_calls!=(global.native_calls+63)/64 || global.sampled_calls!=global.sampled_offscreen_calls || VMS_RS(g_graphics_profile).take().sampled_paints) {
        std::cerr<<"FAIL offscreen Graphics global coverage outside Canvas.paint\n";return 1;
    }
    std::vector<uint8_t> malformed={137,80,78,71,13,10,26,10,0,0,0,0,73,72,68,82,0,0,0,0},rgb,alpha;
    int width=0,height=0;std::string error;
    if(decode_png(malformed,rgb,alpha,width,height,error)){std::cerr<<"FAIL malformed PNG header\n";return 1;}
    std::cout<<"PASS GraphicsImageChecks "<<count<<" image, primitive, clip, stride and alpha checks\n";
}
