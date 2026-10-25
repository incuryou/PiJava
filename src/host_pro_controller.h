#pragma once
#include "pro_controller_layout.h"
#include <cstdio>
#include <vector>
namespace host_pro {
class Preview {
    bool attempted_=false;
    std::vector<uint32_t> pixels_;
    host_controller::Legends labels_;
    std::array<host_controller::FittedInk,16> fitted_;
    host_controller::InkMeasure measure_=nullptr;
    bool fitted_ready_=false;
public:
    bool load(const char* path){
        if(attempted_)return !pixels_.empty();attempted_=true;
        std::FILE* file=path?std::fopen(path,"rb"):nullptr;if(!file)return false;
        pixels_.resize(size_t(image_w)*image_h);
        bool ok=std::fread(pixels_.data(),4,pixels_.size(),file)==pixels_.size()&&std::fgetc(file)==EOF&&!std::ferror(file);
        if(std::fclose(file)!=0)ok=false;
        if(!ok)std::vector<uint32_t>{}.swap(pixels_);
        return ok;
    }
    const host_controller::Legends& labels()const{return labels_;}
    void render(uint32_t* frame,size_t pitch,const host_keymap::Config& config,bool standard,int selected,
                const host_menu::Text& text,const host_menu::CenteredText& center,host_controller::InkMeasure measure=nullptr){
        if(!frame||pitch<1280)return;
        if(pixels_.empty())host_menu::rect(frame,pitch,image_x+35,image_y+70,image_w-70,image_h-95,host_menu::color(0x343a42));
        else for(int y=0;y<image_h;++y)for(int x=0;x<image_w;++x){
            const uint32_t src=pixels_[size_t(y)*image_w+x];const unsigned alpha=src>>24;if(!alpha)continue;
            uint32_t& dst=frame[size_t(image_y+y)*pitch+image_x+x];uint32_t mixed=0xff000000u;
            for(int shift=0;shift<24;shift+=8)mixed|=((((src>>shift)&255)*alpha+((dst>>shift)&255)*(255-alpha)+127)/255)<<shift;
            dst=mixed;
        }
        const bool changed=labels_.update(config,standard,0);
        if(changed||!fitted_ready_||measure_!=measure){
            measure_=measure;fitted_ready_=true;
            for(size_t n=0;n<button_faces.size();++n){fitted_[n]=host_controller::FittedInk{};host_controller::fit_button(labels_.label(button_faces[n].input),button_faces[n],measure,fitted_[n]);}
        }
        for(size_t n=0;n<button_faces.size();++n){const auto& face=button_faces[n];const auto& fitted=fitted_[n];
            for(int row=0;row<fitted.count;++row){const auto& run=fitted.runs[size_t(row)];const int width=run.tag?face.tag.w:face.phone.w;
                if(center)center(run.cx,run.cy,run.text,host_menu::color(0xffffff),run.font,width);
                else if(text)text(run.cx-run.w/2,run.cy-run.h/2,run.text,host_menu::color(0xffffff),run.font,width);
            }
            if(fitted.lock){const int x=fitted.lock_x,y=fitted.lock_y;
                host_menu::rect(frame,pitch,x-2,y,5,3,host_menu::color(0xffffff));
                host_menu::rect(frame,pitch,x-1,y-2,3,1,host_menu::color(0xffffff));
                host_menu::rect(frame,pitch,x-1,y-2,1,2,host_menu::color(0xffffff));host_menu::rect(frame,pitch,x+1,y-2,1,2,host_menu::color(0xffffff));
            }
            // Metrics that cannot fit are reported visibly, never by erasing
            // a different valid legend. These marks need no font glyph.
            const auto warning=[&](const host_controller::Inset& area){const int x=area.x+area.w/2,y=area.y+area.h/2;host_menu::rect(frame,pitch,x-1,y-3,2,4,host_menu::color(0xffcf50));host_menu::rect(frame,pitch,x-1,y+2,2,2,host_menu::color(0xffcf50));};
            if(fitted.main_fallback)warning(face.phone);
            if(fitted.tag_fallback)warning(face.tag);
        }
        if(selected>=host_keymap::SUp&&selected<=host_keymap::SRight)selected=host_controller::Menu;
        if(selected>=host_keymap::RUp&&selected<=host_keymap::RRight)selected=host_keymap::R3;
        for(const auto& a:button_faces)if(a.input==selected){
            const int cx=a.cx,cy=a.cy,rx=int(a.rx)+3,ry=int(a.ry)+3;
            for(int y=cy-ry-2;y<=cy+ry+2;++y)for(int x=cx-rx-2;x<=cx+rx+2;++x){
                const double d=double(x-cx)*(x-cx)/(rx*rx)+double(y-cy)*(y-cy)/(ry*ry);
                if(d>=.86&&d<=1.13)frame[size_t(y)*pitch+x]=host_menu::color(0xd91538);
            }
        }
    }
};
}
