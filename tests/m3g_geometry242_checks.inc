// Differential tests exercise private kernel contracts, separately from public scenes.
static Value& sf242(int cid,const char* n,const char* d){auto* f=find_field(cid,n,d);assert(f&&f->is_static);return VMS_CL(g_classes)[f->owner].statics[f->slot];}
static int rf242(int cid,const char* n,const char* d){return sf242(cid,n,d).ref;}
static std::vector<int> join242(std::initializer_list<int> refs){std::vector<int> s;for(int r:refs){if(!r){s.push_back(-1);continue;}auto a=array(r);s.insert(s.end(),a.begin(),a.end());}return s;}
static uint32_t rand242(uint32_t& s){s=s*1664525u+1013904223u;return s;}
static const char* mats242[]={"maR","mdR","msR","meR","maG","mdG","msG","meG","maB","mdB","msB","meB","shininess"};
static m3g_lighting242::Input setupLight242(int fixture,int cid,int seed){
    run(find_method(fixture,"lightingSetup","(I)V",false),{Value::of_i(seed)});uint32_t rng=seed;
    for(auto n:{"N_EYE","V_EYE","L_MAT"}){int a=rf242(cid,n,"[F");for(size_t j=0;j<VMS_HEAP(g_heap)[a].elems.size();++j)VMS_HEAP(g_heap)[a].elems.set(j,Value::of_i(f_bits(float(int(rand242(rng)%2000)-1000)*0.001f)));}
    for(auto n:mats242)sf242(cid,n,"F")=Value::of_i(f_bits(float(rand242(rng)%255)*0.003921569f));
    sf242(cid,"shininess","F")=Value::of_i(f_bits(float(seed%65)));sf242(cid,"lightAlpha","I")=Value::of_i(seed%256);
    m3g_lighting242::Input i{rf242(fixture,"lightEye","[F"),rf242(fixture,"lightNorm","Ljavax/microedition/m3g/VertexArray;"),rf242(fixture,"lightMatrix","Ljavax/microedition/m3g/Transform;"),rf242(fixture,"lightMaterial","Ljavax/microedition/m3g/Material;"),seed%4==0?164:165,seed%2!=0,seed%3==0,
        rf242(fixture,"testLights","Ljavax/microedition/m3g/M3GVector;"),rf242(fixture,"lightPos","[F"),rf242(fixture,"lightDir","[F"),3,rf242(fixture,"lightTris","[I"),0,{-1,-1,-1},{0,0,0},rf242(fixture,"lightOut","[I")};
    for(int n=0;n<3;++n){i.prev[n]=(seed+n)%4==0?(seed+n)%7:-1;i.colors[n]=int_bits(rand242(rng));}return i;
}
static std::vector<int> lightingState242(int cid,const m3g_lighting242::Input& in){auto s=join242({in.eye,in.positions,in.directions,in.tris,in.out,rf242(cid,"N_EYE","[F"),rf242(cid,"V_EYE","[F"),rf242(cid,"L_MAT","[F")});for(auto n:mats242)s.push_back(sf242(cid,n,"F").i);s.push_back(sf242(cid,"lightAlpha","I").i);return s;}
static bool lighting242Checks(int fixture,int count,int first,bool legacy){
    int cid=ensure_class("javax/microedition/m3g/Triangle");auto* m=find_method(cid,"calculateLighting","([FLjavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/Transform;Ljavax/microedition/m3g/Material;IZZLjavax/microedition/m3g/M3GVector;[F[FI[IIIIIIII[I)V",false);assert(m);
    uint64_t hash=1469598103934665603ull;double times[2]={};
    for(int seed=first;seed<first+count;++seed){std::vector<int> state[2];for(int pass=0;pass<(legacy?1:2);++pass){int mode=legacy?0:(seed+pass)%2;VMS_RS(g_m3g_mesh242_enabled)=mode!=0;auto i=setupLight242(fixture,cid,seed);
        std::vector<Value> args={Value::of_ref(i.eye),Value::of_ref(i.norm),Value::of_ref(i.matrix),Value::of_ref(i.material),Value::of_i(i.shade),Value::of_i(i.flip),Value::of_i(i.local),Value::of_ref(i.lights),Value::of_ref(i.positions),Value::of_ref(i.directions),Value::of_i(i.scope),Value::of_ref(i.tris),Value::of_i(i.offset)};
        for(int n:i.prev)args.push_back(Value::of_i(n));for(int n:i.colors)args.push_back(Value::of_i(n));args.push_back(Value::of_ref(i.out));
        auto t=std::clock();run(m,args);times[mode]+=1000.0*(std::clock()-t)/CLOCKS_PER_SEC;state[mode]=lightingState242(cid,i);gc_collect("stage242-light");}
        if(!legacy&&state[0]!=state[1]){std::cerr<<"FAIL lighting seed="<<seed<<"\n";for(size_t n=0;n<state[0].size();++n)if(state[0][n]!=state[1][n]){std::cerr<<"word="<<n<<" ref="<<std::hex<<uint32_t(state[0][n])<<" native="<<uint32_t(state[1][n])<<std::dec<<"\n";break;}return false;}
        hash=digest(state[0],hash);
    }
    std::cout<<"PASS lighting cases="<<count<<" start="<<first<<" hash="<<std::hex<<hash<<std::dec<<" ref_cpu_ms="<<times[0]<<" native_cpu_ms="<<times[1]<<"\n";return true;
}
static void declines242(int fixture){
    VMS_RS(g_m3g_mesh242_enabled)=true;int checked=0;
    int vb=run(find_method(fixture,"base","(IIII)Ljavax/microedition/m3g/VertexBuffer;",false),{Value::of_i(4),Value::of_i(3),Value::of_i(2),Value::of_i(7)}).ref;
    int pos=m3g_ref(vb,"positions","Ljavax/microedition/m3g/VertexArray;"),norm=m3g_ref(vb,"normals","Ljavax/microedition/m3g/VertexArray;");
    int mm=alloc_array("[[F",1),mat=alloc_array("[F",16),range=alloc_array("[I",3),sb=alloc_array("[F",4),out=alloc_array("[F",12),on=alloc_array("[F",12),bb=alloc_array("[F",6);
    m3g_mesh242::identity(mat);VMS_HEAP(g_heap)[mm].elems.set(0,Value::of_ref(mat));VMS_HEAP(g_heap)[range].elems.set(1,Value::of_i(4));VMS_HEAP(g_heap)[range].elems.set(2,Value::of_i(1));VMS_HEAP(g_heap)[sb].elems.set(0,Value::of_i(f_bits(0.1f)));
    for(int r:{out,on,bb})for(size_t i=0;i<VMS_HEAP(g_heap)[r].elems.size();++i)VMS_HEAP(g_heap)[r].elems.set(i,Value::of_i(f_bits(0.25f+float(i))));
    for(int scenario=0;scenario<9;++scenario){int p=pos,n=norm,o=out,b=bb,f=0,c=4,m=mm,br=range;
        if(scenario==0)c=129;if(scenario==1)f=INT32_MAX;if(scenario==2)o=sb;if(scenario==3)b=out;if(scenario==4)p=0;if(scenario==5)n=0;
        if(scenario==6)VMS_HEAP(g_heap)[range].elems.set(1,Value::of_i(5));if(scenario==7)VMS_HEAP(g_heap)[range].elems.set(2,Value::of_i(0));if(scenario==8)VMS_HEAP(g_heap)[mm].elems.set(0,Value::of_ref(out));
        auto before=join242({out,on,bb,sb,mat});assert(!m3g_mesh242::skin_block(p,n,m,br,sb,o,on,b,f,c));assert(before==join242({out,on,bb,sb,mat}));++checked;
        VMS_HEAP(g_heap)[range].elems.set(1,Value::of_i(4));VMS_HEAP(g_heap)[range].elems.set(2,Value::of_i(1));VMS_HEAP(g_heap)[mm].elems.set(0,Value::of_ref(mat));
    }
    int raw=alloc_array("[B",12);for(int scenario=0;scenario<4;++scenario){auto before=array(raw);assert(!m3g_mesh242::quantize(scenario==3?0:out,raw,scenario==1?-1:0,scenario==0?1025:12,scenario==2?4:3,1,0,0,0,true));assert(before==array(raw));++checked;}
    int shortMat=alloc_array("[F",15),zero=alloc_array("[F",16);auto before=array(mat);assert(!m3g_mesh242::invert(mat,mat));assert(before==array(mat));++checked;
    assert(!m3g_mesh242::invert(zero,mat));assert(before==array(mat));++checked;assert(!m3g_mesh242::identity(shortMat));++checked;
    int tc=ensure_class("javax/microedition/m3g/Triangle");for(int scenario=0;scenario<5;++scenario){auto i=setupLight242(fixture,tc,71);JavaThreadContext other;
        int vec=m3g_ref(i.lights,"elements","Ljava/util/Vector;");int saved=i.out;
        if(scenario==0)i.offset=INT32_MAX;if(scenario==1)i.material=i.norm;if(scenario==2)i.out=i.tris;if(scenario==3)i.directions=0;
        if(scenario==4){VMS_EXEC(g_monitors)[vec].owner=&other;VMS_EXEC(g_monitors)[vec].depth=1;}
        auto state=lightingState242(tc,i);assert(!m3g_lighting242::draw(tc,i));assert(state==lightingState242(tc,i));++checked;if(scenario==4)VMS_EXEC(g_monitors).erase(vec);
    }
    std::cout<<"PASS atomic_declines="<<checked<<" skin=9 quantize=4 transform=3 lighting=5\n";
}
