#pragma once
#include "filters.h"
#include "../logical_screen.h"
#include <vector>

namespace j2me_present { namespace gpu_filter {
using uint=uint32_t;
#define GPU_INLINE inline
#define GPU_PIXEL_SOURCE const uint* pixels,
#define GPU_PIXEL_READ(x,y) pixels[(y)*w+(x)]
#include "gpu_filter_math.inc"
#undef GPU_PIXEL_READ
#undef GPU_PIXEL_SOURCE
#undef GPU_INLINE
constexpr uint32_t max_width=logical_screen::max_axis,max_height=logical_screen::max_axis,header_words=4;
constexpr uint32_t max_pixels=logical_screen::max_pixels;
constexpr uint32_t max_input_bytes=(header_words+max_pixels)*4;
inline bool supported(display_filter::Filter f){
    return display_filter::valid(f)&&f!=display_filter::Filter::Original;
}
// Pure packing accepts 1x1 for contract tests; production still applies the
// Java logical-screen policy (min16, max800, <=409600 pixels) before presentation.
inline bool pack(const std::vector<uint8_t>& rgb,int w,int h,display_filter::Filter f,std::vector<uint32_t>& out,bool adaptive=false){
    if(!logical_screen::valid_extent(w,h)||!supported(f)||rgb.size()!=size_t(w)*h*3)return false;
    out.resize(header_words+size_t(w)*h);out[0]=uint32_t(w);out[1]=uint32_t(h);out[2]=uint32_t(f);out[3]=adaptive?1u:0u;
    for(size_t i=0;i<size_t(w)*h;++i)out[header_words+i]=0xff000000u|uint32_t(rgb[3*i])|(uint32_t(rgb[3*i+1])<<8)|(uint32_t(rgb[3*i+2])<<16);
    return true;
}
struct FrameKey {
    uint64_t epoch=0,frame=0;int w=0,h=0;display_filter::Filter filter=display_filter::Filter::Original;bool adaptive=false;
    bool operator==(const FrameKey& other)const{return epoch==other.epoch&&frame==other.frame&&w==other.w&&h==other.h&&filter==other.filter&&adaptive==other.adaptive;}
};
struct FrameCache {
    FrameKey uploaded;bool valid=false;
    bool needs_upload(const FrameKey& key)const{return !valid||!(uploaded==key);}
    void committed(const FrameKey& key){uploaded=key;valid=true;}
    void invalidate(){valid=false;}
};
}}
