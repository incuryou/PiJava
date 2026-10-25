// Native UI preview harness. Usage: menu_preview OUTPUT_DIR FONT_FILE PHONE_RGBA
// Screenshots call the production menu renderers and the production font rasterizer.
#include "../src/host_keymap_ui.h"
#include "../src/host_font.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>

int main(int argc,char** argv) {
    assert(argc==4);
    const std::filesystem::path destination(argv[1]);std::filesystem::create_directories(destination);
    std::ifstream fontfile(argv[2],std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(fontfile)),{});
    host_font::Font font;assert(font.add_font(bytes.data(),bytes.size()));
    constexpr size_t pitch=1297,guard=37;constexpr uint32_t sentinel=0x12345678;
    std::vector<uint32_t> pixels(guard+pitch*720+guard,sentinel);auto* frame=pixels.data()+guard;
    struct Call {int x,y;std::string value;uint32_t color;int size,maxw;};std::vector<Call> calls;
    const host_menu::Text text=[&](int x,int y,const std::string& value,uint32_t color,int size,int maxw) {
        assert(x>=0 && y>=0 && x+maxw<=1280 && y+size<=720);
        calls.push_back({x,y,value,color,size,maxw});font.draw(frame,pitch,x,y,value,color,size,maxw);
    };
    const host_menu::CenteredText centered=[&](int x,int y,const std::string& value,uint32_t color,int size,int maxw) {
        font.draw_centered(frame,pitch,x,y,value,color,size,maxw);
    };
    const auto advance=[&](const std::string& value,int size){return font.measure(value,size).advance;};
    const auto right=[&](const std::string& value,int size){auto m=font.measure(value,size);return m.x+m.w;};
    const auto dump=[&](const char* name) {
        std::ofstream out(destination/(std::string(name)+".ppm"),std::ios::binary);assert(out);
        out<<"P6\n1280 720\n255\n";
        for(int y=0;y<720;++y)for(int x=0;x<1280;++x) {
            uint32_t p=frame[size_t(y)*pitch+x];char rgb[]={char(p&255),char((p>>8)&255),char((p>>16)&255)};out.write(rgb,3);
        }
        for(size_t i=0;i<guard;++i)assert(pixels[i]==sentinel && pixels[guard+pitch*720+i]==sentinel);
        for(int y=0;y<720;++y)for(size_t x=1280;x<pitch;++x)assert(frame[y*pitch+x]==sentinel);
    };
    std::vector<std::string> labels,details;std::vector<uint8_t> saved(24,0);
    for(int i=1;i<=24;++i){labels.push_back("游戏"+std::to_string(i));details.push_back(labels.back()+" · 版本 1.0.3 · 1191 KB");}
    saved[23]=1;host_menu::Settings settings;
    const auto launcher=[&](host_menu::Focus focus=host_menu::Focus::Games,const std::string& status="",bool error=false,bool neutral=true) {
        calls.clear();host_menu::render_menu(frame,pitch,labels,23,focus,settings,status,text,centered,"原生",saved,advance,details,0,nullptr,error,neutral);
    };
    launcher();dump("launcher-metadata");
    assert(frame[59*pitch+1125]==host_menu::color(0xffffff)); // + center, not an A-shaped ring
    assert(frame[53*pitch+1125]==host_menu::color(0xffffff) && frame[59*pitch+1131]==host_menu::color(0xffffff));
    bool metadata=false;for(const auto& c:calls)if(c.value==details[23])metadata=c.y==586 && c.size==20;
    assert(metadata);
    launcher(host_menu::Focus::Bezel);dump("launcher-options-bezel");
    settings.filter=display_filter::Filter::ScanlinesDX;
    launcher(host_menu::Focus::Filter);dump("launcher-options-filter-scanlinesdx");
    settings.filter=display_filter::Filter::Original;
    launcher(host_menu::Focus::Speed);dump("launcher-options-speed");
    launcher(host_menu::Focus::Fps);dump("launcher-options-fps");
    launcher();menu_actions::Deletion deletion;assert(deletion.begin("/switch/j2me/games/game24.jar",labels[23]));
    host_menu::render_delete(frame,pitch,deletion,text,centered);dump("launcher-delete-options-game");
    deletion.selection=1;launcher();host_menu::render_delete(frame,pitch,deletion,text,centered);dump("launcher-delete-options-instant");
    deletion.selection=0;deletion.confirming=true;launcher();host_menu::render_delete(frame,pitch,deletion,text,centered);dump("launcher-delete-confirm-game");
    deletion.selection=1;launcher();host_menu::render_delete(frame,pitch,deletion,text,centered);dump("launcher-delete-confirm-instant");
    launcher(host_menu::Focus::Games,"游戏已删除",false,false);dump("launcher-delete-success");
    launcher(host_menu::Focus::Games,"删除游戏失败",true,false);dump("launcher-delete-failure");
    launcher(host_menu::Focus::Games,"没有即时存档",false,true);dump("launcher-delete-absent");
    host_keymap::Editor editor;host_keymap::Store store;editor.begin(store,std::string(64,'a'),"装甲特战队",false,false);
    const auto phone=host_keymap::load_phone(argv[3]);assert(phone.size()==227*518);
    host_keymap::render(frame,pitch,editor,phone,text,centered,right);dump("l3-save-exit");
    editor.save_note="已保存即时存档";editor.save_note_neutral=false;
    host_keymap::render(frame,pitch,editor,phone,text,centered,right);dump("l3-save-success");
    editor.save_selection=2;editor.pending=host_keymap::Pending::Exit;
    host_keymap::render(frame,pitch,editor,phone,text,centered,right);dump("l3-exit-confirm");
    editor.save_selection=1;editor.pending=host_keymap::Pending::Delete;
    host_keymap::render(frame,pitch,editor,phone,text,centered,right);dump("l3-delete-confirm");
    editor.pending=host_keymap::Pending::None;
    editor.page=host_keymap::Page::Keymap;host_keymap::render(frame,pitch,editor,phone,text,centered,right);dump("l3-keymap");
    std::puts("PASS native menu previews: selected game24 version 1.0.3 / 1191 KB, launcher option focus, delete choices and confirmations, success/error/neutral status, both L3 pages, font rendering and pixel guards");
}
