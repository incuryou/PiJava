#define main emulator_main
#ifndef CHECK_VM
#define CHECK_VM "../src/j2mevm.cpp"
#endif
#include CHECK_VM
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/font-state.bin"; }
#ifndef CHECK_STATE
#define CHECK_STATE "../src/instant_state.inc"
#endif
#include CHECK_STATE
static void check(bool ok,const char* label) { if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);} }
static Value invoke(const char* n,const char* d="()I",std::vector<Value> a={}) {
    Method* m=find_method(ensure_class("FontChecks"),n,d,false);check(m,"fixture method");return run(m,a);
}
static SavState capture() {
    int c=ensure_class("InstantStateCanvas");VMS_RS(g_current)=alloc_obj(c);
    run(find_method(c,"<init>","()V",false),{Value::of_ref(VMS_RS(g_current))});
    VMS_EXEC(g_midlet)=alloc_obj(ensure_class("java/lang/Object"));VMS_EXEC(g_midlet_state)=MidletState::Active;
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
    SavState s;check(capture_state(s,"font-fixture.jar"),"capture J2S5");return s;
}
static int stat(const char* key,const char* desc) {
    int c=ensure_class("FontChecks");return VMS_CL(g_classes)[c].statics[find_field(c,key,desc)->slot].ref;
}
int main(int argc,char** argv) {
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    check(sizeof(Value)==16,"Value ABI");
#ifdef BASELINE_PROBE
    invoke("legacySetup");std::string bytes;check(encode_state(capture(),bytes),"old J2S5 encode");
    check(argc==2,"old state output");std::ofstream f(argv[1],std::ios::binary);f.write(bytes.data(),bytes.size());f.close();check(bool(f),"old state write");
    int c=ensure_class("javax/microedition/lcdui/Font");int font=run(find_method(c,"getFont","(III)Ljavax/microedition/lcdui/Font;",false),{Value::of_i(0),Value::of_i(0),Value::of_i(8)}).ref;
    int medium=invoke("draw","(IIIIII)Ljavax/microedition/lcdui/Image;",{Value::of_i(0),Value::of_i(0),Value::of_i(0),Value::of_i(20),Value::of_i(10),Value::of_i(10)}).ref;
    std::ofstream golden(std::string(argv[1])+".medium.rgb",std::ios::binary);const auto& pixels=VMS_HEAP(g_heap)[medium].pixels;
    golden.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());golden.close();check(bool(golden),"old medium raster golden");
    int h=run(find_method(c,"getHeight","()I",false),{Value::of_ref(font)}).i;
    check(h==16,"baseline reproduces ignored SMALL");std::cout<<"BASELINE_REPRODUCED SMALL height="<<h<<'\n';return 17;
#else
    check(invoke("run").i==72,"72 font configurations and exceptions");
    int crisp=invoke("smallCrisp","()Ljavax/microedition/lcdui/Image;").ref;
    const auto& crisp_pixels=VMS_HEAP(g_heap)[crisp].pixels;bool crisp_ink=false;
    for(uint8_t v:crisp_pixels){check(v==0||v==255,"opaque small native glyphs have no resampling grey");crisp_ink|=v!=0;}
    check(crisp_ink,"native small glyphs produce real ink");
    const uint16_t drink[12]={0x0fc0,0xe840,0xafc0,0xa840,0xafc0,0xa400,0xefe0,0x1920,0x0aa0,0x0f20,0x00c0,0};
    for(int y=0;y<12;++y)for(int x=0;x<12;++x)check(crisp_pixels[(y*240+x)*3]==((drink[y]&(0x8000>>x))?255:0),"native 喝 retains every source stroke at exact pixel coordinates");
    int stroke=invoke("topStroke","()Ljavax/microedition/lcdui/Image;").ref;
    const auto& stroke_pixels=VMS_HEAP(g_heap)[stroke].pixels;
    for(int x=0;x<16;++x)check(stroke_pixels[((4*24)+(4+x))*3]==((0x1ff0&(0x8000>>x))?255:0),"complete top horizontal of 是 at actual TOP anchor");
    for(int size:{8,0,16})for(int style=0;style<8;++style)for(int face:{0,32,64}) {
        auto render=[&](int anchor,int x,int y) {
            int ref=invoke("draw","(IIIIII)Ljavax/microedition/lcdui/Image;",{Value::of_i(size),Value::of_i(style),Value::of_i(face),Value::of_i(anchor),Value::of_i(x),Value::of_i(y)}).ref;
            return VMS_HEAP(g_heap)[ref].pixels;
        };
        const MidpFont font{face,style,size};const int w=text_width("Ag中é",font),h=font.height(),base=font.baseline();
        auto top=render(4|16,10,10);bool ink=false;
        for(int y=0;y<100;++y)for(int x=0;x<120;++x)if(top[(y*120+x)*3]) {
            ink=true;check(x>=10&&x<10+w&&y>=10&&y<10+h,"ink lies within Java metrics");
        }
        check(ink,"real raster ink");
        check(top==render(8|32,10+w,10+h),"right bottom metrics");
        check(top==render(1|64,10+w/2,10+base),"center baseline metrics");
        // The same selected value is copied into Graphics. Dropping the Font
        // object cannot invalidate it or create a hidden native Java root.
        int gfx=stat("graphics","Ljavax/microedition/lcdui/Graphics;");gc_collect("font value copy");
        check(midp_font(gfx,true).size==size,"font survives GC as native value");
        auto& obj=VMS_HEAP(g_heap)[gfx];(void)obj;
        gfx_opacity_set(gfx,91);check(gfx_opacity(gfx)==91&&midp_font(gfx,true).style==style,"alpha setter preserves font");
        midp_font_set(gfx,{face,style,size},true);check(gfx_opacity(gfx)==91,"font setter preserves alpha");
        VMS_RS(g_clip)[gfx]={15,14,3,3,true};int target=VMS_RS(g_gfx_target)[gfx];
        auto& px=VMS_HEAP(g_heap)[target].pixels;std::fill(px.begin(),px.end(),0);
        draw_string(gfx,target,10,10,0xffffff,"Ag中é");
        for(int y=0;y<100;++y)for(int x=0;x<120;++x)if(px[(y*120+x)*3])check(x>=15&&x<18&&y>=14&&y<17,"glyph clipping");
        VMS_RS(g_clip).erase(gfx);draw_string(gfx,target,INT32_MAX,INT32_MIN,0xffffff,"中Ag");
    }
    if(argc==2) {
        int ref=invoke("draw","(IIIIII)Ljavax/microedition/lcdui/Image;",{Value::of_i(0),Value::of_i(0),Value::of_i(0),Value::of_i(20),Value::of_i(10),Value::of_i(10)}).ref;
        std::ifstream golden(std::string(argv[1])+".medium.rgb",std::ios::binary);std::vector<uint8_t> pixels(std::istreambuf_iterator<char>(golden),{});
        check(pixels.size()==36000&&pixels==VMS_HEAP(g_heap)[ref].pixels,"default medium actual old/new raster bytes identical");
    }
    int lines=invoke("lines","()Ljavax/microedition/lcdui/Image;").ref;
    const auto& linepx=VMS_HEAP(g_heap)[lines].pixels;
    for(int y:{22,23,36,37})for(int x=0;x<100;++x)check(linepx[(y*100+x)*3]==0,"small multiline leading remains clear");
    int copied=invoke("valueCopy","()Ljavax/microedition/lcdui/Font;").ref;
    gc_collect("Graphics holds a font value, no hidden object root");
    check(VMS_HEAP(g_heap)[copied].cls<0,"selected Font has no hidden strong root");
    check(midp_font(stat("graphics","Ljavax/microedition/lcdui/Graphics;"),true).size==8,"graphics copy outlives original Font");
    invoke("savedSetup");SavState saved=capture(),decoded;std::string bytes;
    check(encode_state(saved,bytes)&&bytes.substr(0,4)=="J2S5"&&decode_state(bytes,decoded)&&apply_state(decoded),"new font J2S5 round trip");
    gc_collect("restored font values");check(invoke("savedCheck").i==1,"font/graphics restore");
    if(argc==2) {
        std::ifstream f(argv[1],std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(f),{});
        check(decode_state(bytes,decoded)&&apply_state(decoded),"actual pre-R3 zero-field Font/Graphics J2S5 accepted");
        check(invoke("legacyCheck").i==1,"legacy empty tags retain medium metrics");
    }
    std::cout<<"PASS FontChecks: 72 configurations, advances/ink/anchors, clipping, GC, alpha and old/new J2S5\n";
#endif
}
