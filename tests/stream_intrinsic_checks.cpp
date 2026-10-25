// gcc -O2 -c src/puff.c -o build/puff.o
// g++ -O2 -std=c++17 -fno-exceptions tests/stream_intrinsic_checks.cpp build/puff.o -o build/stream_intrinsic_checks
// java -m jdk.compiler/com.sun.tools.javac.Main -source 8 -target 8 -bootclasspath romfs/classes -classpath romfs/classes -d build/test-classes tests/java/IoChecks.java
// build/stream_intrinsic_checks
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

static unsigned checks = 0;
static void check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL " << message << " check=" << checks << "\n"; std::exit(1); }
}
static int add_class(const std::string& name, int super) {
    Class c; c.name=name; c.super_id=super; c.init_state=2;
    c.instance_fields=super<0?0:VMS_CL(g_classes)[super].instance_fields;
    int id=(int)VMS_CL(g_classes).size(); VMS_CL(g_classes).push_back(std::move(c)); VMS_CL(g_class_index)[name]=id; return id;
}
static void override_int(int cls, const char* name, int value) {
    Method m; m.class_id=cls; m.name=name; m.desc="()I"; m.max_locals=1; m.max_stack=1;
    m.code={17,uint8_t(value>>8),uint8_t(value),172};
    VMS_CL(g_classes)[cls].methods.push_back(std::move(m));
}
// Real interpreter invoke sites, including virtual resolution and the site's
// cached runtime class. Each wrapper returns the caught exception reference.
static Method* wrapper(int owner, const std::string& name, const std::string& desc, bool special=false) {
    std::string key="StreamCall"+std::to_string(owner)+name+desc+(special?"S":"V");
    auto found=VMS_CL(g_class_index).find(key);
    if(found!=VMS_CL(g_class_index).end())return &VMS_CL(g_classes)[found->second].methods[0];
    int id=add_class(key,-1); Class& c=VMS_CL(g_classes)[id]; c.cp.resize(7);
    c.cp[1].tag=1;c.cp[1].utf=VMS_CL(g_classes)[owner].name;
    c.cp[2].tag=7;c.cp[2].a=1;
    c.cp[3].tag=1;c.cp[3].utf=name;
    c.cp[4].tag=1;c.cp[4].utf=desc;
    c.cp[5].tag=12;c.cp[5].a=3;c.cp[5].b=4;
    c.cp[6].tag=10;c.cp[6].a=2;c.cp[6].b=5;
    Method m;m.class_id=id;m.name="call";m.desc="()I";m.flags=8;m.max_locals=6;m.max_stack=6;
    m.code={42};
    if(desc=="([BII)I")m.code.insert(m.code.end(),{43,28,29});
    else if(desc=="(J)J")m.code.push_back(31);
    else if(desc=="(I)V")m.code.push_back(27);
    m.code.insert(m.code.end(),{uint8_t(special?183:182),0,6});
    int end=(int)m.code.size();
    m.code.push_back(returns_void(desc)?177:desc=="(J)J"?173:172);
    m.handlers.push_back({0,end,(int)m.code.size(),0});m.code.push_back(176);
    c.methods.push_back(std::move(m));return &c.methods[0];
}
static Value call(int owner, int receiver, const std::string& name, const std::string& desc="()I",
                  std::vector<Value> rest={}, bool special=false) {
    rest.insert(rest.begin(),Value::of_ref(receiver));return run(wrapper(owner,name,desc,special),rest);
}
static int field_slot(int cls, const char* name, const char* desc="I") {
    return find_field(cls,name,desc)->slot;
}
static int byte_stream(int cls, const std::vector<uint8_t>& bytes, int offset=0, int length=-1) {
    int array=alloc_array("[B",bytes.size());
    for(size_t i=0;i<bytes.size();++i)VMS_HEAP(g_heap)[array].elems.set(i,Value::of_i(bytes[i]));
    int object=alloc_obj(cls);
    int base=ensure_class("java/io/ByteArrayInputStream");
    run(find_method(base,"<init>","([BII)V",false),
        {Value::of_ref(object),Value::of_ref(array),Value::of_i(offset),Value::of_i(length<0?(int)bytes.size():length)});
    return object;
}
static int data_stream(int cls, int input) {
    int object=alloc_obj(cls);
    run(find_method(ensure_class("java/io/DataInputStream"),"<init>","(Ljava/io/InputStream;)V",false),
        {Value::of_ref(object),Value::of_ref(input)});return object;
}
static bool exception(Value result, const char* expected, bool null_message=true) {
    if(result.ref<=0 || VMS_CL(g_classes)[VMS_HEAP(g_heap)[result.ref].cls].name!=expected)return false;
    return !null_message || VMS_HEAP(g_heap)[result.ref].fields[field_slot(VMS_HEAP(g_heap)[result.ref].cls,"detail","Ljava/lang/String;")].ref==0;
}
static void benchmark(int bai, int dis, const char* name, const char* desc, int width) {
    Method* call_site=wrapper(dis,name,desc);
    int id=add_class(std::string("StreamLoop")+name,-1);
    VMS_CL(g_classes)[id].cp=VMS_CL(g_classes)[call_site->class_id].cp;
    Method m;m.class_id=id;m.name="loop";m.desc="(Ljava/lang/Object;I)I";m.flags=8;
    m.max_locals=3;m.max_stack=2;
    // sum=0; while (n>0) { sum+=input.readX(); --n; } return sum;
    m.code={3,61,27,158,0,16,42,182,0,6,28,96,61,132,1,255,167,255,242,28,172};
    VMS_CL(g_classes)[id].methods.push_back(std::move(m));
    const int iterations=30000;
    int plain_bai=add_class(std::string("StreamPlainBAI")+name,bai);
    int plain_dis=add_class(std::string("StreamPlainDIS")+name,dis);
    std::vector<uint8_t> bytes(iterations*width,1);
    int fast_source=byte_stream(bai,bytes),slow_source=byte_stream(plain_bai,bytes);
    int fast=data_stream(dis,fast_source),slow=data_stream(plain_dis,slow_source);
    int64_t fast_us=INT64_MAX,slow_us=INT64_MAX;
    int expected=0;for(int i=0;i<width;++i)expected=(expected<<8)|1;
    for(int repeat=0;repeat<3;++repeat) {
        for(int path=0;path<2;++path) {
            int source=path?slow_source:fast_source,input=path?slow:fast;
            VMS_HEAP(g_heap)[source].fields[field_slot(bai,"pos")]=Value::of_i(0);
            int64_t start=paint_clock_us();
            int result=run(&VMS_CL(g_classes)[id].methods[0],{Value::of_ref(input),Value::of_i(iterations)}).i;
            int64_t elapsed=paint_clock_us()-start;
            check(result==int_bits(uint32_t(expected)*uint32_t(iterations)),"benchmark checksum");
            check(call(bai,source,"available").i==0,"benchmark consumed exact byte count");
            if(path)slow_us=std::min(slow_us,elapsed);else fast_us=std::min(fast_us,elapsed);
        }
    }
    std::cout<<"BENCH "<<name<<" "<<iterations<<" reads: exact fast="<<fast_us
             <<"us inherited Java="<<slow_us<<"us ratio="<<double(slow_us)/fast_us<<"x\n";
}
int main() {
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    int bai=ensure_class("java/io/ByteArrayInputStream"),dis=ensure_class("java/io/DataInputStream");
    check(stream_intrinsic_kind(find_method(bai,"read","()I",false))==StreamRead,"resolved BAI identity");
    Method wrong=*find_method(bai,"read","()I",false);wrong.desc="()J";
    check(stream_intrinsic_kind(&wrong)==StreamNone,"return descriptor must match");
    wrong=*find_method(dis,"readUnsignedByte","()I",false);wrong.flags|=0x0020;
    check(stream_intrinsic_kind(&wrong)==StreamNone,"synchronized methods retain monitor path");
    int source=byte_stream(bai,{0,128,255,17},1,2);
    check(call(bai,source,"available").i==2,"constructor offset and count");
    check(call(bai,source,"read").i==128,"unsigned read");
    call(bai,source,"mark","(I)V",{Value::of_i(0)});
    check(call(bai,source,"skip","(J)J",{Value::of_l(INT64_MAX)}).l==1,"wide skip clamped");
    check(call(bai,source,"read").i==-1,"single read EOF");
    int target=alloc_array("[B",3);
    check(call(bai,source,"read","([BII)I",{Value::of_ref(target),Value::of_i(3),Value::of_i(0)}).i==0,"zero length before EOF");
    check(exception(call(bai,source,"read","([BII)I",{Value::of_ref(0),Value::of_i(-1),Value::of_i(-1)}),"java/lang/NullPointerException"),"null before bounds and EOF");
    check(exception(call(bai,source,"read","([BII)I",{Value::of_ref(target),Value::of_i(3),Value::of_i(1)}),"java/lang/IndexOutOfBoundsException"),"bounds before EOF");
    check(exception(call(bai,source,"read","([BII)I",{Value::of_ref(target),Value::of_i(0),Value::of_i(INT32_MAX)}),"java/lang/IndexOutOfBoundsException"),"range avoids overflow");
    call(bai,source,"reset","()V");
    check(call(bai,source,"read","([BII)I",{Value::of_ref(target),Value::of_i(1),Value::of_i(2)}).i==1 && VMS_HEAP(g_heap)[target].elems.get(1).i==-1,"bulk partial read and signed destination");
    check(call(bai,source,"skip","(J)J",{Value::of_l(INT64_MIN)}).l==0,"negative wide skip");
    check(call(bai,source,"available").i==0,"EOF position");
    check(call(bai,source,"read","([BII)I",{Value::of_ref(target),Value::of_i(0),Value::of_i(1)}).i==-1,"bulk EOF");
    source=byte_stream(bai,{1,2,3,4},1,INT32_MAX);
    check(call(bai,source,"available").i==3,"constructor clamps long requested length");
    source=byte_stream(bai,{1,2,3,4,5});
    int buf=VMS_HEAP(g_heap)[source].fields[field_slot(bai,"buf","[B")].ref;
    check(call(bai,source,"read","([BII)I",{Value::of_ref(buf),Value::of_i(1),Value::of_i(4)}).i==4 &&
          VMS_HEAP(g_heap)[buf].elems.get(4).i==4,"overlapping buffer copy uses memmove");
    for(int length=0;length<4;++length) {
        source=byte_stream(bai,std::vector<uint8_t>(length,42));int d=data_stream(dis,source);
        check(exception(call(dis,d,"readInt"),"java/io/EOFException"),"partial int raises EOF");
        check(call(bai,source,"available").i==0,"partial int consumes remaining bytes");
    }
    source=byte_stream(bai,{128,254,220,137,171,205,239});int d=data_stream(dis,source);
    check(call(dis,d,"readByte","()B").i==-128,"Java readByte reaches intrinsic");
    check(call(dis,d,"readShort","()S").i==-292,"Java readShort reaches intrinsic");
    check(call(dis,d,"readInt").i==int_bits(0x89abcdef),"big endian signed int");
    check(exception(call(dis,d,"readUnsignedByte"),"java/io/EOFException"),"unsigned byte EOF");
    d=data_stream(dis,byte_stream(bai,{255,254}));
    check(call(dis,d,"readUnsignedShort").i==65534,"unsigned short is not sign extended");
    source=byte_stream(bai,{1});d=data_stream(dis,source);
    check(exception(call(dis,d,"readUnsignedShort"),"java/io/EOFException") && call(bai,source,"available").i==0,"partial unsigned short consumes byte");
    d=data_stream(dis,0);
    check(exception(call(dis,d,"readUnsignedByte"),"java/lang/NullPointerException",false),"null input follows Java path");
    int child=add_class("StreamOverrideBAI",bai);override_int(child,"read",99);
    source=byte_stream(child,{1,2,3,4});d=data_stream(dis,source);
    check(call(bai,source,"read").i==99,"BAI override selected");
    check(call(dis,d,"readInt").i==0x63636363,"wrapped BAI override selected");
    check(call(bai,source,"read","()I",{},true).i==1,"invokespecial parent body still executes for subclass");
    Value ignored;Value args[]={Value::of_ref(source)};
    check(!stream_intrinsic_dispatch(StreamRead,find_method(bai,"read","()I",false),args,ignored),"even inherited or special BAI method rejects subclass");
    int custom=add_class("StreamCustomInput",ensure_class("java/io/InputStream"));override_int(custom,"read",42);
    d=data_stream(dis,alloc_obj(custom));
    check(call(dis,d,"readInt").i==0x2a2a2a2a,"custom InputStream executes Java read");
    int data_child=add_class("StreamOverrideDataByte",dis);override_int(data_child,"readUnsignedByte",7);
    d=data_stream(data_child,byte_stream(bai,{1,2,3,4}));
    check(call(dis,d,"readInt").i==0x07070707,"DIS inherited int respects unsigned byte override");
    int short_child=add_class("StreamOverrideDataShort",dis);override_int(short_child,"readUnsignedShort",0x1234);
    d=data_stream(short_child,byte_stream(bai,{1,2,3,4}));
    check(call(dis,d,"readInt").i==0x12341234,"DIS inherited int respects unsigned short override");
    // Reuse one warmed site across changing runtime classes and back again.
    source=byte_stream(bai,{55});check(call(bai,source,"read").i==55,"polymorphic site returns to BAI fast path");
    args[0]=Value::of_ref(d);
    check(!stream_intrinsic_dispatch(StreamInt,find_method(dis,"readInt","()I",false),args,ignored),"DIS concrete class gate");
    int io=ensure_class("IoChecks");int count=run(find_method(io,"run","()I",false),{}).i;
    check(count>=50,"full Java IO regression suite");
    benchmark(bai,dis,"readByte","()B",1);
    benchmark(bai,dis,"readShort","()S",2);
    benchmark(bai,dis,"readInt","()I",4);
    std::cout<<"PASS StreamIntrinsicChecks "<<checks<<" C++/VM checks + "<<count<<" Java IO checks\n";
}
