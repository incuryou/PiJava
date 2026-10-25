// Catches changed source pixels, wrong filter math/edges, CPU/GPU divergence,
// stale selection, overwritten host chrome, stride writes and staging growth.
#include <cstdio>
#include <cstdlib>
#if !__has_include("../src/presentation/filters.h")
#include "../src/presentation/layers.h"
int main() {
    std::vector<uint8_t> rgb(240*320*3,255);std::vector<uint32_t> out;
    bool failed=false;
    if(!j2me_present::game_rgba(rgb,240,320,out)||out[480]!=0xffbfbfbfu){
        std::fputs("ASSERT selected CRT scanline must dim odd output row (legacy presenter stays white)\n",stderr);failed=true;
    }
    if(out.empty()||out[0]!=0xffe6e6e6u){
        std::fputs("ASSERT selected ScanlinesDX must apply upstream diagonal gain at UV center (legacy presenter stays white)\n",stderr);failed=true;
    }
    return failed?1:0;
}
#else
#include "../src/presentation/filters.h"
#include "../src/presentation/layers.h"
#include "../src/p7_present.h"
#include "../src/host_menu.h"
#include <climits>
#include <chrono>
#include <cmath>
#include <vector>
static void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"ASSERT %s\n",message);std::exit(1);}}
using display_filter::Filter;
static void put(std::vector<uint8_t>& rgb,int w,int x,int y,uint8_t r,uint8_t g,uint8_t b){const size_t i=(size_t(y)*w+x)*3;rgb[i]=r;rgb[i+1]=g;rgb[i+2]=b;}
static uint32_t pixel(const std::vector<uint8_t>& rgb,int w,int x,int y){const size_t i=(size_t(y)*w+x)*3;return 0xff000000u|rgb[i]|(uint32_t(rgb[i+1])<<8)|(uint32_t(rgb[i+2])<<16);}
static std::vector<uint32_t> layer(const std::vector<uint8_t>& rgb,int w,int h,Filter f){std::vector<uint32_t> out;require(j2me_present::game_rgba(rgb,w,h,out,f),"valid filter layer");return out;}
int main(){
    std::vector<uint8_t> rgb(240*320*3,0);
    put(rgb,240,1,0,160,0,0);put(rgb,240,0,1,0,80,0);put(rgb,240,1,1,160,80,0);
    auto out=layer(rgb,240,320,Filter::Bilinear);
    require(out[0]==0xff000000u,"bilinear top left clamp");
    require(out[481]==0xff001428u,"bilinear center-aligned quarter weights");
    require(out[2]==0xff000078u,"bilinear horizontal three-quarter weight");
    std::fill(rgb.begin(),rgb.end(),0);
    put(rgb,240,10,9,200,0,0);put(rgb,240,9,10,200,0,0);
    put(rgb,240,11,10,0,180,0);put(rgb,240,10,11,0,180,0);
    out=layer(rgb,240,320,Filter::Scale2x);
    require(out[20*480+20]==0xff0000c8u,"Scale2x top left diagonal extends matching neighbors");
    require(out[20*480+21]==0xff000000u&&out[21*480+20]==0xff000000u,"Scale2x unrelated quadrants retain center");
    require(out[21*480+21]==0xff00b400u,"Scale2x bottom right diagonal");
    std::fill(rgb.begin(),rgb.end(),255);
    out=layer(rgb,240,320,Filter::Lcd);
    require(out[0]==0xffbfbfffu&&out[1]==0xffbfffbfu&&out[2]==0xffffbfbfu,"LCD RGB mask phases");
    require(out[480]==0xffa7a7dfu,"LCD horizontal grid line");
    out=layer(rgb,240,320,Filter::Crt);
    require(out[0]==0xffffffffu&&out[480]==0xffbfbfbfu&&out[960]==0xffffffffu,"CRT alternating scanline brightness");
    out=layer(rgb,240,320,Filter::ScanlinesDX);
    require(out[0]==0xffe6e6e6u&&out[1]==0xffffffffu,"ScanlinesDX literal dark pixel and bright clamp");
    require(out[24*480+15]==0xffc1c1c1u,"ScanlinesDX diagonal dark extreme");
    for(size_t i=0;i<rgb.size();i+=3){rgb[i]=17;rgb[i+1]=101;rgb[i+2]=231;}
    out=layer(rgb,240,320,Filter::ScanlinesDX);
    require(out[0]==0xffd15b0fu&&out[1]==0xffee6812u&&out[480]==0xffe86511u,"ScanlinesDX literal channels include negative difference phase");
    // Independent direct upstream formula: positive and negative fract, center
    // UVs, RGB clamping and opaque host alpha. Q12 gain permits at most 1 LSB.
    const auto fract=[](double x){return x-std::floor(x);};
    for(int y=0;y<64;++y)for(int x=0;x<48;++x){
        const double px=((double(x)+.5)/480.0+1.0)*170.0;
        const double py=((double(y)+.5)/640.0+1.0)*170.0;
        const double gain=.6*1.1*.8*(std::cos((fract(px+py)-.5)*3.1415926*.5)+std::cos((fract(px-py)-.5)*3.1415926*.5));
        const uint32_t got=out[size_t(y)*480+x];
        require((got>>24)==255,"ScanlinesDX keeps opaque host alpha");
        const int channels[]={17,101,231};
        for(int c=0;c<3;++c){const int want=std::min(255,int(channels[c]*gain+.5));require(std::abs(int((got>>(8*c))&255)-want)<=1,"ScanlinesDX matches independent floating shader reference within 1 LSB");}
        require(got==out[size_t(y+64)*480+x+48],"ScanlinesDX exact 48 by 64 phase period");
    }
    const Filter filters[]={Filter::Original,Filter::Bilinear,Filter::Scale2x,Filter::Lcd,Filter::Crt,Filter::ScanlinesDX};
    for(auto dims:{std::pair<int,int>{240,320},{176,208},{360,360},{640,16},{16,640},{16,16},{640,640}}){
        const int w=dims.first,h=dims.second;
        rgb.resize(size_t(w)*h*3);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)put(rgb,w,x,y,uint8_t(x*17),uint8_t(y*13),uint8_t(x^y));
        const auto source=rgb;
        std::vector<uint32_t> gpu,chrome(1280*720,0xff123456u),composed;
        constexpr size_t pitch=1301,prefix=23;constexpr uint32_t guard=0x12345678u;
        std::vector<uint32_t> frame(prefix+pitch*720+prefix,guard);
        for(Filter filter:filters){
            for(int y=0;y<720;++y)std::fill_n(frame.data()+prefix+size_t(y)*pitch,1280,0xff123456u);
            require(j2me_present::game_rgba(rgb,w,h,gpu,filter),"all logical sizes accepted");
            const auto* storage=gpu.data();const auto capacity=gpu.capacity();
            require(j2me_present::game_rgba(rgb,w,h,gpu,filter)&&gpu.data()==storage&&gpu.capacity()==capacity,"GPU staging storage reused");
            p7present::blit_game(frame.data()+prefix,pitch,rgb.data(),w,h,filter);
            require(j2me_present::compose_reference(chrome,&gpu,composed),"compose real game layer");
            for(int y=0;y<720;++y)for(size_t x=0;x<pitch;++x){
                const auto got=frame[prefix+size_t(y)*pitch+x];
                require(got==(x<1280?composed[size_t(y)*1280+x]:guard),"CPU/GPU equality and pitch guards");
                if(x<1280&&(x<400||x>=880||y<40||y>=680))require(got==0xff123456u,"host chrome outside viewport unchanged");
                if(filter==Filter::Original&&x>=400&&x<880&&y>=40&&y<680)
                    require(got==pixel(rgb,w,int((x-400)*w/480),int(int64_t(y-40)*h/640)),"original equals legacy nearest pixels");
            }
            for(size_t i=0;i<prefix;++i)require(frame[i]==guard&&frame[frame.size()-1-i]==guard,"frame guards unchanged");
            require(rgb==source,"Java RGB source immutable");
        }
        const auto original=layer(rgb,w,h,Filter::Original);
        require(layer(rgb,w,h,static_cast<Filter>(-1))==original&&layer(rgb,w,h,static_cast<Filter>(INT_MAX))==original,"invalid filter safely selects original");
        require(layer(rgb,w,h,Filter::Crt)!=original,"fixture observes selected filter");
        require(layer(rgb,w,h,Filter::Original)==original,"switching back removes prior filter");
        std::vector<uint32_t> bad(7,guard);const auto saved=bad;
        auto truncated=rgb;truncated.pop_back();
        require(!j2me_present::game_rgba(truncated,w,h,bad,Filter::Crt)&&bad==saved,"truncated buffer preserves staging");
        require(!j2me_present::game_rgba(rgb,1,1,bad,Filter::Crt)&&bad==saved,"invalid tiny logical dimensions rejected");
        auto frame_before=frame;
        p7present::blit_game(frame.data()+prefix,1279,rgb.data(),w,h,Filter::Crt);
        p7present::blit_game(frame.data()+prefix,pitch,nullptr,w,h,Filter::Crt);
        p7present::blit_game(frame.data()+prefix,pitch,rgb.data(),1,1,Filter::Crt);
        p7present::blit_game(nullptr,pitch,rgb.data(),w,h,Filter::Crt);
        require(frame==frame_before,"invalid CPU inputs leave destination untouched");
        // Real host overlay functions run after the game, including shapes which
        // overlap it; their pixels must be independent of the game filter.
        auto a=chrome,b=chrome;
        p7present::blit_game(a.data(),1280,rgb.data(),w,h,Filter::Original);
        p7present::blit_game(b.data(),1280,rgb.data(),w,h,Filter::Crt);
        host_menu::render_exit_sheet(a.data(),1280,{}, {}, "");
        host_menu::render_exit_sheet(b.data(),1280,{}, {}, "");
        require(a[227*1280+480]==b[227*1280+480],"exit overlay drawn after filter unchanged");
    }
    // Default and split presenter retain the old exact 2x original output.
    rgb.assign(240*320*3,77);std::vector<uint32_t> a(1280*720,0),b=a;
    p7present::blit_game(a.data(),1280,rgb.data(),240,320);
    require(p7::pool().start(),"present workers start");p7present::g_enabled=true;
    for(int n=0;n<6;++n)p7present::blit_game(b.data(),1280,rgb.data(),240,320,Filter::Original);
    require(a==b,"original worker and serial outputs match");p7::pool().shutdown();
    // Observational host timing only: no hardware claim or pass threshold.
    std::vector<uint32_t> staging;
    for(Filter filter:filters){
        j2me_present::game_rgba(rgb,240,320,staging,filter);
        const auto start=std::chrono::steady_clock::now();
        for(int n=0;n<12;++n)require(j2me_present::game_rgba(rgb,240,320,staging,filter),"benchmark frame");
        const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        std::printf("FILTER_BENCH host_only filter=%d frames=12 total_us=%lld us_per_frame=%.2f\n",int(filter),static_cast<long long>(us),double(us)/12.0);
    }
    std::puts("PASS FiltersChecks: literal pixels, dimensions, CPU/GPU, immutability, overlays, bounds and reuse");
}
#endif
