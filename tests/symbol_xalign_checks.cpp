#include "../src/host_pro_controller.h"
#include "../src/host_font.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
static host_font::Font* font;
static host_controller::Ink measure(const std::string& s,int px){if(font->ready()){const auto m=font->measure(s,px);return {m.w,m.h,m.ok};}return {std::max(1,host_menu::text_advance(s,px)),px,true};}
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"ASSERT %s\n",message);std::exit(1);}}
int main(int argc,char** argv){
    require(argc==4,"font and independent pixel-derived cap centers");host_font::Font raster;font=&raster;
    if(std::string(argv[1])!="none"){std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});require(raster.add_font(bytes.data(),bytes.size()),"trusted real font loads");
        // stb retains pointers to the trusted font bytes throughout rendering.
        static std::vector<uint8_t> retained;retained=std::move(bytes);
    }
    const int minus=std::atoi(argv[2]),plus=std::atoi(argv[3]);std::vector<uint32_t> frame(1280*720),glyph(frame.size());int symbols=0,tags=0;
    host_pro::Preview preview;require(preview.load("romfs/ui/pro-controller.rgba"),"actual RGBA loads");
    const host_menu::CenteredText center=[&](int cx,int cy,const std::string& s,uint32_t,int px,int){
        if(s!="*"&&s!="#"&&s!="-"&&s!="+")return;const bool tag=s=="-"||s=="+";const int expected=s=="*"||s=="-"?minus:plus;
        std::printf("SYMBOL %s center=%d,%d pixel_cap_x=%d\n",s.c_str(),cx,cy,expected);
        require(cx==expected,"actual Preview symbol/tag X differs from flood-filled image cap center");require(cy==(tag?264:244),"symbol/tag Y must remain unchanged");
        if(tag)++tags;else ++symbols;
        if(raster.ready()){std::fill(glyph.begin(),glyph.end(),0);raster.draw_centered(glyph.data(),1280,cx,cy,s,0xffffffffu,px,512);int lo=1280,hi=-1;for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(glyph[size_t(y)*1280+x]&0xffffffu){lo=std::min(lo,x);hi=std::max(hi,x);}require(hi>=lo&&std::abs((lo+hi)/2.0-expected)<=.75,"unclipped real glyph X is not centered on actual cap");}
    };
    preview.render(frame.data(),1280,host_keymap::Config{},false,host_keymap::none,[](int,int,const std::string&,uint32_t,int,int){},center,measure);
    require(symbols==2&&tags==2,"both mapped symbols and exterior tags actually rendered");
    std::puts("PASS SymbolXAlign: real image cap X, actual Preview centers, real glyph ink, unchanged Y and shared exterior-tag axes");
}
