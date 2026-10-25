#include "vm_rs_fallback.h"
#ifndef J2ME_M3G_NATIVE_H
#define J2ME_M3G_NATIVE_H
#include <algorithm>
#include <cmath>
#include <cstdint>
// All storage remains in Java objects, so snapshots and GC own the state.
 VMS_RS_ACCESSOR(g_m3g_native_triangles, uint64_t) VMS_RS_ACCESSOR(g_m3g_native_pixels, uint64_t)
 VMS_RS_ACCESSOR(g_m3g_fallback_aa, uint64_t) VMS_RS_ACCESSOR(g_m3g_fallback_dither, uint64_t) VMS_RS_ACCESSOR(g_m3g_fallback_fog, uint64_t) VMS_RS_ACCESSOR(g_m3g_fallback_mip, uint64_t)
static Value* m3g_field(int ref, const char* name, const char* desc) {
    if (ref <= 0 || ref >= int(VMS_HEAP(g_heap).size()) || VMS_HEAP(g_heap)[ref].cls < 0) return nullptr;
    Field* f = find_field(VMS_HEAP(g_heap)[ref].cls, name, desc);
    if (!f || f->slot < 0) return nullptr;
    auto& v = f->is_static ? VMS_CL(g_classes)[f->owner].statics : VMS_HEAP(g_heap)[ref].fields;
    return size_t(f->slot) < v.size() ? &v[f->slot] : nullptr;
}
static bool m3g_int(int ref,const char* name,const char* desc,int& out) {
    Value* v=m3g_field(ref,name,desc); if(!v)return false;out=v->i;return true;
}
static int m3g_ref(int ref,const char* name,const char* desc) {
    Value* v=m3g_field(ref,name,desc);return v?v->ref:0;
}
static ArrayStorage* m3g_array(int ref,const char* desc,size_t minimum) {
    if(ref<=0||ref>=int(VMS_HEAP(g_heap).size()))return nullptr;
    Obj& o=VMS_HEAP(g_heap)[ref];return o.is_array&&o.array_desc==desc&&o.elems.size()>=minimum?&o.elems:nullptr;
}
static int m3g_clamp(int n) { return std::max(0,std::min(255,n)); }
static uint32_t m3g_pack(const int c[4]) {
    return (uint32_t(m3g_clamp(c[0]))<<24)|(uint32_t(m3g_clamp(c[1]))<<16)|(uint32_t(m3g_clamp(c[2]))<<8)|uint32_t(m3g_clamp(c[3]));
}
static void m3g_unpack(uint32_t p,int c[4]) { for(int i=0;i<4;++i)c[i]=(p>>(24-8*i))&255; }
struct M3GNativeTexture {
    ArrayStorage* pixels=nullptr; int width=0,height=0,format=100,mode=227,color=0;
    bool linear=false,repeatS=false,repeatT=false; float uv[6]={};
};
static int m3g_wrap(double n,int size,bool repeat) {
    if(!repeat)return n<0?0:(n>=size?size-1:int(n));
    double v=std::fmod(n,double(size));if(v<0)v+=size;return int(v);
}
static uint32_t m3g_sample(const M3GNativeTexture& t,double s,double v) {
    // Java Triangle coordinates are already in texel units.
    if(!t.linear) {
        int x=m3g_wrap(std::floor(s),t.width,t.repeatS),y=m3g_wrap(std::floor(v),t.height,t.repeatT);
        return uint32_t(t.pixels->get(size_t(y)*t.width+x).i);
    }
    double sx=std::trunc(s*256.0)-128.0,sy=std::trunc(v*256.0)-128.0;
    double ix=std::floor(sx/256.0),iy=std::floor(sy/256.0);
    int fx=int(sx-ix*256.0),fy=int(sy-iy*256.0);
    int x0=m3g_wrap(ix,t.width,t.repeatS),x1=m3g_wrap(ix+1,t.width,t.repeatS);
    int y0=m3g_wrap(iy,t.height,t.repeatT),y1=m3g_wrap(iy+1,t.height,t.repeatT);
    int a[4],b[4],c[4],d[4],out[4];
    m3g_unpack(t.pixels->get(size_t(y0)*t.width+x0).i,a);m3g_unpack(t.pixels->get(size_t(y0)*t.width+x1).i,b);
    m3g_unpack(t.pixels->get(size_t(y1)*t.width+x0).i,c);m3g_unpack(t.pixels->get(size_t(y1)*t.width+x1).i,d);
    for(int k=0;k<4;++k)out[k]=((((a[k]*(256-fx)+b[k]*fx)>>8)*(256-fy))+(((c[k]*(256-fx)+d[k]*fx)>>8)*fy))>>8;
    return m3g_pack(out);
}
static uint32_t m3g_texture_blend(uint32_t bg,uint32_t fg,const M3GNativeTexture& t) {
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
static uint32_t m3g_composite(uint32_t bg,uint32_t fg,int alpha,int mode) {
    if(mode==68)return fg;
    if((mode==64||mode==65)&&alpha<=0)return bg;
    if(mode==64&&alpha>=255)return fg;
    int b[4],f[4],o[4];m3g_unpack(bg,b);m3g_unpack(fg,f);
    if(mode==64) {o[0]=alpha+((b[0]*(255-alpha))>>8);for(int i=1;i<4;++i)o[i]=(f[i]*alpha+b[i]*(255-alpha))>>8;}
    else if(mode==65) {o[0]=b[0]+((alpha*(255-b[0]))>>8);for(int i=1;i<4;++i)o[i]=std::min(255,b[i]+((f[i]*alpha)>>8));}
    else {int shift=mode==67?7:8;for(int i=0;i<4;++i)o[i]=std::min(255,(b[i]*f[i])>>shift);}
    return m3g_pack(o);
}
static bool m3g_draw_triangle(int g,int tri,int appearance,int defColor,bool hasColors,bool hasTex,bool perspective) {
    int hints=0,w=0,h=0,ox=0,oy=0,vx=0,vy=0,l=0,t=0,r=0,b=0,depthEnabled=0,alphaFactor=0;
    if(!m3g_int(g,"hints","I",hints))return false;
    if(hints&6){if(hints&2)++VMS_RS(g_m3g_fallback_aa);if(hints&4)++VMS_RS(g_m3g_fallback_dither);return false;}
    if(m3g_ref(appearance,"fog","Ljavax/microedition/m3g/Fog;")){++VMS_RS(g_m3g_fallback_fog);return false;}
    if(!m3g_int(g,"canvasWidth","I",w)||!m3g_int(g,"canvasHeight","I",h)||w<=0||h<=0)return false;
    if(!m3g_int(g,"originX","I",ox)||!m3g_int(g,"originY","I",oy)||!m3g_int(g,"viewx","I",vx)||!m3g_int(g,"viewy","I",vy)||
       !m3g_int(g,"viewClipL","I",l)||!m3g_int(g,"viewClipT","I",t)||!m3g_int(g,"viewClipR","I",r)||!m3g_int(g,"viewClipB","I",b)||
       !m3g_int(g,"depthEnabled","Z",depthEnabled)||!m3g_int(g,"currAlphaFactor","I",alphaFactor))return false;
    if(int64_t(w)*h>INT32_MAX)return false;
    ArrayStorage* pixels=m3g_array(m3g_ref(g,"rasterData","[I"),"[I",size_t(w)*h);
    ArrayStorage* depths=m3g_array(m3g_ref(g,"depthBuffer","[S"),"[S",size_t(w)*h);
    ArrayStorage* verts=m3g_array(m3g_ref(tri,"v","[F"),"[F",12);
    ArrayStorage* colors=hasColors?m3g_array(m3g_ref(tri,"colors","[I"),"[I",3):nullptr;
    ArrayStorage* invw=perspective?m3g_array(m3g_ref(tri,"invW","[F"),"[F",3):nullptr;
    if(!pixels||!verts||(depthEnabled&&!depths)||(hasColors&&!colors)||(perspective&&!invw))return false;
    int comp=m3g_ref(appearance,"compositingMode","Ljavax/microedition/m3g/CompositingMode;");
    if(!comp)comp=m3g_ref(g,"defaultCompositing","Ljavax/microedition/m3g/CompositingMode;");
    int mode=68,test=1,write=1,colorWrite=1,alphaWrite=1;float threshold=0,factor=0,units=0;
    if(comp) {
        if(!m3g_int(comp,"blending","I",mode)||!m3g_int(comp,"depthTest","Z",test)||!m3g_int(comp,"depthWrite","Z",write)||
           !m3g_int(comp,"colorWrite","Z",colorWrite)||!m3g_int(comp,"alphaWrite","Z",alphaWrite))return false;
        Value* at=m3g_field(comp,"alphaThreshold","F"),*df=m3g_field(comp,"depthOffsetFactor","F"),*du=m3g_field(comp,"depthOffsetUnits","F");
        if(!at||!df||!du)return false;
        threshold=bits_f(at->i);factor=bits_f(df->i);units=bits_f(du->i);
    }
    if(mode<64||mode>68||!std::isfinite(threshold)||!std::isfinite(factor)||!std::isfinite(units)||alphaFactor<0||alphaFactor>256)return false;
    M3GNativeTexture tex[4];int count=0;
    if(hasTex) {
        if(!m3g_int(g,"ACTIVE_TEXTURE_UNITS","B",count)||count<0||count>4)return false;
        ArrayStorage* textures=m3g_array(m3g_ref(g,"textures","[Ljavax/microedition/m3g/Texture2D;"),"[Ljavax/microedition/m3g/Texture2D;",count);
        ArrayStorage* coords=m3g_array(m3g_ref(tri,"t","[[F"),"[[F",count);if(!textures||!coords)return false;
        for(int i=0;i<count;++i) {
            int tr=textures->get(i).ref,img=m3g_ref(tr,"texImage","Ljavax/microedition/m3g/Image2D;"),level,filter,ws,wt;
            if(!m3g_int(tr,"levelFilter","I",level))return false;
            if(level!=208){++VMS_RS(g_m3g_fallback_mip);return false;}
            if(!m3g_int(tr,"imageFilter","I",filter)||
               !m3g_int(tr,"wraps","I",ws)||!m3g_int(tr,"wrapt","I",wt)||!m3g_int(tr,"blending","I",tex[i].mode)||!m3g_int(tr,"blendColor","I",tex[i].color)||
               !m3g_int(img,"width","I",tex[i].width)||!m3g_int(img,"height","I",tex[i].height)||!m3g_int(img,"format","I",tex[i].format))return false;
            if(tex[i].width<=0||tex[i].height<=0||int64_t(tex[i].width)*tex[i].height>INT32_MAX||(filter!=209&&filter!=210)||
               (ws!=240&&ws!=241)||(wt!=240&&wt!=241)||tex[i].mode<224||tex[i].mode>228||tex[i].format<96||tex[i].format>100)return false;
            tex[i].pixels=m3g_array(m3g_ref(img,"image","[I"),"[I",size_t(tex[i].width)*tex[i].height);
            ArrayStorage* uv=m3g_array(coords->get(i).ref,"[F",6);if(!tex[i].pixels||!uv)return false;
            tex[i].linear=filter==209;tex[i].repeatS=ws==241;tex[i].repeatT=wt==241;
            for(int k=0;k<6;++k){tex[i].uv[k]=bits_f(uv->get(k).i);if(!std::isfinite(tex[i].uv[k]))return false;}
        }
    }
    double x[3],y[3],z[3],iw[3]={1,1,1};uint32_t co[3];
    for(int i=0;i<3;++i) {
        x[i]=bits_f(verts->get(i*4).i);y[i]=bits_f(verts->get(i*4+1).i);z[i]=bits_f(verts->get(i*4+2).i);
        if(perspective)iw[i]=bits_f(invw->get(i).i);
        co[i]=hasColors?uint32_t(colors->get(i).i):uint32_t(defColor);
        if(!std::isfinite(x[i])||!std::isfinite(y[i])||!std::isfinite(z[i])||!std::isfinite(iw[i]))return false;
    }
    double area=(x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]);
    if(!std::isfinite(area))return false;
    ++VMS_RS(g_m3g_native_triangles);
    if(std::fabs(area)<1e-6)return true;
    int order[3]={0,1,2};if(area<0){std::swap(order[1],order[2]);area=-area;}
    const int64_t originX=int64_t(ox)+vx,originY=int64_t(oy)+vy;
    int64_t ll=std::max<int64_t>(l,-originX),rr=std::min<int64_t>(r,int64_t(w)-originX);
    int64_t tt=std::max<int64_t>(t,-originY),bb=std::min<int64_t>(b,int64_t(h)-originY);
    if(ll>=rr||tt>=bb)return true;
    // Clip before converting floats to integers, including very large offscreen coordinates.
    double minx=std::max(double(ll),std::ceil(std::min({x[0],x[1],x[2]})-0.5));
    double maxx=std::min(double(rr),std::floor(std::max({x[0],x[1],x[2]})-0.5)+1);
    double miny=std::max(double(tt),std::ceil(std::min({y[0],y[1],y[2]})-0.5));
    double maxy=std::min(double(bb),std::floor(std::max({y[0],y[1],y[2]})-0.5)+1);
    if(minx>=maxx||miny>=maxy)return true;
    double offset=0;
    if(depthEnabled&&(factor!=0||units!=0)) {
        double dzdx=((z[1]-z[0])*(y[2]-y[0])-(z[2]-z[0])*(y[1]-y[0]))/((x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]));
        double dzdy=((x[1]-x[0])*(z[2]-z[0])-(x[2]-x[0])*(z[1]-z[0]))/((x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]));
        offset=factor*std::hypot(dzdx,dzdy)+units*1e-7;
    }
    int vc[3][4];for(int i=0;i<3;++i)m3g_unpack(co[i],vc[i]);
    bool topLeft[3];double dx[3],dy[3];
    for(int k=0;k<3;++k){int a=order[(k+1)%3],c=order[(k+2)%3];dx[k]=x[c]-x[a];dy[k]=y[c]-y[a];topLeft[k]=dy[k]<0||(dy[k]==0&&dx[k]>0);}
    for(int py=int(miny);py<int(maxy);++py) {
        double edge[3];for(int k=0;k<3;++k){int a=order[(k+1)%3];edge[k]=dx[k]*(py+0.5-y[a])-dy[k]*(minx+0.5-x[a]);}
        for(int px=int(minx);px<int(maxx);++px) {
            double e[3]={edge[0],edge[1],edge[2]};for(int k=0;k<3;++k)edge[k]-=dy[k];
            if((e[0]<0||(e[0]==0&&!topLeft[0]))||(e[1]<0||(e[1]==0&&!topLeft[1]))||(e[2]<0||(e[2]==0&&!topLeft[2])))continue;
            double weight[3];for(int k=0;k<3;++k)weight[order[k]]=e[k]/area;
            size_t index=size_t(originY+py)*w+size_t(originX+px);
            double zz=weight[0]*z[0]+weight[1]*z[1]+weight[2]*z[2]+offset;
            int depth=int(std::max(-32768.0,std::min(32767.0,zz)));
            if(depthEnabled&&test&&depths->get(index).i<depth)continue;
            int c[4];for(int k=0;k<4;++k)c[k]=int(weight[0]*vc[0][k]+weight[1]*vc[1][k]+weight[2]*vc[2][k]);
            uint32_t fg=hasColors?m3g_pack(c):uint32_t(defColor);
            double recip=1;if(perspective){double q=weight[0]*iw[0]+weight[1]*iw[1]+weight[2]*iw[2];if(q==0||!std::isfinite(q))continue;recip=1/q;}
            for(int k=0;k<count;++k) {
                double s=(weight[0]*tex[k].uv[0]+weight[1]*tex[k].uv[2]+weight[2]*tex[k].uv[4])*recip;
                double v=(weight[0]*tex[k].uv[1]+weight[1]*tex[k].uv[3]+weight[2]*tex[k].uv[5])*recip;
                fg=m3g_texture_blend(fg,m3g_sample(tex[k],s,v),tex[k]);
            }
            int alpha=((fg>>24)*alphaFactor)>>8;if(alpha<int(threshold*255))continue;
            if(depthEnabled&&write)depths->set(index,Value::of_i(depth));
            if(colorWrite||alphaWrite) {
                uint32_t bg=pixels->get(index).i,out=m3g_composite(bg,fg,alpha,mode);
                if(!colorWrite)out=(out&0xff000000u)|(bg&0x00ffffffu);
                if(!alphaWrite)out=(out&0x00ffffffu)|(bg&0xff000000u);
                pixels->set(index,Value::of_i(int32_t(out)));++VMS_RS(g_m3g_native_pixels);
            }
        }
    }
    return true;
}
#endif
