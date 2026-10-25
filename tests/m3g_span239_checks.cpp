#include <cassert>
#include <ctime>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static int fixture=-1,gcid=-1;
static Value& field(int cid,const char* n,const char* d){auto* f=find_field(cid,n,d);assert(f&&f->is_static);return VMS_CL(g_classes)[f->owner].statics[f->slot];}
static std::vector<int> array(int ref){std::vector<int> out;assert(ref>0);for(size_t i=0;i<VMS_HEAP(g_heap)[ref].elems.size();++i)out.push_back(VMS_HEAP(g_heap)[ref].elems.get(i).i);return out;}
static int renderer(){return field(fixture,"g","Ljavax/microedition/m3g/Graphics3D;").ref;}
static std::vector<int> state(int g,bool scratch=true){
    auto out=array(m3g_ref(g,"rasterData","[I"));auto d=array(m3g_ref(g,"depthBuffer","[S"));out.insert(out.end(),d.begin(),d.end());
    for(const char* n:{"deltaA","deltaR","deltaG","deltaB","stepA","stepR","stepG","stepB"})out.push_back(m3g_field(g,n,"I")->i);
    out.push_back(field(gcid,"paintPixel","I").i);
    for(const char* n:{"curS","curT","stepS","stepT","sStepX","sStepY","tStepX","tStepY"}){auto a=array(field(gcid,n,"[F").ref);out.insert(out.end(),a.begin(),a.end());}
    if(scratch){for(const char* n:{"spanZ239","spanPw239","spanInvPw239"})out.push_back(m3g_field(g,n,"F")->i);out.push_back(m3g_field(g,"spanFog239","I")->i);}
    return out;
}
static void setup(int seed){
    VMS_RS(g_m3g_texture238_enabled)=false;
    run(find_method(fixture,"setup","(I)I",false),{Value::of_i(seed)});
    const int g=renderer(),dep=m3g_ref(g,"depthBuffer","[S");
    field(fixture,"depths","[S")=Value::of_ref(dep);
    *m3g_field(g,"currAlphaFactor","I")=field(fixture,"alphaFactor","I");
    for(size_t i=0;i<VMS_HEAP(g_heap)[dep].elems.size();++i){int v=int((uint32_t(seed)*1103515245u+uint32_t(i)*7919u)&65535u)-32768;VMS_HEAP(g_heap)[dep].elems.set(i,Value::of_i(v));}
}
static m3g_span239::Input input(){
    auto ip=array(field(fixture,"ip","[I").ref),fp=array(field(fixture,"fp","[F").ref);
    m3g_span239::Input in;
    in.index=ip[0];in.x=ip[1];in.y=ip[2];in.count=ip[3];in.color=ip[4];in.flags=ip[5];in.alpha_threshold=ip[6];
    in.z=bits_f(fp[0]);in.z_step=bits_f(fp[1]);in.pw=bits_f(fp[2]);in.pw_step=bits_f(fp[3]);in.inv_pw=bits_f(fp[4]);in.inv_pw_step=bits_f(fp[5]);
    in.fog=ip[7];in.fog_step=ip[8];in.fog_rb=ip[9];in.fog_g=ip[10];return in;
}
static bool bridge(const m3g_span239::Input& i){
    auto* m=find_method(gcid,"_rasterSpan239","(IIIIIIIFFFFFFIIII)Z",false);assert(m);
    std::vector<Value> v={Value::of_ref(renderer())};
    for(int a:{i.index,i.x,i.y,i.count,i.color,i.flags,i.alpha_threshold})v.push_back(Value::of_i(a));
    for(float a:{i.z,i.z_step,i.pw,i.pw_step,i.inv_pw,i.inv_pw_step})v.push_back(Value::of_i(f_bits(a)));
    for(int a:{i.fog,i.fog_step,i.fog_rb,i.fog_g})v.push_back(Value::of_i(a));
    return run(m,v).i!=0;
}
static bool equal(const std::vector<int>& a,const std::vector<int>& b,int seed,const char* label){
    if(a==b)return true;
    std::cerr<<"FAIL "<<label<<" seed="<<seed<<" sizes="<<a.size()<<","<<b.size()<<"\n";
    for(size_t k=0;k<std::min(a.size(),b.size());++k)if(a[k]!=b[k])std::cerr<<" word="<<k<<" ref="<<std::hex<<uint32_t(a[k])<<" got="<<uint32_t(b[k])<<std::dec<<"\n";
    return false;
}
int main(int argc,char** argv){
    VMS_RS(g_m3g_row240_enabled)=false; // Keep this historical oracle independent.
    std::cout << std::unitbuf;
    const int cases=argc>1?std::atoi(argv[1]):12032,frames=argc>2?std::atoi(argv[2]):256;
    const int first_seed=argc>3?std::atoi(argv[3]):0;
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    fixture=ensure_class("javax/microedition/m3g/M3GSpan239Checks");gcid=ensure_class("javax/microedition/m3g/Graphics3D");
    assert(run(find_method(fixture,"setupPool","()I",false),{}).i==15);
    auto* reference=find_method(fixture,"reference","()I",false);assert(reference);
    double ref_ms=0,fast_ms=0;uint64_t accepted=0;
    for(int case_index=0;case_index<cases;++case_index){
        const int seed=first_seed+case_index;
        std::vector<int> want,got;
        for(int pass=0;pass<2;++pass){
            const bool native=((seed+pass)&1)!=0;setup(seed);VMS_RS(g_m3g_span239_enabled)=native;
            const auto in=input();std::clock_t start=std::clock();
            if(native){if(!bridge(in)){std::cerr<<"unexpected decline seed="<<seed<<"\n";return 1;}++accepted;}
            else run(reference,{});
            double ms=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;
            if(native){fast_ms+=ms;got=state(renderer());}else{ref_ms+=ms;want=state(renderer());}
        }
        if(!equal(want,got,seed,"span"))return 1;
    }
    std::cout<<"PASS span_cases="<<cases<<" first_seed="<<first_seed<<" accepted="<<accepted<<" reference_cpu_ms="<<ref_ms<<" native_bridge_cpu_ms="<<fast_ms<<"\n";
    // Malformed calls and unsupported callbacks must not commit even one pixel.
    int rejected=0;
    for(int scenario=0;scenario<10;++scenario){
        setup(63);VMS_RS(g_m3g_span239_enabled)=true;auto in=input();
        if(scenario==0)in.count=9;
        if(scenario==1)in.index=64;
        if(scenario==2)in.flags=128;
        if(scenario==3||scenario==4||scenario==5)run(find_method(fixture,"custom","(I)I",false),{Value::of_i(scenario-3)});
        if(scenario==6)field(gcid,"curT","[F")=field(gcid,"curS","[F");
        if(scenario==7){
            int textures=field(gcid,"textures","[Ljavax/microedition/m3g/Texture2D;").ref;
            int tr=VMS_HEAP(g_heap)[textures].elems.get(0).ref,img=m3g_ref(tr,"texImage","Ljavax/microedition/m3g/Image2D;");
            *m3g_field(img,"image","[I")=Value::of_ref(m3g_ref(renderer(),"rasterData","[I"));
        }
        if(scenario==8||scenario==9){in.index=0;in.x=0;in.count=8;in.z=-32768;in.z_step=0;
            in.inv_pw=scenario==8?std::nanf(""):1.0f;in.inv_pw_step=scenario==9?4000000.0f:0;}
        auto before=state(renderer());assert(!bridge(in));assert(before==state(renderer()));++rejected;
        // Restore intentional alias edits before the next case.
        if(scenario==6){int ar=alloc_array("[F",4);field(gcid,"curT","[F")=Value::of_ref(ar);}
        if(scenario==7)assert(run(find_method(fixture,"setupPool","()I",false),{}).i==15);
    }
    std::cout<<"PASS atomic_declines="<<rejected<<" (shape/callback/alias/late_coordinate)\n";
    const int fc=ensure_class("javax/microedition/m3g/M3GSpan239FrameChecks");auto* frame=find_method(fc,"frame","(I)[I",false);assert(frame);
    uint64_t pixels=0,span_before=m3g_span239::stats.calls;double mode_ms[3]={};
    for(int seed=0;seed<frames;++seed){
        std::vector<int> rendered[3],states[3];
        for(int pass=0;pass<3;++pass){int mode=(seed+pass)%3;VMS_RS(g_m3g_span239_enabled)=mode==2;VMS_RS(g_m3g_texture238_enabled)=mode!=0;
            std::clock_t start=std::clock();int ref=run(frame,{Value::of_i(seed)}).ref;mode_ms[mode]+=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;
            assert(ref>0&&VMS_HEAP(g_heap)[ref].elems.size()==4096);rendered[mode]=array(ref);
            int g=field(fc,"g","Ljavax/microedition/m3g/Graphics3D;").ref;states[mode]=state(g,false);
        }
        if(!equal(rendered[0],rendered[1],seed,"image s1")||!equal(rendered[0],rendered[2],seed,"image span")||
           !equal(states[0],states[1],seed,"state s1")||!equal(states[0],states[2],seed,"state span"))return 1;
        pixels+=4096;
    }
    assert(m3g_span239::stats.calls>span_before);
    std::cout<<"PASS full_frames="<<frames<<" pixels_per_mode="<<pixels<<" modes=java,s1_texture,span"<<
        " java_cpu_ms="<<mode_ms[0]<<" s1_cpu_ms="<<mode_ms[1]<<" span_cpu_ms="<<mode_ms[2]<<"\n";
    const auto& st=m3g_span239::stats;
    std::cout<<"COUNTERS span_calls="<<st.calls<<" pixels="<<st.pixels<<" texture_samples="<<st.texture_samples<<" mip_samples="<<st.mip_samples<<
        " depth_rejects="<<st.depth_rejects<<" alpha_rejects="<<st.alpha_rejects<<" sample_prepare_us="<<st.prepare_us<<
        " sample_raster_us="<<st.raster_us<<" sample_commit_us="<<st.commit_us<<" timed_calls="<<st.timed_calls<<"\n";
    return 0;
}
