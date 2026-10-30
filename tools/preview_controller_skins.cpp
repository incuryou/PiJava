// Actual split-JPEG loader, native font, dynamic labels and presenter pixels.
// FONT is read locally; it is never copied into the output or skin assets.
#include "../src/controller_skin_labels.h"
#include "../src/host_skin.h"
#include "../src/host_font.h"
#include "../src/presentation/layers.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
static host_font::Font* font;
static int measure(const std::string& s,int px){const auto m=font->measure(s,px);return m.ok?m.advance:host_menu::text_advance(s,px);}
int main(int argc,char** argv){
    if(argc!=3)return 1;
    std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    host_font::Font raster;if(!raster.add_font(bytes.data(),bytes.size()))return 2;font=&raster;
    const std::filesystem::path output(argv[2]);std::filesystem::create_directories(output);
    std::vector<uint8_t> rgb(240*320*3);
    for(int y=0;y<320;++y)for(int x=0;x<240;++x){
        const bool edge=x==0||x==239||y==0||y==319,grid=x%20==0||y%20==0;
        const size_t p=(size_t(y)*240+x)*3;rgb[p]=edge?255:grid?160:uint8_t(x/2);rgb[p+1]=edge?220:grid?180:uint8_t(y/2);rgb[p+2]=edge?0:grid?200:90;
    }
    std::vector<uint32_t> game;if(!j2me_present::game_rgba(rgb,240,320,game))return 3;
    for(int variant=0;variant<5;++variant){
        const int bezel=variant==3?4:variant==4?6:variant+4;const char* names[]={"ivory","blue","black"};
        const auto filename=std::string("romfs/")+std::string(host_skin::path(bezel)).substr(7);
        host_skin::Pixels skin;std::string error;if(!host_skin::load_file(filename.c_str(),skin,error)){std::cerr<<error;return 4;}
        std::vector<uint32_t> chrome(host_skin::pixel_count);host_menu::render_bezel(chrome.data(),1280,bezel,{},skin.data(),skin.size());
        host_keymap::Config config;
        if(variant==3){host_keymap::assign(config,false,host_keymap::SoftL,1,host_keymap::A);host_keymap::assign(config,false,host_keymap::N1,1,host_keymap::A);host_keymap::assign(config,false,host_keymap::N9,1,host_keymap::A);}
        if(variant==4){config.automatic=false;config.bindings=host_keymap::defaults(false);config.bindings[host_keymap::SoftR].fill(host_keymap::none);config.bindings[host_keymap::N0].fill(host_keymap::none);}
        host_controller::Legends labels;labels.update(config,false,bezel,measure);
        const host_menu::Text text=[&](int x,int y,const std::string& s,uint32_t color,int px,int maxw){if(!raster.draw(chrome.data(),1280,x,y,s,color,px,maxw))std::abort();};
        const host_menu::CenteredText centered=[&](int x,int y,const std::string& s,uint32_t color,int px,int maxw){if(!raster.draw_centered(chrome.data(),1280,x,y,s,color,px,maxw))std::abort();};
        labels.render(chrome.data(),1280,bezel,text,host_controller::Context::Game,centered);
        for(int y=40;y<680;++y)for(int x=400;x<880;++x)if(chrome[size_t(y)*1280+x]!=host_menu::color(0))return 5;
        std::vector<uint32_t> frame;if(!j2me_present::compose_reference(chrome,&game,frame))return 6;
        const std::string name=std::string("controller-")+names[bezel-4]+(variant==3?"-custom":variant==4?"-unmapped":"-default");
        std::ofstream ppm(output/(name+".ppm"),std::ios::binary);ppm<<"P6\n1280 720\n255\n";
        for(uint32_t p:frame){const char pixel[]={char(p&255),char((p>>8)&255),char((p>>16)&255)};ppm.write(pixel,3);}if(!ppm)return 7;
    }
    std::puts("PASS four actual controller/font previews with procedural 240x320 test card; viewport untouched by chrome");
}
