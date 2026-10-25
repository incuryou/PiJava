#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "../logical_screen.h"
#include "filters.h"
// Transfer boundary: immutable, tightly packed host pixels. Neither presenter
// owns Java objects nor writes to the Java-visible RGB framebuffer.
namespace j2me_present {
constexpr int output_w=1280,output_h=720,game_w=480,game_h=640,game_x=400,game_y=40,scale=1;
constexpr size_t canvas_pixels=size_t(output_w)*output_h;
inline bool game_rgba(const std::vector<uint8_t>& rgb,int w,int h,std::vector<uint32_t>& out,display_filter::Filter filter=display_filter::Filter::Original){
    if(!logical_screen::valid(w,h)||rgb.size()!=size_t(w)*h*3)return false;
    out.resize(size_t(game_w)*game_h);
    if(display_filter::normalized(filter)!=display_filter::Filter::Original){
        display_filter::render(out.data(),game_w,rgb.data(),w,h,filter);return true;
    }
    if(w==240&&h==320) {
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){
            const size_t i=(size_t(y)*w+x)*3,p=size_t(y*2)*game_w+x*2;
            const uint32_t c=uint32_t(rgb[i])|(uint32_t(rgb[i+1])<<8)|(uint32_t(rgb[i+2])<<16)|0xff000000u;
            out[p]=out[p+1]=out[p+game_w]=out[p+game_w+1]=c;
        }
    } else {
        const logical_screen::View view(w,h);
        for(int y=0;y<game_h;++y)for(int x=0;x<game_w;++x)out[size_t(y)*game_w+x]=view.sample(rgb.data(),x,y);
    }
    return true;
}
inline bool compose_reference(const std::vector<uint32_t>& chrome,const std::vector<uint32_t>* game,std::vector<uint32_t>& out){
    if(chrome.size()!=canvas_pixels||(game&&game->size()!=size_t(game_w)*game_h))return false;
    out=chrome;if(!game)return true;
    for(int y=0;y<game_h;y++)for(int x=0;x<game_w;x++){
        out[size_t(game_y+y)*output_w+game_x+x]=(*game)[size_t(y)*game_w+x];
    }
    return true;
}
// Per-frame-slot version, not a single global 'already uploaded' bit: different
// swapchain slots can hold different generations of the phone/menu/FPS layer.
struct LayerGeneration {
    uint64_t uploaded=0;bool valid=false;
    bool needs_upload(uint64_t current)const{return !valid||uploaded!=current;}
    void committed(uint64_t current){uploaded=current;valid=true;}
};
}
