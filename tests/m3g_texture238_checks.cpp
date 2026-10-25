#include <cassert>
#include <ctime>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static std::vector<int> state(int cid){auto* f=find_field(cid,"result","[I");int a=VMS_CL(g_classes)[f->owner].statics[f->slot].ref;std::vector<int> v;for(size_t i=0;i<VMS_HEAP(g_heap)[a].elems.size();++i)v.push_back(VMS_HEAP(g_heap)[a].elems.get(i).i);return v;}
int main(int argc,char** argv){
    VMS_RS(g_m3g_row240_enabled)=false;
    VMS_RS(g_m3g_span239_enabled)=false; // Isolate the stage238 texture island.
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    int cid=ensure_class("javax/microedition/m3g/M3GTexture238Checks");
    auto* setup=find_method(cid,"setup","()I",false);auto* sample=find_method(cid,"sample","(I)I",false);auto* fallback=find_method(cid,"fallback","()I",false);
    assert(setup&&sample&&fallback);assert(run(setup,{}).i==15);
    int count=argc>1?std::atoi(argv[1]):12000;double reference_ms=0,native_ms=0;
    for(int seed=0;seed<count;seed++){
        std::vector<int> before,after;
        for(int pass=0;pass<2;pass++){
            bool enabled=bool((seed+pass)&1);VMS_RS(g_m3g_texture238_enabled)=enabled;
            std::clock_t start=std::clock();run(sample,{Value::of_i(seed)});double ms=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;
            if(enabled){after=state(cid);native_ms+=ms;}else{before=state(cid);reference_ms+=ms;}
        }
        if(before!=after){std::cerr<<"FAIL seed="<<seed<<"\n";for(size_t i=0;i<before.size();i++)std::cerr<<i<<" "<<std::hex<<before[i]<<" "<<after[i]<<"\n";return 1;}
    }
    assert(VMS_RS(g_m3g_texture238_calls)==uint64_t(count));assert(VMS_RS(g_m3g_texture238_mips)>0);
    const int wraps=run(find_method(cid,"wrapChecks","()I",false),{}).i;assert(wraps==1539);
    auto* bench=find_method(cid,"bench","(I)I",false);int hashes[2]={};
    for(int v=0;v<2;v++){
        VMS_RS(g_m3g_texture238_enabled)=v!=0;std::clock_t start=std::clock();hashes[v]=run(bench,{Value::of_i(20000)}).i;
        std::cout<<"BENCH enabled="<<v<<" samples=20000 cpu_ms="<<1000.0*(std::clock()-start)/CLOCKS_PER_SEC<<" checksum="<<hashes[v]<<"\n";
    }
    assert(hashes[0]==hashes[1]);

    VMS_RS(g_m3g_texture238_enabled)=false;const int want=run(fallback,{}).i;
    VMS_RS(g_m3g_texture238_enabled)=true;const uint64_t calls=VMS_RS(g_m3g_texture238_calls);const int got=run(fallback,{}).i;
    assert(want==got&&VMS_RS(g_m3g_texture238_calls)==calls&&VMS_RS(g_m3g_texture238_declined)>0);
    const auto before_sub=VMS_RS(g_m3g_texture238_calls);
    assert(run(find_method(cid,"subclassFallback","()I",false),{}).i==1);
    assert(VMS_RS(g_m3g_texture238_calls)==before_sub);
    // Unsupported invocation must have no observable mutation of texture state.
    int gcid=ensure_class("javax/microedition/m3g/Graphics3D");auto* p=m3g_texture238::stat(gcid,1,"paintPixel","I");const int old=p->i;
    assert(!m3g_apply_texture238(gcid,1,0,0,5)&&p->i==old);
    assert(!m3g_apply_texture238(gcid,std::nanf(""),0,0,1)&&p->i==old);
    int fullcid=ensure_class("javax/microedition/m3g/M3GTexture238FrameChecks");
    auto* frame=find_method(fullcid,"frame","(I)[I",false);assert(frame);
    uint64_t full_pixels=0;
    for(int config=0;config<32;++config){
        std::vector<int> images[2];const auto previous_calls=VMS_RS(g_m3g_texture238_calls);
        for(int v=0;v<2;++v){
            VMS_RS(g_m3g_texture238_enabled)=v!=0;int ref=run(frame,{Value::of_i(config)}).ref;
            assert(ref>0&&VMS_HEAP(g_heap)[ref].elems.size()==48*48);
            for(size_t i=0;i<VMS_HEAP(g_heap)[ref].elems.size();++i)images[v].push_back(VMS_HEAP(g_heap)[ref].elems.get(i).i);
        }
        assert(images[0]==images[1]);assert(VMS_RS(g_m3g_texture238_calls)>previous_calls);
        full_pixels+=images[0].size();
    }
    std::cout<<"PASS 32 complete image comparisons; pixels="<<full_pixels<<" fog/dither/AA/2-level-filters/2-image-filters\n";
    std::cout<<"PASS "<<count<<" differential cases; 9 state words each; POT/NPOT/1xN, 1/2/4 units, 3 LOD, 2 filters, 11 blends, mixed wrap; custom callbacks fall back; no partial writes. reference_cpu_ms="<<reference_ms<<" native_cpu_ms="<<native_ms<<" texture_units="<<VMS_RS(g_m3g_texture238_units)<<" mip_units="<<VMS_RS(g_m3g_texture238_mips)<<"\n";
}
