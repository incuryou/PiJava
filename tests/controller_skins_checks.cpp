#include "../src/host_skin.h"
#include "../src/host_menu_settings.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static size_t allocations=0;
void* operator new(size_t size){if(void* p=std::malloc(size?size:1)){++allocations;return p;}std::abort();}
void* operator new[](size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
void operator delete[](void* p,size_t)noexcept{std::free(p);}
#if !__has_include("../src/controller_skin_labels.h")
int main(){
    if(!host_skin::path(4)||!host_skin::path(5)||!host_skin::path(6)){
        std::fputs("ASSERT three additional controller skin IDs must resolve without replacing phone IDs\n",stderr);return 1;
    }
    return 0;
}
#else
#include "../src/controller_skin_labels.h"
#include <fstream>
#include <set>
static void require(bool ok,const char* m){if(!ok){std::fprintf(stderr,"ASSERT %s\n",m);std::exit(1);}}
static bool outside(int x,int y,int w,int h){return x+w<=400||x>=880||y+h<=40||y>=680;}
int main(int argc,char** argv){
    require(argc==2,"isolated settings fixture path");using namespace host_keymap;
    require(host_menu::bezel_count==7,"all seven bezel choices reachable");
    require(std::string(host_skin::path(1))=="romfs:/skins/phone-blue.skin"&&std::string(host_skin::path(2))=="romfs:/skins/phone-black.skin"&&std::string(host_skin::path(3))=="romfs:/skins/phone-ivory.skin","old skin identities unchanged");
    for(int id=4;id<=6;++id){require(host_skin::path(id)&&std::string(host_menu::bezel_name(id))!=host_menu::bezel_name(0),"new skin paths and menu names");host_menu::Settings s;s.bezel=id;s.filter=display_filter::Filter::ScanlinesDX;
        require(host_menu::save_settings(argv[1],s),"new ID saves");host_menu::Settings out;require(host_menu::load_settings(argv[1],out)&&out.bezel==id&&out.filter==s.filter,"new ID and existing filter round trip");}
    for(int id=0;id<4;++id){std::ofstream f(argv[1],std::ios::binary);f<<"J2ME_MENU 2\nbezel="<<id<<"\nshow_fps=0\n";f.close();host_menu::Settings out;require(host_menu::load_settings(argv[1],out)&&out.bezel==id&&!out.show_fps,"old settings retain identities");}
    host_controller::Legends labels;Config automatic;
    require(labels.update(automatic,false,4),"initial numeric legends");
    require(labels.label(A).text=="5"&&labels.label(Y).text=="确认"&&labels.label(DUp).text=="2","numeric automatic labels match active phone codes");
    require(labels.label(SUp).text=="↑"&&labels.label(SDown).text=="↓"&&labels.label(SLeft).text=="←"&&labels.label(SRight).text=="→","left-stick legends always describe phone arrows");
    require(labels.label(host_controller::Menu).text=="菜单"&&labels.label(host_controller::Zero).text=="0","sticks have only fixed menu and zero labels");
    const auto generation=labels.generation();const char* storage=labels.label(A).text.c_str();
    const auto stable_allocations=allocations;
    for(int n=0;n<100;++n)require(!labels.update(automatic,false,4)&&labels.generation()==generation&&labels.label(A).text.c_str()==storage,"unchanged frames reuse legend storage and generation");
    require(allocations==stable_allocations,"stable mapping checks allocate no heap storage");
    require(labels.update(automatic,true,4)&&labels.label(A).text=="确认"&&labels.label(DUp).text=="↑","standard automatic mode invalidates numeric legends");
    Config custom;custom.automatic=false;custom.bindings=empty_bindings();custom.bindings[SoftL]={{A,Y,none}};custom.bindings[N1]={{A,none,none}};custom.bindings[N9]={{A,none,none}};
    require(labels.update(custom,false,4)&&labels.label(A).text=="左软/1/9","all inverse bindings shown in phone order");
    require(labels.label(B).text=="—","unbound physical key explicitly marked");
    for(int p=0;p<PhoneCount;++p)custom.bindings[p]={{A,none,none}};
    require(labels.update(custom,false,4)&&labels.label(A).count==PhoneCount,"nineteen shared physical bindings retained");
    const auto& dense=labels.label(A);std::string joined;
    for(int row=0;row<dense.rows;++row){if(row)joined+='/';joined+=dense.lines[row];}
    require(joined==dense.text&&dense.rows>1&&dense.font>=6,"dense map wraps without dropping any phone legend");
    for(int p=0;p<PhoneCount;++p){const auto emitted=codes(custom,false,1u<<A);require(emitted.count(phone_codes[p])==1,"legend inputs agree with actual key dispatch");}
    Store store;store.global=automatic;const std::string id(64,'a');store.games[id]=custom;
    labels.update(lookup(store,id),false,4);require(labels.label(A).count==PhoneCount,"per-game effective map");store.games.erase(id);labels.update(lookup(store,id),false,4);require(labels.label(A).text=="5","global effective map replaces old per-game legends");
    constexpr size_t pitch=1299,prefix=17;constexpr uint32_t guard=0x12345678;
    std::vector<uint32_t> frame(prefix+pitch*720+prefix,guard);auto* dst=frame.data()+prefix;
    int calls=0;
    const host_menu::Text text=[&](int x,int y,const std::string& value,uint32_t color,int px,int maxw){
        require(value!="L3"&&value!="R3"&&value.find("左摇")==std::string::npos&&value.find("右摇")==std::string::npos,"no stick directions or L3/R3 legends");
        require(x>=0&&y>=0&&x+maxw<=1280&&y+px<=720&&outside(x,y,maxw,px),"all label bounds outside game viewport");
        host_menu::rect(dst,pitch,x,y,maxw,px,color);++calls;
    };
    for(int bezel=4;bezel<=6;++bezel){labels.update(automatic,false,bezel);calls=0;const auto before=allocations;labels.render(dst,pitch,bezel,text);require(calls>=16,"all mapped entity buttons and fixed sticks rendered without physical tags");require(allocations==before,"cached legend render allocates no heap storage");}
    for(int y=40;y<680;++y)for(int x=400;x<880;++x)require(dst[size_t(y)*pitch+x]==guard,"overlay never touches game pixels");
    for(int y=0;y<720;++y)for(size_t x=1280;x<pitch;++x)require(dst[size_t(y)*pitch+x]==guard,"overlay preserves stride padding");
    for(size_t i=0;i<prefix;++i)require(frame[i]==guard&&frame[frame.size()-1-i]==guard,"overlay guards");
    const int before=calls;labels.render(nullptr,pitch,4,text);labels.render(dst,1279,4,text);labels.render(dst,pitch,3,text);require(calls==before,"invalid or old skin overlay is inert");
    int reads=0;host_skin::Cache cache;host_skin::Loader fail=[&](const char*,host_skin::Pixels&,std::string& e){++reads;e="missing";return false;};
    for(int id=4;id<=6;++id){for(int frame=0;frame<10;++frame)require(!cache.select(id,{},fail),"missing new asset uses fallback");}require(reads==3,"missing skins not decoded every frame");
    require(std::remove(argv[1])==0,"settings cleanup");
    std::puts("PASS ControllerSkins: appended IDs, legacy settings, effective inverse maps, full multi-bind legends, fixed sticks, bounded overlays and cache reuse");
}
#endif
