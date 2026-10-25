// Catches unreachable filter focus, incorrect direction/wrap, lost old safe
// preferences and filter choices which disappear on restart.
#include <cstdio>
#include <cstdlib>
#if !__has_include("../src/presentation/filters.h")
#include "../src/host_menu.h"
int main(){
    std::vector<uint32_t> frame(1280*720);bool found=false;
    host_menu::Text text=[&](int,int,const std::string& s,uint32_t,int,int){if(s=="滤镜")found=true;};
    host_menu::render_menu(frame.data(),1280,{"game"},0,host_menu::Focus::Bezel,{},"",text);
    if(!found){std::fputs("ASSERT menu must expose independent filter row below bezel\n",stderr);return 1;}return 0;
}
#else
#include "../src/host_menu_settings.h"
#include <fstream>
#include <unistd.h>
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"ASSERT %s\n",message);std::exit(1);}}
static void write(const std::string& p,const std::string& s){std::ofstream f(p,std::ios::binary);f<<s;require(bool(f),"fixture write");}
int main(){
    using namespace host_menu;using display_filter::Filter;
    char tmp[]="/tmp/j2me-filters-XXXXXX";require(mkdtemp(tmp)!=nullptr,"temporary directory");
    const std::string path=std::string(tmp)+"/menu.cfg";
    Settings s;require(s.filter==Filter::Original,"missing setting defaults to original");
    require(option_step(Focus::Bezel,1)==Focus::Filter&&option_step(Focus::Filter,-1)==Focus::Bezel,"filter focus directly below bezel");
    require(option_step(Focus::Filter,1)==Focus::Speed&&option_step(Focus::Speed,1)==Focus::Fps,"remaining options reachable");
    require(option_step(Focus::Fps,1)==Focus::Fps&&option_step(Focus::Bezel,-1)==Focus::Bezel,"option endpoints clamped");
    require(change_filter(s,-1)&&s.filter==Filter::ScanlinesDX,"left wraps from original to ScanlinesDX");
    require(change_filter(s,1)&&s.filter==Filter::Original,"right wraps to original");
    const Filter expected[]={Filter::Bilinear,Filter::Scale2x,Filter::Lcd,Filter::Crt,Filter::ScanlinesDX,Filter::Original};
    for(Filter f:expected){require(change_filter(s,1)&&s.filter==f,"forward cycle order");s.bezel=2;s.show_fps=false;
        require(save_settings(path,s),"menu selection saves");Settings restored;
        require(load_settings(path,restored)&&restored.filter==f&&restored.bezel==2&&!restored.show_fps,"filter and safe preferences survive reload");}
    for(int v:{1,2}){write(path,"J2ME_MENU "+std::to_string(v)+"\nbezel=2\nshow_fps=0\n");s.filter=Filter::Crt;
        require(load_settings(path,s)&&s.filter==Filter::Original&&s.bezel==(v==1?0:2)&&!s.show_fps,"legacy records reset filter and preserve safe settings");}
    for(const std::string value:{"999","-1","bad","999999999999999999999999999",""}){
        write(path,"J2ME_MENU 2\nbezel=3\nshow_fps=0\nfilter="+value+"\n");s=Settings{};s.filter=Filter::Crt;
        require(load_settings(path,s)&&s.bezel==3&&!s.show_fps&&s.filter==Filter::Original,"bad optional filter preserves validated old preferences");}
    s=Settings{};s.filter=static_cast<Filter>(999);require(!save_settings(path,s),"invalid filter save rejected");
    struct Call{std::string text;int y;};std::vector<Call> calls;std::vector<uint32_t> frame(1280*720);
    Text text=[&](int,int y,const std::string& t,uint32_t,int,int){calls.push_back({t,y});};
    s=Settings{};s.filter=Filter::Scale2x;render_menu(frame.data(),1280,{"game"},0,Focus::Filter,s,"",text);
    int bezel=-1,filter=-1,speed=-1,fps=-1;bool selected=false,pane_hint=false,wrong_hint=false;
    for(const auto& c:calls){if(c.text=="遮罩")bezel=c.y;if(c.text=="滤镜")filter=c.y;if(c.text=="速度")speed=c.y;if(c.text=="显示帧数")fps=c.y;if(c.text=="Scale2x")selected=true;if(c.text=="← → 游戏 / 选项")pane_hint=true;if(c.text=="← → 切换滤镜")wrong_hint=true;}
    require(bezel>=0&&bezel<filter&&filter<speed&&speed<fps&&fps<586&&selected,"independent visible filter row and chosen name");
    require(frame[size_t(filter)*1280+914]==color(0xf4dddd),"real filter focus highlight visible");
    require(pane_hint&&!wrong_hint,"filter footer describes pane navigation, not horizontal filter changes");
    require(std::remove(path.c_str())==0&&rmdir(tmp)==0,"cleanup");
    std::puts("PASS FiltersMenuChecks: focus, directional cycle, visible row, persistence and legacy safety");
}
#endif
