#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Host-only, fixed viewport effects. No allocation, source writes or VM state.
namespace display_filter {
enum class Filter : int { Original=0, Bilinear, Scale2x, Lcd, Crt, ScanlinesDX };
constexpr int count=6, width=480, height=640;
inline bool valid(Filter filter){return int(filter)>=0&&int(filter)<count;}
inline Filter normalized(Filter filter){return valid(filter)?filter:Filter::Original;}
inline const char* name(Filter filter){
    switch(normalized(filter)){
    case Filter::Bilinear:return "双线性";
    case Filter::Scale2x:return "Scale2x";
    case Filter::Lcd:return "LCD 网格";
    case Filter::Crt:return "CRT 扫描线";
    case Filter::ScanlinesDX:return "ScanlinesDX";
    default:return "原画";
    }
}
namespace detail {
inline uint32_t rgb(const uint8_t* source,int w,int x,int y){
    const auto* p=source+(size_t(y)*w+x)*3;
    return 0xff000000u|uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16);
}
inline uint32_t gain(uint32_t c,unsigned numerator,unsigned denominator){
    uint32_t out=0xff000000u;
    for(int shift=0;shift<24;shift+=8)
        out|=std::min(255u,(((c>>shift)&255u)*numerator+denominator/2)/denominator)<<shift;
    return out;
}
inline uint32_t bilinear(const uint8_t* source,int w,int h,int x,int y){
    // Center-aligned coordinates: ((output+.5)*source/output_size)-.5.
    int nx=(2*x+1)*w-width,ny=(2*y+1)*h-height;
    int sx=0,sy=0,fx=0,fy=0;
    if(nx>0){sx=nx/(2*width);fx=nx%(2*width);}
    if(ny>0){sy=ny/(2*height);fy=ny%(2*height);}
    if(sx>=w-1){sx=w-1;fx=0;}
    if(sy>=h-1){sy=h-1;fy=0;}
    const int right=std::min(sx+1,w-1),bottom=std::min(sy+1,h-1);
    const uint32_t c[]={rgb(source,w,sx,sy),rgb(source,w,right,sy),rgb(source,w,sx,bottom),rgb(source,w,right,bottom)};
    const unsigned weights[]={unsigned((2*width-fx)*(2*height-fy)),unsigned(fx*(2*height-fy)),unsigned((2*width-fx)*fy),unsigned(fx*fy)};
    constexpr unsigned denominator=4*width*height;
    uint32_t out=0xff000000u;
    for(int shift=0;shift<24;shift+=8){
        unsigned sum=0;for(int i=0;i<4;++i)sum+=((c[i]>>shift)&255u)*weights[i];
        out|=((sum+denominator/2)/denominator)<<shift;
    }
    return out;
}
inline uint32_t scale2x(const uint8_t* source,int w,int h,int x,int y){
    const int vx=int(int64_t(x)*w*2/width),vy=int(int64_t(y)*h*2/height);
    const int sx=vx/2,sy=vy/2;
    const uint32_t b=rgb(source,w,sx,std::max(0,sy-1)),d=rgb(source,w,std::max(0,sx-1),sy),
        e=rgb(source,w,sx,sy),f=rgb(source,w,std::min(w-1,sx+1),sy),hue=rgb(source,w,sx,std::min(h-1,sy+1));
    if(b==hue||d==f)return e;
    if(!(vy&1))return !(vx&1)?(d==b?d:e):(b==f?f:e);
    return !(vx&1)?(d==hue?d:e):(hue==f?f:e);
}
inline const std::array<uint16_t,48*64>& scanlinesdx_gains(){
    // Formula reference: R-YaTian/DraStic_rev_i18n at
    // 090c4ec0e914f415bf13d38ce642d7ef5918178b,
    // universal/assets/shaders/ScanlinesdX.dsd and .dfx (nearest sampling).
    // amount=170, intensity=.5; .6*1.1*.8*(cos(sum)+cos(diff)).
    // Center UVs reduce to exact integer phases modulo 384. The 48x64
    // period needs only 6 KiB; lazy Q12 storage has no heap/OOM path and
    // removes every trigonometric call from subsequent frame rendering.
    static const std::array<uint16_t,48*64> table=[] {
        std::array<uint16_t,48*64> values{};
        for(int y=0;y<64;++y)for(int x=0;x<48;++x){
            const int sum=(136*x+102*y+119)%384;
            const int delta=136*x-102*y+17;
            const int difference=(delta%384+384)%384; // GLSL fract for negatives.
            const double a=(double(sum)/384.0-.5)*3.1415926*.5;
            const double b=(double(difference)/384.0-.5)*3.1415926*.5;
            values[size_t(y)*48+x]=uint16_t(.6*1.1*.8*(std::cos(a)+std::cos(b))*4096.0+.5);
        }
        return values;
    }();
    return table;
}
}
// Callers validate the fixed destination and logical source before rendering.
// Source RGB888 is tightly packed; output pitch is in uint32_t pixels.
inline void render(uint32_t* output,size_t pitch,const uint8_t* source,int w,int h,Filter filter){
    filter=normalized(filter);
    const auto* dx=filter==Filter::ScanlinesDX?detail::scanlinesdx_gains().data():nullptr;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        uint32_t c;
        if(filter==Filter::Bilinear)c=detail::bilinear(source,w,h,x,y);
        else if(filter==Filter::Scale2x)c=detail::scale2x(source,w,h,x,y);
        else {
            const int sx=dx?int(int64_t(2*x+1)*w/(2*width)):int(int64_t(x)*w/width);
            const int sy=dx?int(int64_t(2*y+1)*h/(2*height)):int(int64_t(y)*h/height);
            c=detail::rgb(source,w,sx,sy);
            if(filter==Filter::Lcd){
                uint32_t masked=0xff000000u;
                for(int channel=0;channel<3;++channel){
                    unsigned value=(c>>(channel*8))&255u;
                    if(channel!=x%3)value=value*3/4;
                    if(y&1)value=value*7/8;
                    masked|=value<<(channel*8);
                }
                c=masked;
            } else if(filter==Filter::Crt&&(y&1)){
                // Exact floor brightness for the simple CRT preset.
                c=0xff000000u|((c&255u)*3/4)|((((c>>8)&255u)*3/4)<<8)|((((c>>16)&255u)*3/4)<<16);
            } else if(dx)c=detail::gain(c,dx[size_t(y&63)*48+x%48],4096);
        }
        output[size_t(y)*pitch+x]=c;
    }
}
}
