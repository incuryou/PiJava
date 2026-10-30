#include "../src/host_keymap_ui.h"
#include "../src/host_font.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
static host_font::Font* preview_font;
static host_controller::Ink measure_ink(const std::string& text,int size){const auto m=preview_font->measure(text,size);return {m.w,m.h,m.ok};}
int main(int argc,char** argv){
    if(argc!=3)return 1;
    std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    host_font::Font font;if(!font.add_font(bytes.data(),bytes.size()))return 2;preview_font=&font;
    host_pro::Preview pro;if(!pro.load("romfs/ui/pro-controller.rgba"))return 4;
    std::filesystem::path output(argv[2]);std::filesystem::create_directories(output);
    std::vector<uint32_t> frame(1280*720);
    const host_menu::Text text=[&](int x,int y,const std::string& s,uint32_t c,int px,int maxw){font.draw(frame.data(),1280,x,y,s,c,px,maxw);};
    const host_menu::CenteredText center=[&](int x,int y,const std::string& s,uint32_t c,int px,int maxw){font.draw_centered(frame.data(),1280,x,y,s,c,px,maxw);};
    const auto right=[&](const std::string& s,int px){const auto m=font.measure(s,px);return m.x+m.w;};
    for(int variant=0;variant<5;++variant){
        host_keymap::Store store;host_keymap::Editor e;e.begin(store,std::string(64,'a'),"示例游戏",false,false);e.page=host_keymap::Page::Keymap;e.phase=host_keymap::Phase::Slot;e.phone=host_keymap::N5;
        if(variant==1){host_keymap::assign(e.draft,false,host_keymap::N5,0,host_keymap::X);host_keymap::assign(e.draft,false,host_keymap::N1,1,host_keymap::X);host_keymap::assign(e.draft,false,host_keymap::N9,1,host_keymap::X);e.note="四绑定草稿，按 + 保存";}
        if(variant==2){e.phase=host_keymap::Phase::Choice;e.choice=host_keymap::choice_for_input(host_keymap::RUp);}
        if(variant==3){e.draft.automatic=false;e.draft.bindings=host_keymap::empty_bindings();}
        if(variant==4){e.phone=host_keymap::Hash;e.slot=0;}
        host_keymap::render(frame.data(),1280,e,text,center,right,&pro,measure_ink);
        const char* names[]={"keymap-default","keymap-draft","keymap-choice","keymap-empty","keymap-hash"};std::ofstream ppm(output/(std::string(names[variant])+".ppm"),std::ios::binary);ppm<<"P6\n1280 720\n255\n";
        for(uint32_t p:frame){const char rgb[]={char(p&255),char((p>>8)&255),char((p>>16)&255)};ppm.write(rgb,3);}if(!ppm)return 5;
    }
    std::puts("PASS real font Pro editor default/draft/Choice previews; trusted font read only");
}
