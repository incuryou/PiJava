#ifndef J2MEVM_RASTER_PRIMITIVES_H
#define J2MEVM_RASTER_PRIMITIVES_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// CPU-only rasterization. Work is limited to visible destination pixels even
// when a MIDlet passes enormous/offscreen coordinates. Geometry uses widened
// coordinates; clipping never changes a shape's rasterization or dotted phase.
namespace raster_primitives {
struct Bounds { int64_t left, top, right, bottom; };
inline Bounds intersect(Bounds a, Bounds b) {
    return {std::max(a.left,b.left),std::max(a.top,b.top),std::min(a.right,b.right),std::min(a.bottom,b.bottom)};
}
struct Surface {
    std::vector<uint8_t>& pixels; int w, h; Bounds clip;
    // Optional straight-alpha target, used by mutable Nokia images. Existing
    // opaque surfaces do not allocate an alpha plane.
    std::vector<uint8_t>* alpha=nullptr;
    unsigned opacity=255;

    Bounds bounds() const { return intersect({0,0,w,h},clip); }
    void put(int64_t x,int64_t y,uint32_t rgb) {
        auto b=bounds();
        if(x<b.left||x>=b.right||y<b.top||y>=b.bottom) return;
        size_t i=(size_t(y)*w+size_t(x))*3;
        if(i+2>=pixels.size()) return;
        unsigned sa=opacity;
        if(!sa)return;
        size_t pi=i/3;
        unsigned da=(alpha&&pi<alpha->size())?(*alpha)[pi]:255;
        if(sa==255) {
            pixels[i]=uint8_t(rgb>>16);pixels[i+1]=uint8_t(rgb>>8);pixels[i+2]=uint8_t(rgb);
            if(alpha&&pi<alpha->size())(*alpha)[pi]=255;
            return;
        }
        const unsigned denom=sa*255+da*(255-sa);
        if(!denom)return;
        const unsigned channels[3]={(rgb>>16)&255,(rgb>>8)&255,rgb&255};
        for(unsigned c=0;c<3;c++)pixels[i+c]=uint8_t((channels[c]*sa*255+unsigned(pixels[i+c])*da*(255-sa)+denom/2)/denom);
        if(alpha&&pi<alpha->size())(*alpha)[pi]=uint8_t((denom+127)/255);
    }
};
inline bool array_range(size_t size,int offset,int scan,int w,int h) {
    if(w<=0||h<=0) return true;
    int64_t last=int64_t(h-1)*scan;
    int64_t lo=int64_t(offset)+std::min(int64_t(0),last);
    int64_t hi=int64_t(offset)+std::max(int64_t(0),last)+w-1;
    return lo>=0 && uint64_t(hi)<size;
}
inline uint32_t source_over(uint32_t source,uint32_t destination,bool alpha) {
    unsigned a=alpha ? source>>24 : 255;
    if(a==255) return source&0xffffff;
    if(!a) return destination&0xffffff;
    uint32_t rgb=0;
    for(int shift=0;shift<=16;shift+=8) {
        unsigned value=((source>>shift)&255)*a+((destination>>shift)&255)*(255-a);
        rgb|=(value/255)<<shift;
    }
    return rgb;
}
template<class Predicate> inline void shape(Surface& s,int64_t x,int64_t y,int64_t w,int64_t h,
                                            uint32_t color,Predicate contains,bool outline,bool dotted=false) {
    if(w<=0||h<=0) return;
    auto b=intersect(s.bounds(),{x,y,x+w,y+h});
    for(int64_t yy=b.top;yy<b.bottom;++yy) for(int64_t xx=b.left;xx<b.right;++xx) {
        double u=double(xx-x)+0.5,v=double(yy-y)+0.5;
        if(!contains(u,v)) continue;
        if(outline && contains(u-1,v)&&contains(u+1,v)&&contains(u,v-1)&&contains(u,v+1)) continue;
        if(outline && dotted && ((xx-x+yy-y)&1)) continue;
        s.put(xx,yy,color);
    }
}
inline void rect(Surface& s,int64_t x,int64_t y,int w,int h,uint32_t color,bool outline,bool dotted=false) {
    if(w<0||h<0||(!outline && (!w||!h))) return;
    int64_t ww=int64_t(w)+(outline?1:0),hh=int64_t(h)+(outline?1:0);
    auto b=intersect(s.bounds(),{x,y,x+ww,y+hh});
    auto put=[&](int64_t xx,int64_t yy){if(!outline||!dotted||!((xx-x+yy-y)&1))s.put(xx,yy,color);};
    for(int64_t yy=b.top;yy<b.bottom;++yy) {
        if(!outline||yy==y||yy==y+hh-1) {
            for(int64_t xx=b.left;xx<b.right;++xx)put(xx,yy);
        } else {
            if(x>=b.left&&x<b.right)put(x,yy);
            if(ww>1&&x+ww-1>=b.left&&x+ww-1<b.right)put(x+ww-1,yy);
        }
    }
}
inline double normalized_angle(double angle) {
    angle=std::fmod(angle,360.0);return angle<0?angle+360.0:angle;
}
inline bool in_sweep(double angle,int start,int sweep) {
    if(sweep>=360||sweep<=-360) return true;
    if(!sweep) return false;
    double extent=sweep<0 ? -double(sweep):double(sweep);
    double delta=normalized_angle(sweep<0 ? double(start)-angle : angle-double(start));
    return delta<=extent+1e-9;
}
inline void arc(Surface& s,int64_t x,int64_t y,int w,int h,int start,int sweep,
                uint32_t color,bool outline,bool dotted=false) {
    if(w<0||h<0||!sweep||(!outline&&(!w||!h))) return;
    double ww=double(w)+(outline?1:0),hh=double(h)+(outline?1:0);
    auto inside=[=](double u,double v) {
        double nx=(u-ww/2)/(ww/2),ny=(v-hh/2)/(hh/2);
        return nx*nx+ny*ny<=1.0+1e-12;
    };
    auto b=intersect(s.bounds(),{x,y,x+int64_t(ww),y+int64_t(hh)});
    for(int64_t yy=b.top;yy<b.bottom;++yy) for(int64_t xx=b.left;xx<b.right;++xx) {
        double u=double(xx-x)+0.5,v=double(yy-y)+0.5;
        if(!inside(u,v)) continue;
        if(outline && inside(u-1,v)&&inside(u+1,v)&&inside(u,v-1)&&inside(u,v+1)) continue;
        // Full ellipses have no angular boundary. Avoid one transcendental
        // calculation per covered pixel; compare directly to handle INT_MIN.
        if(sweep<360 && sweep>-360) {
            double nx=(u-ww/2)/(ww/2),ny=(v-hh/2)/(hh/2);
            double angle=std::atan2(-ny,nx)*180.0/3.14159265358979323846;
            if((nx!=0||ny!=0)&&!in_sweep(angle,start,sweep)) continue;
        }
        if(outline&&dotted&&((xx-x+yy-y)&1)) continue;
        s.put(xx,yy,color);
    }
}
inline void round_rect(Surface& s,int64_t x,int64_t y,int w,int h,int aw,int ah,
                       uint32_t color,bool outline,bool dotted=false) {
    if(w<0||h<0||(!outline&&(!w||!h))) return;
    int64_t ww=int64_t(w)+(outline?1:0),hh=int64_t(h)+(outline?1:0);
    double rx=std::min(int64_t(w),std::abs(int64_t(aw)))/2.0;
    double ry=std::min(int64_t(h),std::abs(int64_t(ah)))/2.0;
    if(rx==0||ry==0) { rect(s,x,y,w,h,color,outline,dotted);return; }
    auto inside=[=](double u,double v) {
        if(u<0||v<0||u>=ww||v>=hh) return false;
        double dx=u<rx ? (u-rx)/rx : u>ww-rx ? (u-(ww-rx))/rx : 0;
        double dy=v<ry ? (v-ry)/ry : v>hh-ry ? (v-(hh-ry))/ry : 0;
        return dx*dx+dy*dy<=1.0+1e-12;
    };
    shape(s,x,y,ww,hh,color,inside,outline,dotted);
}
// Closed-form Bresenham stepping on the visible major axis preserves phase
// and endpoint inclusion while avoiding a walk through offscreen coordinates.
inline void line(Surface& s,int64_t x0,int64_t y0,int64_t x1,int64_t y1,uint32_t color,bool dotted=false) {
    int64_t dx=std::abs(x1-x0),dy=std::abs(y1-y0);
    bool swap=dy>dx;
    if(swap){std::swap(x0,y0);std::swap(x1,y1);std::swap(dx,dy);}
    int64_t sx=x0<x1?1:-1,sy=y0<y1?1:-1;
    auto b=s.bounds();
    int64_t low=swap?b.top:b.left,high=swap?b.bottom:b.right;
    int64_t begin=std::max(int64_t(0),sx>0?low-x0:x0-high+1);
    int64_t end=std::min(dx,sx>0?high-1-x0:x0-low);
    for(int64_t i=begin;i<=end;++i) {
        // Unsigned arithmetic accommodates full-range Java coordinates.
        uint64_t minor=dx ? (uint64_t(i)*uint64_t(dy)+uint64_t(dx/2))/uint64_t(dx):0;
        int64_t xx=x0+sx*i,yy=y0+sy*int64_t(minor);
        if(!dotted||!(i&1)) {if(swap)s.put(yy,xx,color);else s.put(xx,yy,color);}
    }
}
inline void triangle(Surface& s,int64_t x0,int64_t y0,int64_t x1,int64_t y1,int64_t x2,int64_t y2,uint32_t color) {
    auto edge=[](int64_t ax,int64_t ay,int64_t bx,int64_t by,int64_t xx,int64_t yy) {
        return (long double)(bx-ax)*(yy-ay)-(long double)(by-ay)*(xx-ax);
    };
    long double area=edge(x0,y0,x1,y1,x2,y2);
    auto b=intersect(s.bounds(),{std::min({x0,x1,x2}),std::min({y0,y1,y2}),std::max({x0,x1,x2})+1,std::max({y0,y1,y2})+1});
    if(area!=0)for(int64_t yy=b.top;yy<b.bottom;++yy)for(int64_t xx=b.left;xx<b.right;++xx) {
        long double a=edge(x0,y0,x1,y1,xx,yy),c=edge(x1,y1,x2,y2,xx,yy),d=edge(x2,y2,x0,y0,xx,yy);
        if((a>=0&&c>=0&&d>=0)||(a<=0&&c<=0&&d<=0))s.put(xx,yy,color);
    }
    line(s,x0,y0,x1,y1,color);line(s,x1,y1,x2,y2,color);line(s,x2,y2,x0,y0,color);
}
} // namespace raster_primitives
#endif
