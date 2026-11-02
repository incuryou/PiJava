#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
// Pure native transport and raster arithmetic. No VM/Java or platform access.
namespace m3g_raster {
struct Texture {
    int width=0,height=0,format=100,mode=227,color=0;
    bool linear=false,repeatS=false,repeatT=false;
    float uv[6]={};
    std::shared_ptr<const std::vector<uint32_t>> pixels;
};
struct Command {
    int w=0,h=0,ox=0,oy=0,vx=0,vy=0,l=0,t=0,r=0,b=0;
    int depthEnabled=0,alphaFactor=256,mode=68,test=1,write=1,colorWrite=1,alphaWrite=1;
    float threshold=0,factor=0,units=0;
    bool hasColors=false,perspective=false;
    uint32_t defColor=0;
    double x[3]={},y[3]={},z[3]={},iw[3]={1,1,1};
    uint32_t co[3]={};
    std::array<Texture,4> textures;
    int count=0;
};
struct Surface {int width=0,height=0;std::vector<uint32_t> colors;std::vector<int16_t> depth;};
struct Counts {bool ok=true;uint64_t triangles=0,pixels=0;};
inline int m3g_clamp(int n) { return std::max(0,std::min(255,n)); }
inline uint32_t m3g_pack(const int c[4]) {
    return (uint32_t(m3g_clamp(c[0]))<<24)|(uint32_t(m3g_clamp(c[1]))<<16)|(uint32_t(m3g_clamp(c[2]))<<8)|uint32_t(m3g_clamp(c[3]));
}
inline void m3g_unpack(uint32_t p,int c[4]) { for(int i=0;i<4;++i)c[i]=(p>>(24-8*i))&255; }
inline int m3g_wrap(double n,int size,bool repeat) {
    if(!repeat)return n<0?0:(n>=size?size-1:int(n));
    double v=std::fmod(n,double(size));if(v<0)v+=size;return int(v);
}
template<class IO> inline uint32_t m3g_sample(IO& io,int unit,const Texture& t,double s,double v) {
    // Java Triangle coordinates are already in texel units.
    if(!t.linear) {
        int x=m3g_wrap(std::floor(s),t.width,t.repeatS),y=m3g_wrap(std::floor(v),t.height,t.repeatT);
        return uint32_t(io.texture(unit,size_t(y)*t.width+x));
    }
    double sx=std::trunc(s*256.0)-128.0,sy=std::trunc(v*256.0)-128.0;
    double ix=std::floor(sx/256.0),iy=std::floor(sy/256.0);
    int fx=int(sx-ix*256.0),fy=int(sy-iy*256.0);
    int x0=m3g_wrap(ix,t.width,t.repeatS),x1=m3g_wrap(ix+1,t.width,t.repeatS);
    int y0=m3g_wrap(iy,t.height,t.repeatT),y1=m3g_wrap(iy+1,t.height,t.repeatT);
    int a[4],b[4],c[4],d[4],out[4];
    m3g_unpack(io.texture(unit,size_t(y0)*t.width+x0),a);m3g_unpack(io.texture(unit,size_t(y0)*t.width+x1),b);
    m3g_unpack(io.texture(unit,size_t(y1)*t.width+x0),c);m3g_unpack(io.texture(unit,size_t(y1)*t.width+x1),d);
    for(int k=0;k<4;++k)out[k]=((((a[k]*(256-fx)+b[k]*fx)>>8)*(256-fy))+(((c[k]*(256-fx)+d[k]*fx)>>8)*fy))>>8;
    return m3g_pack(out);
}
inline uint32_t m3g_texture_blend(uint32_t bg,uint32_t fg,const Texture& t) {
    int b[4],f[4],c[4],o[4];m3g_unpack(bg,b);m3g_unpack(fg,f);m3g_unpack(t.color,c);
    for(int i=0;i<4;++i)o[i]=b[i];
    bool alpha=t.format==96,rgba=t.format==98||t.format==100;
    if(t.mode==228) { if(alpha)o[0]=f[0];else return fg; }
    else if(alpha) { if(t.mode!=226)o[0]=(b[0]*f[0]+128)>>8; }
    else if(t.mode==224) { for(int i=1;i<4;++i)o[i]=std::min(255,b[i]+f[i]);if(rgba)o[0]=(b[0]*f[0]+128)>>8; }
    else if(t.mode==225) { for(int i=1;i<4;++i)o[i]=b[i]+(((c[i]-b[i])*f[i]+128)>>8);if(rgba)o[0]=(b[0]*f[0]+128)>>8; }
    else if(t.mode==226) { if(t.format==99)return fg;if(t.format==100)for(int i=1;i<4;++i)o[i]=b[i]+(((f[i]-b[i])*f[0])>>8); }
    else if(t.mode==227) { for(int i=1;i<4;++i)o[i]=(b[i]*f[i]+(rgba?128:0))>>8;if(rgba)o[0]=(b[0]*f[0]+b[0])>>8; }
    return m3g_pack(o);
}
inline uint32_t m3g_composite(uint32_t bg,uint32_t fg,int alpha,int mode) {
    if(mode==68)return fg;
    if((mode==64||mode==65)&&alpha<=0)return bg;
    if(mode==64&&alpha>=255)return fg;
    int b[4],f[4],o[4];m3g_unpack(bg,b);m3g_unpack(fg,f);
    if(mode==64) {o[0]=alpha+((b[0]*(255-alpha))>>8);for(int i=1;i<4;++i)o[i]=(f[i]*alpha+b[i]*(255-alpha))>>8;}
    else if(mode==65) {o[0]=b[0]+((alpha*(255-b[0]))>>8);for(int i=1;i<4;++i)o[i]=std::min(255,b[i]+((f[i]*alpha)>>8));}
    else {int shift=mode==67?7:8;for(int i=0;i<4;++i)o[i]=std::min(255,(b[i]*f[i])>>shift);}
    return m3g_pack(o);
}
template<class IO> inline Counts draw(const Command& q,IO& io){
    const int w=q.w,h=q.h,ox=q.ox,oy=q.oy,vx=q.vx,vy=q.vy,l=q.l,t=q.t,r=q.r,b=q.b;
    const int depthEnabled=q.depthEnabled,alphaFactor=q.alphaFactor,mode=q.mode,test=q.test,write=q.write,colorWrite=q.colorWrite,alphaWrite=q.alphaWrite,count=q.count;
    const float threshold=q.threshold,factor=q.factor,units=q.units;
    const bool hasColors=q.hasColors,perspective=q.perspective;const uint32_t defColor=q.defColor;
    const auto& x=q.x;const auto& y=q.y;const auto& z=q.z;const auto& iw=q.iw;const auto& co=q.co;const auto& tex=q.textures;
    double area=(x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]);
    if(!std::isfinite(area))return {false,0,0};
    Counts result;result.triangles=1;
    if(std::fabs(area)<1e-6)return result;
    int order[3]={0,1,2};if(area<0){std::swap(order[1],order[2]);area=-area;}
    const int64_t originX=int64_t(ox)+vx,originY=int64_t(oy)+vy;
    int64_t ll=std::max<int64_t>(l,-originX),rr=std::min<int64_t>(r,int64_t(w)-originX);
    int64_t tt=std::max<int64_t>(t,-originY),bb=std::min<int64_t>(b,int64_t(h)-originY);
    if(ll>=rr||tt>=bb)return result;
    // Clip before converting floats to integers, including very large offscreen coordinates.
    double minx=std::max(double(ll),std::ceil(std::min({x[0],x[1],x[2]})-0.5));
    double maxx=std::min(double(rr),std::floor(std::max({x[0],x[1],x[2]})-0.5)+1);
    double miny=std::max(double(tt),std::ceil(std::min({y[0],y[1],y[2]})-0.5));
    double maxy=std::min(double(bb),std::floor(std::max({y[0],y[1],y[2]})-0.5)+1);
    if(minx>=maxx||miny>=maxy)return result;
    double offset=0;
    if(depthEnabled&&(factor!=0||units!=0)) {
        double dzdx=((z[1]-z[0])*(y[2]-y[0])-(z[2]-z[0])*(y[1]-y[0]))/((x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]));
        double dzdy=((x[1]-x[0])*(z[2]-z[0])-(x[2]-x[0])*(z[1]-z[0]))/((x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]));
        offset=factor*std::hypot(dzdx,dzdy)+units*1e-7;
    }
    int vc[3][4];for(int i=0;i<3;++i)m3g_unpack(co[i],vc[i]);
    // A constant plane has zero depth gradients in the Java rasterizer.
    // Reweighting its identical depths can produce e.g.16099.999999999998
    // instead of16100 and incorrectly win a later equal-depth comparison.
    const bool constantDepth=z[0]==z[1]&&z[0]==z[2]&&offset==0;
    bool topLeft[3];double dx[3],dy[3];
    for(int k=0;k<3;++k){int a=order[(k+1)%3],c=order[(k+2)%3];dx[k]=x[c]-x[a];dy[k]=y[c]-y[a];topLeft[k]=dy[k]<0||(dy[k]==0&&dx[k]>0);}
    for(int py=int(miny);py<int(maxy);++py) {
        double edge[3];for(int k=0;k<3;++k){int a=order[(k+1)%3];edge[k]=dx[k]*(py+0.5-y[a])-dy[k]*(minx+0.5-x[a]);}
        for(int px=int(minx);px<int(maxx);++px) {
            double e[3]={edge[0],edge[1],edge[2]};for(int k=0;k<3;++k)edge[k]-=dy[k];
            if((e[0]<0||(e[0]==0&&!topLeft[0]))||(e[1]<0||(e[1]==0&&!topLeft[1]))||(e[2]<0||(e[2]==0&&!topLeft[2])))continue;
            double weight[3];for(int k=0;k<3;++k)weight[order[k]]=e[k]/area;
            size_t index=size_t(originY+py)*w+size_t(originX+px);
            double zz=constantDepth?z[0]:weight[0]*z[0]+weight[1]*z[1]+weight[2]*z[2]+offset;
            int depth=int(std::max(-32768.0,std::min(32767.0,zz)));
            if(depthEnabled&&test&&io.depth(index)<depth)continue;
            int c[4];for(int k=0;k<4;++k)c[k]=int(weight[0]*vc[0][k]+weight[1]*vc[1][k]+weight[2]*vc[2][k]);
            uint32_t fg=hasColors?m3g_pack(c):uint32_t(defColor);
            double recip=1;if(perspective){double q=weight[0]*iw[0]+weight[1]*iw[1]+weight[2]*iw[2];if(q==0||!std::isfinite(q))continue;recip=1/q;}
            for(int k=0;k<count;++k) {
                double s=(weight[0]*tex[k].uv[0]+weight[1]*tex[k].uv[2]+weight[2]*tex[k].uv[4])*recip;
                double v=(weight[0]*tex[k].uv[1]+weight[1]*tex[k].uv[3]+weight[2]*tex[k].uv[5])*recip;
                fg=m3g_texture_blend(fg,m3g_sample(io,k,tex[k],s,v),tex[k]);
            }
            int alpha=((fg>>24)*alphaFactor)>>8;if(alpha<int(threshold*255))continue;
            if(depthEnabled&&write)io.depth(index,depth);
            if(colorWrite||alphaWrite) {
                uint32_t bg=io.color(index),out=m3g_composite(bg,fg,alpha,mode);
                if(!colorWrite)out=(out&0xff000000u)|(bg&0x00ffffffu);
                if(!alphaWrite)out=(out&0x00ffffffu)|(bg&0xff000000u);
                io.color(index,out);++result.pixels;
            }
        }
    }
    return result;
}
struct OwnedIO {
    Surface& surface;const Command& command;
    uint32_t color(size_t i)const{return surface.colors[i];}
    void color(size_t i,uint32_t v){surface.colors[i]=v;}
    int depth(size_t i)const{return surface.depth[i];}
    void depth(size_t i,int v){surface.depth[i]=int16_t(v);}
    uint32_t texture(int unit,size_t i)const{return (*command.textures[size_t(unit)].pixels)[i];}
};
inline Counts draw_owned(const Command& command,Surface& surface){OwnedIO io{surface,command};return draw(command,io);}
} // namespace m3g_raster
