#ifndef J2ME_PIXEL_COMPAT236_H
#define J2ME_PIXEL_COMPAT236_H
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include "raster_primitives.h"
namespace pixel_compat236 {
// Nokia rotates counter-clockwise after reflection. Convert to MIDP's eight
// explicit transforms without inventing unsupported flag combinations.
inline int transform(int flags) {
    int rotation=flags&~(8192|16384);
    if(rotation!=0&&rotation!=90&&rotation!=180&&rotation!=270)return -1;
    int f=((flags&8192)?1:0)|((flags&16384)?2:0);
    static const int map[4][4]={{0,2,1,3},{6,7,4,5},{3,1,2,0},{5,4,7,6}};
    return map[rotation/90][f];
}
inline void inverse(int t,int w,int h,int dx,int dy,int& sx,int& sy) {
    sx=dx;sy=dy;
    switch(t){case 1:sy=h-1-dy;break;case 2:sx=w-1-dx;break;
    case 3:sx=w-1-dx;sy=h-1-dy;break;case 4:sx=dy;sy=dx;break;
    case 5:sx=dy;sy=h-1-dx;break;case 6:sx=w-1-dy;sy=dx;break;
    case 7:sx=w-1-dy;sy=h-1-dx;break;default:break;}
}
// Exact overlap semantics: source is captured before any destination write.
inline bool copy(std::vector<uint8_t>& rgb,int w,int h,int sx,int sy,int cw,int ch,
                 int64_t dx,int64_t dy,raster_primitives::Bounds clip,
                 std::vector<uint8_t>* alpha=nullptr) {
    if(cw<0||ch<0||sx<0||sy<0||int64_t(sx)+cw>w||int64_t(sy)+ch>h||
       w<0||h<0||uint64_t(w)*h*3>rgb.size())return false;
    if(!cw||!ch)return true;
    auto b=raster_primitives::intersect({0,0,w,h},clip);
    b=raster_primitives::intersect(b,{dx,dy,dx+cw,dy+ch});
    if(b.left>=b.right||b.top>=b.bottom)return true;
    const size_t rw=size_t(b.right-b.left),rh=size_t(b.bottom-b.top);
    std::vector<uint8_t> temp(rw*rh*3),at;
    bool a=alpha&&alpha->size()==size_t(w)*h;if(a)at.resize(rw*rh);
    for(size_t y=0;y<rh;++y){size_t from=(size_t(sy+b.top-dy)+y)*w+size_t(sx+b.left-dx);
        std::memcpy(temp.data()+y*rw*3,rgb.data()+from*3,rw*3);
        if(a)std::memcpy(at.data()+y*rw,alpha->data()+from,rw);}
    for(size_t y=0;y<rh;++y){size_t to=(size_t(b.top)+y)*w+size_t(b.left);
        std::memcpy(rgb.data()+to*3,temp.data()+y*rw*3,rw*3);
        if(a)std::memcpy(alpha->data()+to,at.data()+y*rw,rw);}
    return true;
}
}
#endif
