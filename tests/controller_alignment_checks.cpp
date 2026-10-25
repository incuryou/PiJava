#include "../src/host_pro_controller.h"
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char* m){if(!ok){std::fprintf(stderr,"ASSERT %s\n",m);std::exit(1);}}
static host_controller::Ink shared_font(const std::string& s,int px){
    if(s=="+")return {7,7,true};
    return {std::max(1,int(s.size())*px/3),std::max(1,px-1),true};
}
static host_controller::Ink oversized_tag(const std::string& s,int px){if(s=="+")return {100,40,true};return shared_font(s,px);}
static host_controller::Ink oversized_main(const std::string& s,int px){if(s=="#")return {100,100,true};return shared_font(s,px);}
int main(){using namespace host_keymap;using namespace host_controller;
    Config custom;custom.automatic=false;custom.bindings=empty_bindings();custom.bindings[Star][0]=Minus;custom.bindings[Hash][0]=Plus;custom.bindings[N0][0]=R3;
    Legends labels;labels.update(custom,false,0);const auto& plus=host_pro::button_faces[10];FittedInk fitted;
    require(fit_button(labels.label(Plus),plus,shared_font,fitted),"7px high physical + must not suppress valid phone #");
    bool hash=false;for(int n=0;n<fitted.count;++n)hash|=!fitted.runs[n].tag&&fitted.runs[n].text=="#";
    require(hash,"phone # remains visible with shared-font-like + metrics");
    fit_button(labels.label(Plus),plus,oversized_tag,fitted);hash=false;for(int n=0;n<fitted.count;++n)hash|=!fitted.runs[n].tag&&fitted.runs[n].text=="#";
    require(hash,"oversized physical tag cannot clear valid main mapping");
#ifdef HOST_CONTROLLER_ALIGNMENT_REV
    require(fitted.tag_fallback,"unfittable physical tag has explicit visible fallback");
    fit_button(labels.label(Plus),plus,oversized_main,fitted);require(fitted.main_fallback,"unfittable phone glyph has explicit visible fallback");
    host_pro::Preview preview;std::vector<uint32_t> frame(1280*720);int hashes=0;
    const host_menu::CenteredText draw=[&](int,int,const std::string& s,uint32_t,int,int){hashes+=s=="#";};
    preview.render(frame.data(),1280,custom,false,none,[](int,int,const std::string&,uint32_t,int,int){},draw,oversized_tag);
    require(hashes==1,"actual Preview still invokes valid phone # when physical tag cannot fit");
    const int tx=plus.tag.x+plus.tag.w/2,ty=plus.tag.y+plus.tag.h/2;
    require(frame[size_t(ty)*1280+tx]==host_menu::color(0xffcf50),"actual Preview displays font-independent tag fallback marker");
#endif
    std::vector<uint32_t> scratch(1280*720);
    for(int bezel=4;bezel<=6;++bezel){Legends game;game.update(custom,false,bezel);int dashes=0,zeros=0,menus=0;
        const host_menu::CenteredText draw=[&](int,int,const std::string& s,uint32_t,int,int){dashes+=s=="—";zeros+=s=="0";menus+=s=="菜单";};
        game.render(scratch.data(),1280,bezel,[](int,int,const std::string&,uint32_t,int,int){},Context::Game,draw);
        require(dashes==0&&zeros==1&&menus==1,"game unmapped controls are blank while actual R3=0 and reserved menu remain");
        custom.bindings[N0][0]=none;game.update(custom,false,bezel);zeros=0;game.render(scratch.data(),1280,bezel,[](int,int,const std::string&,uint32_t,int,int){},Context::Game,draw);require(zeros==0,"unbound R3 must not pretend to map phone zero");custom.bindings[N0][0]=R3;
    }
    const auto& black=game_layout(6);require(black[2].x+black[2].w/2==313&&black[2].y+black[2].h/2==161,"black game star uses measured image center");
    require(black[10].x+black[10].w/2==965&&black[10].y+black[10].h/2==161,"black game hash uses measured image center");
    std::puts("PASS ControllerAlignment: independent tag/main fitting, visible fallbacks, blank game controls, actual zero binding and measured black star/hash centers");
}
