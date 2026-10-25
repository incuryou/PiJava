#include "../src/logical_screen.h"
#include "../src/presentation/layers.h"
#include "../src/p7_present.h"
#include <cassert>
#include <fstream>
#include <iostream>
static uint32_t pixel(const std::vector<uint8_t>& rgb,int w,int x,int y){
    const size_t p=(size_t(y)*w+x)*3;
    return rgb[p]|(uint32_t(rgb[p+1])<<8)|(uint32_t(rgb[p+2])<<16)|0xff000000u;
}
int main(int argc,char** argv){
    for(const std::string s:{"0x320","-1x2","240","640x641","9999999999999999999999999x2","240x320junk","240xx320","16x15"}){int w=91,h=92;assert(!logical_screen::parse(s,w,h)&&w==91&&h==92);}
    struct Case{int sw,sh;};
    // Legacy policy: every logical canvas fills the same fixed 480x640 output.
    for(auto c:{Case{240,320},{176,208},{128,160},{360,360},{360,640},{320,480},{320,240},{640,360},{16,640},{640,16},{640,640}}){
        const auto dims=std::pair<int,int>{c.sw,c.sh};
        int w=0,h=0;assert(logical_screen::parse(std::to_string(dims.first)+"x"+std::to_string(dims.second)+"\r\n",w,h));
        std::vector<uint8_t> rgb(size_t(w)*h*3);for(int y=0;y<h;++y)for(int x=0;x<w;++x){size_t p=(size_t(y)*w+x)*3;rgb[p]=uint8_t(x);rgb[p+1]=uint8_t(y);rgb[p+2]=uint8_t(x^y);}
        auto before=rgb;std::vector<uint32_t> rgba;assert(j2me_present::game_rgba(rgb,w,h,rgba));
        assert(rgba.size()==480*640);
        std::vector<uint32_t> chrome(1280*720,0xff123456),cpu=chrome,gpu;
        p7present::blit_game(cpu.data(),1280,rgb.data(),w,h);assert(j2me_present::compose_reference(chrome,&rgba,gpu));assert(cpu==gpu&&rgb==before);
        const logical_screen::View view(w,h);int sx=-1,sy=-1;
        assert(view.x==0&&view.y==0&&view.w==480&&view.h==640);
        for(int y=0;y<720;++y)for(int x=0;x<1280;++x){
            uint32_t expected=0xff123456;
            if(x>=400&&x<880&&y>=40&&y<680){
                const int vx=x-400,vy=y-40;
                const int ex=int(int64_t(vx)*w/480),ey=int(int64_t(vy)*h/640);
                assert(view.host_point(x,y,sx,sy)&&sx==ex&&sy==ey);
                expected=pixel(rgb,w,ex,ey);
            }
            assert(cpu[size_t(y)*1280+x]==expected);
        }
        assert(view.host_point(400,40,sx,sy)&&sx==0&&sy==0);
        assert(view.host_point(879,679,sx,sy)&&sx==int(int64_t(479)*w/480)&&sy==int(int64_t(639)*h/640));
        assert(!view.host_point(399,40,sx,sy)&&!view.host_point(880,40,sx,sy));
        assert(!view.host_point(400,39,sx,sy)&&!view.host_point(400,680,sx,sy));
        if(w==240&&h==320)for(int y=0;y<320;++y)for(int x=0;x<240;++x){size_t p=(size_t(y)*w+x)*3;uint32_t c=rgb[p]|(uint32_t(rgb[p+1])<<8)|(uint32_t(rgb[p+2])<<16)|0xff000000u;assert(cpu[size_t(40+2*y)*1280+400+2*x]==c);}
    }
    logical_screen::View bad(0,0);int x=0,y=0;assert(!bad.host_point(400,40,x,y));
    for(auto dims:{std::pair<int,int>{176,208},{360,360},{360,640},{640,360}}){
        std::vector<uint8_t> rgb(size_t(dims.first)*dims.second*3,77);
        std::vector<uint32_t> padded(size_t(1301)*720,0xff123456);
        p7present::blit_game(padded.data(),1301,rgb.data(),dims.first,dims.second);
        for(int y=0;y<720;++y)for(int x=1280;x<1301;++x)assert(padded[size_t(y)*1301+x]==0xff123456);
    }
    p7::pool().shutdown();std::cout<<"PASS ScreenPresentChecks: fixed output rectangle, stretched CPU/GPU equality, bounds and inverse mapping\n";
}
