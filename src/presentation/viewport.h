#pragma once
#include <cstdint>
#include "../logical_screen.h"
namespace j2me_present {
constexpr int adaptive_bezel=7;
struct Viewport { int x=400,y=40,w=480,h=640; };
inline bool valid_viewport(const Viewport& v){
    return v.x>=0&&v.y>=0&&v.w>0&&v.h>0&&v.x<=1280-v.w&&v.y<=720-v.h;
}
inline Viewport game_viewport(int w,int h,bool adaptive=false){
    if(!adaptive)return {};
    if(!logical_screen::valid_extent(w,h))return {0,0,0,0};
    int dw=1280,dh=720;
    if(int64_t(w)*720<=int64_t(h)*1280)dw=int(int64_t(w)*720/h);
    else dh=int(int64_t(h)*1280/w);
    if(dw<1)dw=1;if(dh<1)dh=1;
    return {(1280-dw)/2,(720-dh)/2,dw,dh};
}
}
