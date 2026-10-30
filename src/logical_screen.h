#pragma once
#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <string>
// Java canvas dimensions are independent of the fixed 480x640 host viewport.
namespace logical_screen {
constexpr int min_axis=16,max_axis=800,max_pixels=640*640;
// Share the same bounded pixel budget with presentation. The low-level
// presenter also accepts thin contract fixtures; Java canvases retain min16.
inline bool valid_extent(int w,int h) {
    return w>=1&&h>=1&&w<=max_axis&&h<=max_axis&&int64_t(w)*h<=max_pixels;
}
inline bool valid(int w,int h) { return w>=min_axis&&h>=min_axis&&valid_extent(w,h); }
inline bool parse(std::string text,int& w,int& h) {
    if(text.size()>32)return false;
    if(!text.empty()&&text.back()=='\n')text.pop_back();
    if(!text.empty()&&text.back()=='\r')text.pop_back();
    size_t x=text.find('x');if(x==std::string::npos||x==0||x+1==text.size())return false;
    int a=0,b=0;
    for(size_t i=0;i<text.size();++i) {
        if(i==x)continue;
        if(text[i]<'0'||text[i]>'9')return false;
        int& n=i<x?a:b;n=n*10+(text[i]-'0');if(n>max_axis)return false;
    }
    if(!valid(a,b))return false;w=a;h=b;return true;
}
// Missing sidecar is the default. Bad/unreadable files never select a size.
inline int sidecar(const std::string& jar,int& w,int& h) {
    w=240;h=320;if(jar.empty())return 0;
    const std::string path=jar+".screen";FILE* f=std::fopen(path.c_str(),"rb");
    if(!f){
        if(errno!=ENOENT)return -1;
        f=std::fopen((path+".previous").c_str(),"rb");
        if(!f)return errno==ENOENT?0:-1;
    }
    char bytes[33];size_t n=std::fread(bytes,1,sizeof(bytes),f);bool ok=!std::ferror(f);ok=std::fclose(f)==0&&ok;
    return ok&&n<=32&&parse(std::string(bytes,n),w,h)?1:-1;
}
struct View {
    // Legacy output policy: fixed 480x640 game viewport. The Java canvas
    // resolution is only the source sampling grid; it never changes the
    // host output rectangle or adds aspect-ratio-dependent letterboxing.
    int x=0,y=0,w=480,h=640,sw=240,sh=320;
    explicit View(int width,int height):sw(width),sh(height) {
        if(!valid(width,height)){w=h=0;return;}
    }
    bool point(int px,int py,int& sx,int& sy) const {
        if(px<0||py<0||px>=w||py>=h)return false;
        sx=int(int64_t(px)*sw/w);sy=int(int64_t(py)*sh/h);return true;
    }
    // No rotation or resolution-dependent fit: all logical canvas sizes are
    // stretched into the same fixed 480x640 host viewport.
    bool host_point(int px,int py,int& sx,int& sy) const {
        if(px<400||py<40||px>=880||py>=680)return false;
        return point(px-400,py-40,sx,sy);
    }
    uint32_t sample(const uint8_t* rgb,int px,int py) const {
        int sx=0,sy=0;if(!point(px,py,sx,sy))return 0xff000000u;
        const auto* p=rgb+(size_t(sy)*sw+sx)*3;
        return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|0xff000000u;
    }
};
}
