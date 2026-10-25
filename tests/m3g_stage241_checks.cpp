#include <cassert>
#include <ctime>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
static int fixture,gcid;
static Value& field(int cid,const char* n,const char* d){auto* f=find_field(cid,n,d);assert(f&&f->is_static);return VMS_CL(g_classes)[f->owner].statics[f->slot];}
static int ref(const char* n,const char* d){return field(fixture,n,d).ref;}
static std::vector<int> array(int r){assert(r>0);std::vector<int> out;for(size_t i=0;i<VMS_HEAP(g_heap)[r].elems.size();i++)out.push_back(VMS_HEAP(g_heap)[r].elems.get(i).i);return out;}
static std::vector<int> state(){int g=ref("g","Ljavax/microedition/m3g/Graphics3D;");auto o=array(m3g_ref(g,"rasterData","[I"));auto d=array(m3g_ref(g,"depthBuffer","[S"));o.insert(o.end(),d.begin(),d.end());o.push_back(field(gcid,"paintPixel","I").i);return o;}
static uint32_t random32(uint32_t& s){s=s*1664525u+1013904223u;return s;}
static bool equal(const std::vector<int>& a,const std::vector<int>& b,const char* what,int seed){
    if(a==b)return true;std::cerr<<"FAIL "<<what<<" seed="<<seed<<" size="<<a.size()<<","<<b.size()<<"\n";
    for(size_t i=0;i<std::min(a.size(),b.size());++i)if(a[i]!=b[i]){std::cerr<<"word="<<i<<" ref="<<std::hex<<uint32_t(a[i])<<" got="<<uint32_t(b[i])<<std::dec<<"\n";break;}return false;}
static void mode(bool on){VMS_RS(g_m3g_sprite241_enabled)=VMS_RS(g_m3g_prep241_enabled)=VMS_EXEC(g_fast_float241_enabled)=on;}
static m3g_sprite241::Input setup(int seed){
    int g=ref("g","Ljavax/microedition/m3g/Graphics3D;"),s=ref("spr","Ljavax/microedition/m3g/Image2D;");
    field(fixture,"depths","[S")=Value::of_ref(m3g_ref(g,"depthBuffer","[S"));
    uint32_t rng=uint32_t(seed);
    for(int r:{m3g_ref(g,"rasterData","[I"),m3g_ref(s,"image","[I")})for(size_t i=0;i<VMS_HEAP(g_heap)[r].elems.size();i++)VMS_HEAP(g_heap)[r].elems.set(i,Value::of_i(int_bits(random32(rng))));
    for(size_t i=0;i<VMS_HEAP(g_heap)[ref("depths","[S")].elems.size();i++)VMS_HEAP(g_heap)[ref("depths","[S")].elems.set(i,Value::of_i(int(random32(rng)%65536)-32768));
    int lengths[]={1,2,7,8,9,31,127,240,320,511,512,1023,1024};
    m3g_sprite241::Input in;in.image=s;in.count=lengths[seed%13];in.index=0;in.left=seed%31;in.width=2048-in.left;
    in.z=(seed*31)%65536-32768;in.flags=(seed/13)%8;in.alpha=seed%257;in.threshold=(seed/17)%256;
    in.u=(seed%191-95)*0.75f;in.du=((seed%17)-8)*0.375f;in.fog=(seed/7)%256;in.fog_rb=0x00a500d3;in.fog_g=0x71;
    if(seed>=4096&&seed<4160){int n=seed-4096;in.z=-32768;in.alpha=256;in.threshold=0;in.flags=6;
        int fog[]={INT32_MIN,INT32_MAX,-1,0,1,255,256,65535};in.fog=fog[n%8];
        float coord[]={-0.0f,0.0f,-0.0001f,0.99999994f,2047.9999f,-4096.0f,4096.0f,1e-30f};in.u=coord[n/8];in.du=0.25f;}
    run(find_method(fixture,"blend","(I)I",false),{Value::of_i(seed%6)});
    int ip[]={in.index,in.count,in.tex_row,in.left,in.width,in.z,in.flags,in.alpha,in.threshold,in.fog,in.fog_rb,in.fog_g};
    for(int i=0;i<12;++i)VMS_HEAP(g_heap)[ref("ip","[I")].elems.set(i,Value::of_i(ip[i]));
    VMS_HEAP(g_heap)[ref("fp","[F")].elems.set(0,Value::of_i(f_bits(in.u)));VMS_HEAP(g_heap)[ref("fp","[F")].elems.set(1,Value::of_i(f_bits(in.du)));
    field(gcid,"paintPixel","I")=Value::of_i(0x719357ab);return in;
}
static bool bridge(const m3g_sprite241::Input& i){
    auto* m=find_method(gcid,"_spriteRow241","(Ljavax/microedition/m3g/Image2D;IIIIIFFIIIIIII)Z",false);assert(m);
    std::vector<Value> v={Value::of_ref(ref("g","Ljavax/microedition/m3g/Graphics3D;")),Value::of_ref(i.image)};
    for(int a:{i.index,i.count,i.tex_row,i.left,i.width})v.push_back(Value::of_i(a));
    v.push_back(Value::of_i(f_bits(i.u)));v.push_back(Value::of_i(f_bits(i.du)));
    for(int a:{i.z,i.flags,i.alpha,i.threshold,i.fog,i.fog_rb,i.fog_g})v.push_back(Value::of_i(a));return run(m,v).i!=0;
}
static uint64_t digest(const std::vector<int>& a,uint64_t h=1469598103934665603ull){for(int v:a)for(unsigned s=0;s<32;s+=8){h^=(uint32_t(v)>>s)&255;h*=1099511628211ull;}return h;}
static void floats(int count){
    const unsigned ops[]={11,12,13,98,102,106,110,114,118,134,139,149,150};
    uint32_t edge[]={0,0x80000000u,0x7f800000u,0xff800000u,0x7fc00001u,0x7fa00001u,0xffc01234u,1,0x80000001u,0x7f7fffffu,0xff7fffffu,0x3f800000u,0x4effffffu,0x4f000000u,0xcf000000u,0x007fffffu};
    uint32_t rng=7143;uint64_t checks=0,nan_equal=0;
    for(unsigned op:ops){Method m;m.class_id=fixture;m.name="float241fixture";m.desc="(II)I";m.flags=8;m.max_locals=2;m.max_stack=4;
        int arity=op<14?0:op==118||op==134||op==139?1:2;
        if(arity)m.code.push_back(26);if(arity==2)m.code.push_back(27);m.code.push_back(uint8_t(op));m.code.push_back(172);
        for(int j=0;j<count;++j){uint32_t a=j<256?edge[j%16]:random32(rng),b=j<256?edge[(j/16)%16]:random32(rng);
            std::vector<Value> args={Value::of_i(int_bits(a)),Value::of_i(int_bits(b))};
            VMS_EXEC(g_fast_float241_enabled)=false;int want=run(&m,args).i;VMS_EXEC(g_fast_float241_enabled)=true;int got=run(&m,args).i;
            bool fp_result=op!=139&&op!=149&&op!=150;
            if(want!=got){std::cerr<<"float mismatch op="<<op<<" j="<<j<<"\n";std::abort();}++checks;
        }
    }
    // Batches may not run past the original 1024-op polling boundary, nor
    // consume unsupported opcodes / operands when disabled or underflowing.
    Method m;for(int j=0;j<2048;++j)m.code.insert(m.code.end(),{12,13,98,87});Frame f;f.method=&m;
    assert(execute_fast_batch(f,1024)==1024&&f.pc==1024&&f.stack.empty());
    Frame stopped;stopped.method=&m;VMS_EXEC(g_fast_float241_enabled)=false;assert(execute_fast_batch(stopped,1024)==0&&stopped.pc==0&&stopped.stack.empty());
    m.code={98};Frame short_stack;short_stack.method=&m;short_stack.stack.push_back(Value::of_i(5));VMS_EXEC(g_fast_float241_enabled)=true;
    assert(execute_fast_batch(short_stack,8)==0&&short_stack.stack.size()==1&&short_stack.stack[0].i==5&&short_stack.pc==0);
    std::cout<<"PASS float_opcode_checks="<<checks<<" arithmetic_nan_payload_equivalent="<<nan_equal<<" budget_and_fallback=3\n";
}
int main(int argc,char** argv){
    std::cout<<std::unitbuf;const int rows=argc>1?std::atoi(argv[1]):4160,resources=argc>2?std::atoi(argv[2]):180,scenes=argc>3?std::atoi(argv[3]):64;
    bool legacy=argc>4&&std::string(argv[4])=="legacy";
    int row_start=argc>5?std::atoi(argv[5]):0,resource_start=argc>6?std::atoi(argv[6]):0,scene_start=argc>7?std::atoi(argv[7]):0;
    VMS_CL(g_classpath)=legacy?std::vector<std::string>{"build/test-classes","build/legacy240-classes"}:std::vector<std::string>{"build/test-classes","romfs/classes"};
    VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();fixture=ensure_class("javax/microedition/m3g/M3GStage241Checks");gcid=ensure_class("javax/microedition/m3g/Graphics3D");
    assert(run(find_method(fixture,"pool","()I",false),{}).i==1);
    if(!legacy)floats(2048);
    double rowtime[2]={},preptime[2]={},frametime[2]={};uint64_t row_pixels=0;
    if(!legacy){
        for(int seed=row_start;seed<row_start+rows;++seed){std::vector<int> states[2];
            for(int pass=0;pass<2;++pass){int m=(seed+pass)&1;auto in=setup(seed);mode(m!=0);std::clock_t start=std::clock();
                if(m){assert(bridge(in));row_pixels+=in.count;}else run(find_method(fixture,"rowReference","()I",false),{});
                rowtime[m]+=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;states[m]=state();}
            if(!equal(states[0],states[1],"sprite_row",seed))return 1;
        }
        std::cout<<"PASS sprite_rows="<<rows<<" pixels="<<row_pixels<<" ref_cpu_ms="<<rowtime[0]<<" native_cpu_ms="<<rowtime[1]<<"\n";
        int declines=0;
        for(int scenario=0;scenario<10;++scenario){auto in=setup(71);mode(true);
            if(scenario==0)in.count=1025;if(scenario==1)in.index=INT32_MAX;if(scenario==2)in.width=2049;
            if(scenario==3)in.tex_row=1;if(scenario==4)in.u=std::nanf("");
            if(scenario==5){in.count=20;in.u=0;in.du=1000000;in.z=-32768;in.alpha=256;in.threshold=0;}
            if(scenario==6)run(find_method(fixture,"blend","(I)I",false),{Value::of_i(6)});
            int g=ref("g","Ljavax/microedition/m3g/Graphics3D;"),src=ref("spr","Ljavax/microedition/m3g/Image2D;");int saved=m3g_ref(src,"image","[I");
            if(scenario==7)*m3g_field(src,"image","[I")=Value::of_ref(m3g_ref(g,"rasterData","[I"));
            if(scenario==8)in.flags=128;if(scenario==9)in.alpha=257;
            auto before=state();assert(!bridge(in));assert(before==state());*m3g_field(src,"image","[I")=Value::of_ref(saved);++declines;
        }
        std::cout<<"PASS sprite_atomic_declines="<<declines<<"\n";
        int dst=alloc_array("[I",4),src=alloc_array("[B",4),pal=alloc_array("[B",4);
        for(int i=0;i<4;i++){VMS_HEAP(g_heap)[dst].elems.set(i,Value::of_i(713+i));VMS_HEAP(g_heap)[src].elems.set(i,Value::of_i(i));}
        auto saved=array(dst);assert(!m3g_image241::decode(src,0,pal,dst,0,4,100,false));assert(saved==array(dst));
        assert(!m3g_image241::decode(src,0,0,dst,0,4,100,false));assert(saved==array(dst));
        assert(!m3g_image241::convert(dst,-1,4,100));assert(saved==array(dst));
        assert(!m3g_image241::convert(dst,0,4,101));assert(saved==array(dst));
        std::cout<<"PASS preparation_atomic_declines=4\n";
    }
    uint64_t resource_hash=1469598103934665603ull,frame_hash=resource_hash,resource_words=0;
    for(int seed=resource_start;seed<resource_start+resources;++seed){std::vector<int> states[2];
        for(int pass=0;pass<(legacy?1:2);++pass){int m=legacy?0:(seed+pass)&1;mode(m!=0);std::clock_t start=std::clock();
            int out=run(find_method(fixture,"resources","(I)[I",false),{Value::of_i(seed)}).ref;
            preptime[m]+=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;states[m]=array(out);}
        if(!legacy&&!equal(states[0],states[1],"resources",seed))return 1;
        resource_hash=digest(states[0],resource_hash);resource_words+=states[0].size();gc_collect("stage241-test");
    }
    for(int m=0;m<(legacy?1:2);++m){mode(m!=0);assert(run(find_method(fixture,"errors","()I",false),{}).i==255);}
    std::cout<<"PASS resource_cases="<<resources<<" compared_words="<<resource_words<<" hash="<<std::hex<<resource_hash<<std::dec<<" ref_cpu_ms="<<preptime[0]<<" native_cpu_ms="<<preptime[1]<<" public_exceptions=8\n";
    for(int seed=scene_start;seed<scene_start+scenes;++seed){std::vector<int> pixels[2],states[2];
        for(int pass=0;pass<(legacy?1:2);++pass){int m=legacy?0:(seed+pass)&1;mode(m!=0);std::clock_t start=std::clock();
            int out=run(find_method(fixture,"scene","(I)[I",false),{Value::of_i(seed)}).ref;
            frametime[m]+=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;pixels[m]=array(out);states[m]=state();}
        if(!legacy&&(!equal(pixels[0],pixels[1],"sprite_scene",seed)||!equal(states[0],states[1],"sprite_state",seed)))return 1;
        frame_hash=digest(pixels[0],frame_hash);frame_hash=digest(states[0],frame_hash);gc_collect("stage241-test");
    }
    std::cout<<"PASS sprite_scenes="<<scenes<<" pixels_per_mode="<<uint64_t(scenes)*240*320<<" hash="<<std::hex<<frame_hash<<std::dec<<" ref_cpu_ms="<<frametime[0]<<" native_cpu_ms="<<frametime[1]<<" legacy240="<<legacy<<"\n";
    if(!legacy){
        // Fixed storage overflow, report selection, non-overlap and reset.
        paint_hotspots241::reset();paint_hotspots241::begin();std::vector<Frame> f(1);Method methods[66];
        for(auto& m:methods){m.class_id=fixture;m.name="sample";f[0].method=&m;paint_hotspots241::sample(f);}
        assert(paint_hotspots241::current.used==64&&paint_hotspots241::current.dropped==2);
        paint_hotspots241::finish(60000);assert(paint_hotspots241::have_pending&&paint_hotspots241::slow_paints==1);
        paint_hotspots241::begin();paint_hotspots241::finish(1000);assert(paint_hotspots241::pending.wall_us==60000);
        paint_hotspots241::reset();assert(!paint_hotspots241::active&&!paint_hotspots241::have_pending);
        // Exercise real interpreter checkpoint capture, without altering events.
        mode(false);paint_hotspots241::begin();++VMS_RS(g_paint_depth);auto start=paint_clock_us();
        run(find_method(fixture,"spin","(I)I",false),{Value::of_i(1000000)});--VMS_RS(g_paint_depth);
        paint_hotspots241::finish(paint_clock_us()-start);assert(paint_hotspots241::total_samples>0);paint_hotspots241::report();
        std::cout<<"PASS checkpoint_sampler=5 real_interpreter_samples="<<paint_hotspots241::total_samples<<"\n";
        mode(true);m3g_image241::report();m3g_sprite241::report();
    }
    return 0;
}
