#ifndef GRAPHICS_BENCH_VM
#define GRAPHICS_BENCH_VM "../src/j2mevm.cpp"
#endif
#define main emulator_main
#include GRAPHICS_BENCH_VM
#undef main

static uint64_t hash_pixels() {
    uint64_t hash = 1469598103934665603ULL;
    for (uint8_t b : VMS_RS(g_fb)) { hash ^= b; hash *= 1099511628211ULL; }
    return hash;
}
template<class F> static int64_t timed(int rounds, F fn) {
    for (int i=0;i<16;++i) fn();
    int64_t begin=paint_clock_us();
    for (int i=0;i<rounds;++i) fn();
    return paint_clock_us()-begin;
}
int main(int argc, char** argv) {
    int rounds=argc>1?std::atoi(argv[1]):256;
#ifdef GRAPHICS_BENCH_PROFILE
    bool profiling=argc>2&&std::string(argv[2])=="--profile";
    VMS_RS(g_graphics_profile).reset(profiling);
#endif
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_RS(g_fb).assign(240*320*3,0);
    int bench=ensure_class("GraphicsPaintBenchmark");
    int image_cls=ensure_class("javax/microedition/lcdui/Image");
    int graphics_cls=ensure_class("javax/microedition/lcdui/Graphics");
    int image=alloc_obj(image_cls), graphics=alloc_obj(graphics_cls);
    VMS_HEAP(g_heap)[image].width=128;VMS_HEAP(g_heap)[image].height=128;
    VMS_HEAP(g_heap)[image].pixels.resize(128*128*3);
    VMS_HEAP(g_heap)[image].alpha.resize(128*128);
    for(size_t i=0;i<128*128;++i) {
        VMS_HEAP(g_heap)[image].pixels[i*3]=uint8_t(i*17);VMS_HEAP(g_heap)[image].pixels[i*3+1]=uint8_t(i*29);VMS_HEAP(g_heap)[image].pixels[i*3+2]=uint8_t(i*47);
        VMS_HEAP(g_heap)[image].alpha[i]=i%7==0?128:(i%3==0?0:255);
    }
    gfx_bind(graphics,0,0);
    Method* paint=find_method(bench,"paint","(Ljavax/microedition/lcdui/Graphics;Ljavax/microedition/lcdui/Image;I)V",false);
    Method* clip=find_method(graphics_cls,"setClip","(IIII)V",false);
    Method* draw=find_method(graphics_cls,"drawImage","(Ljavax/microedition/lcdui/Image;III)V",false);
    Frame cf,df;cf.locals.resize(5);df.locals.resize(5);
    cf.locals[0]=df.locals[0]=Value::of_ref(graphics);df.locals[1]=Value::of_ref(image);df.locals[4]=Value::of_i(20);
    std::cout<<"{\"scope\":\"desktop fixed clipped atlas paint; not real-game or Switch FPS\",\"rounds\":"<<rounds<<",\"samples\":[";
    bool first=true;
    for(int count:{84,433}) for(int sample=0;sample<5;++sample) {
        std::fill(VMS_RS(g_fb).begin(),VMS_RS(g_fb).end(),0);
        int64_t vm_us=timed(rounds,[&] {
#ifdef GRAPHICS_BENCH_PROFILE
            VMS_RS(g_graphics_profile).begin_paint();
#endif
            run(paint,{Value::of_ref(graphics),Value::of_ref(image),Value::of_i(count)});
#ifdef GRAPHICS_BENCH_PROFILE
            VMS_RS(g_graphics_profile).end_paint();
#endif
        });
        uint64_t vm_hash=hash_pixels();
        auto native=[&] {
#ifdef GRAPHICS_BENCH_PROFILE
            VMS_RS(g_graphics_profile).begin_paint();
#endif
            for(int i=0;i<count;++i) {
                int x=i*13%232,y=i*17%308;
                cf.locals[1]=Value::of_i(x);cf.locals[2]=Value::of_i(y);cf.locals[3]=Value::of_i(8);cf.locals[4]=Value::of_i(12);call_native(clip,cf);
                df.locals[2]=Value::of_i(x-i*7%120);df.locals[3]=Value::of_i(y-i*11%116);call_native(draw,df);
            }
            cf.locals[1]=cf.locals[2]=Value::of_i(0);cf.locals[3]=Value::of_i(240);cf.locals[4]=Value::of_i(320);call_native(clip,cf);
#ifdef GRAPHICS_BENCH_PROFILE
            VMS_RS(g_graphics_profile).end_paint();
#endif
        };
        std::fill(VMS_RS(g_fb).begin(),VMS_RS(g_fb).end(),0);
        int64_t native_us=timed(rounds,native);uint64_t native_hash=hash_pixels();
        std::fill(VMS_RS(g_fb).begin(),VMS_RS(g_fb).end(),0);
        int64_t raster_us=timed(rounds,[&] {
            for(int i=0;i<count;++i) {
                int x=i*13%232,y=i*17%308;
                raster_region::draw(VMS_HEAP(g_heap)[image].pixels,VMS_HEAP(g_heap)[image].alpha,128,128,VMS_RS(g_fb),240,320,0,0,128,128,0,x-i*7%120,y-i*11%116,{true,x,y,8,12});
            }
        });uint64_t raster_hash=hash_pixels();
        if(vm_hash!=native_hash||vm_hash!=raster_hash||VMS_EXEC(g_thrown)) return 2;
        if(!first)std::cout<<",";first=false;
        std::cout<<"{\"image_calls\":"<<count<<",\"clip_calls\":"<<(count+1)<<",\"vm_us\":"<<vm_us<<",\"native_us\":"<<native_us<<",\"raster_us\":"<<raster_us<<",\"pixel_hash\":\""<<vm_hash<<"\"}";
    }
    std::cout<<"]";
#ifdef GRAPHICS_BENCH_PROFILE
    auto stats=VMS_RS(g_graphics_profile).take();
    std::cout<<",\"profiling_enabled\":"<<(profiling?"true":"false")<<",\"profile\":{\"sampled_paints\":"<<stats.sampled_paints<<",\"image_calls\":"<<stats.image_calls<<",\"clip_calls\":"<<stats.clip_calls<<",\"raster_calls\":"<<stats.raster_calls<<",\"image_native_ns\":"<<stats.image_native_ns<<",\"clip_native_ns\":"<<stats.clip_native_ns<<",\"raster_ns\":"<<stats.raster_ns<<"}";
#endif
    std::cout<<"}\n";
}
