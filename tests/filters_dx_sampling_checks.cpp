// Regression: ScanlinesdX's GL_NEAREST texture reads use center UVs. Legacy
// View sampling must remain untouched for Original/LCD/CRT presentation.
#include "../src/presentation/layers.h"
#include "../src/p7_present.h"
#include <cmath>
#include <cstdio>
#include <vector>
static unsigned channel(int x,int y,int c){
    return c==0?unsigned((x*17+y*7)&255):c==1?unsigned((x*5+y*11)&255):unsigned(((x^y)*13)&255);
}
static uint32_t reference(int w,int h,int x,int y){
    // Independent direct texture sampling and the pinned shader formula.
    const double u=(double(x)+.5)/480.0,v=(double(y)+.5)/640.0;
    const int sx=int(std::floor(u*w)),sy=int(std::floor(v*h));
    const double px=(u+1)*170,py=(v+1)*170;
    const auto fract=[](double value){return value-std::floor(value);};
    const double gain=.6*1.1*.8*(std::cos((fract(px+py)-.5)*3.1415926*.5)+std::cos((fract(px-py)-.5)*3.1415926*.5));
    uint32_t out=0xff000000u;
    for(int c=0;c<3;++c)out|=uint32_t(std::min(255,int(channel(sx,sy,c)*gain+.5)))<<(8*c);
    return out;
}
static bool close(uint32_t actual,uint32_t expected){
    if((actual>>24)!=255)return false;
    for(int c=0;c<3;++c)if(std::abs(int((actual>>(8*c))&255)-int((expected>>(8*c))&255))>1)return false;
    return true;
}
int main(){
    int failures=0;
    for(auto dims:{std::pair<int,int>{176,208},{640,640},{640,16},{16,640},{240,320}}){
        const int w=dims.first,h=dims.second;
        std::vector<uint8_t> rgb(size_t(w)*h*3);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c)rgb[(size_t(y)*w+x)*3+c]=uint8_t(channel(x,y,c));
        const auto before=rgb;std::vector<uint32_t> gpu;
        if(!j2me_present::game_rgba(rgb,w,h,gpu,display_filter::Filter::ScanlinesDX)){
            std::fputs("ASSERT valid ScanlinesDX sampling layer\n",stderr);return 1;
        }
        for(auto point:{std::pair<int,int>{5,3},{1,0},{0,0},{479,639},{478,638},{239,319},{17,39},{100,511}}){
            const int x=point.first,y=point.second;const auto got=gpu[size_t(y)*480+x],want=reference(w,h,x,y);
            if(!close(got,want)){
                std::fprintf(stderr,"ASSERT ScanlinesDX center-UV source cell logical=%dx%d output=%d,%d expected=%08x actual=%08x\n",w,h,x,y,want,got);++failures;
            }
        }
        std::vector<uint32_t> cpu(1280*720,0xff123456u);
        p7present::blit_game(cpu.data(),1280,rgb.data(),w,h,display_filter::Filter::ScanlinesDX);
        for(int y=0;y<640;++y)for(int x=0;x<480;++x)if(cpu[size_t(y+40)*1280+x+400]!=gpu[size_t(y)*480+x]){
            std::fputs("ASSERT ScanlinesDX CPU/GPU sampling match\n",stderr);return 1;
        }
        if(rgb!=before){std::fputs("ASSERT ScanlinesDX source remains immutable\n",stderr);return 1;}
    }
    if(failures)return 1;
    std::puts("PASS FiltersDXSampling: independent center-UV reference, nonuniform 176x208/640 and border fixtures, CPU/GPU equality");
}
