#include "../src/controller_skin_labels.h"
#include "../src/host_keymap_ui.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
static void require(bool ok,const char* m){if(!ok){std::fprintf(stderr,"ASSERT %s\n",m);std::exit(1);}}
int main(int argc,char** argv){
    using namespace host_keymap;require(argc==2,"isolated settings fixture");
    host_controller::Legends labels;labels.update(Config{},false,4);
    std::vector<uint32_t> frame(1297*720,0x12345678);int physical=0;
    const host_menu::Text capture=[&](int,int,const std::string& s,uint32_t,int px,int){if(px==11&&(s=="A"||s=="ZL"||s=="+"))++physical;};
    labels.render(frame.data(),1297,4,capture);
    require(physical==0,"game Pro overlay contains mapped phone legends only, without physical tags");
    require(host_menu::Settings{}.bezel==4,"new settings default to ivory Pro");
#if __has_include("../src/host_pro_controller.h")
    int id=4;const int choices[]={5,6,1,2,3,0,4};for(int want:choices){id=host_menu::next_bezel(id);require(id==want,"bezel A-cycle follows display order while preserving stored IDs");}
    for(int old=0;old<7;++old){std::ofstream f(argv[1],std::ios::binary);f<<"J2ME_MENU 2\nbezel="<<old<<"\nshow_fps=0\n";f.close();host_menu::Settings loaded;require(host_menu::load_settings(argv[1],loaded)&&loaded.bezel==old,"valid saved bezel ID is never forced to new default");}
    for(int bezel=4;bezel<=6;++bezel){const auto& a=host_controller::game_layout(bezel);
        for(auto pair:{std::pair<int,int>{0,8},{1,9}}){const auto& l=a[pair.first];const auto& r=a[pair.second];require(2*l.x+l.w+2*r.x+r.w==2560&&l.y==r.y&&l.h==r.h,"game shoulder text rectangles are mirrored and vertically centered");}}
    physical=0;labels.render(frame.data(),1297,4,capture,host_controller::Context::Editor);require(physical>=3,"editor context retains original physical tags independently");
    Editor e;Store store;e.begin(store,std::string(64,'a'),"fixture",false,false);e.page=Page::Keymap;e.phone=N5;e.phase=Phase::Slot;
    host_pro::Preview preview;
    require(preview.load("romfs/ui/pro-controller.rgba"),"bounded RGBA Pro asset loads successfully");
    std::vector<std::pair<std::string,int>> calls;
    const host_menu::Text text=[&](int x,int y,const std::string& s,uint32_t,int px,int maxw){require(x>=0&&y>=0&&x+maxw<=1280&&y+px<=720,"new page text clipped to host bounds");calls.push_back({s,x});};
    const host_menu::CenteredText center=[&](int x,int y,const std::string& s,uint32_t,int px,int maxw){require(x-maxw/2>=0&&x+maxw/2<=1280&&y-px/2>=0&&y+px/2<=720,"centered text bounded");calls.push_back({s,x});};
    host_keymap::render(frame.data(),1297,e,{},text,center,{},&preview);
    require(preview.labels().label(A).text=="5","controller preview reads initial editor draft");
    assign(e.draft,false,N5,0,B);host_keymap::render(frame.data(),1297,e,{},text,center,{},&preview);
    require(preview.labels().label(A).text=="—"&&preview.labels().label(B).text=="右软/5","draft confirmation immediately changes inverse controller preview before save");
    int slots=0,first=-1;for(auto c:calls)if(c.first=="映射 1"||c.first=="映射 2"||c.first=="映射 3"){if(first<0)first=c.second;else require(c.second>=first,"slot headers progress horizontally");++slots;}
    require(slots>=3,"all three horizontal slots rendered");
    for(int y=0;y<720;++y)for(int x=1280;x<1297;++x)require(frame[size_t(y)*1297+x]==0x12345678,"new layout preserves stride padding");
    std::remove(argv[1]);
#endif
    std::puts("PASS KeymapProUI: game/editor contexts, symmetric shoulders, stable catalog IDs, live draft and horizontal slots");
}
