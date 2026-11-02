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
    const int cls=VMS_HEAP(g_heap)[ref].cls;
    if(size_t(cls)>=VMS_CL(g_classes).size())return nullptr;
    // Native islands repeatedly access a small, fixed set of fields. Avoid
    // rescanning the class (Graphics3D has over a hundred fields) each row.
    // Hashes choose a slot only; the current name/descriptor validate each hit.
    uint32_t hash=2166136261u;
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(name);*p;++p)hash=(hash^*p)*16777619u;
    hash=(hash^0xffu)*16777619u;
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(desc);*p;++p)hash=(hash^*p)*16777619u;
    auto& cache=VMS_CL(g_classes)[cls].m3g_field_slots31;
    if(cache.empty())cache.assign(64,{-1,-1});
    // Mix the upper bits: depthBuffer, rowFogStep240 and compBlender otherwise
    // collide on the low six FNV bits and evict one another on every row.
    auto& bound=cache[(hash^(hash>>16))&63u];Field* f=nullptr;
    if(bound.first>=0&&size_t(bound.first)<VMS_CL(g_classes).size()){
        auto& fields=VMS_CL(g_classes)[bound.first].fields;
        if(bound.second>=0&&size_t(bound.second)<fields.size()){
            auto& candidate=fields[bound.second];
            if(candidate.name==name&&candidate.desc==desc)f=&candidate;
        }
    }
    if(!f){
        f=find_field(cls,name,desc);
        if(f&&f->owner>=0&&size_t(f->owner)<VMS_CL(g_classes).size()){
            const auto& fields=VMS_CL(g_classes)[f->owner].fields;
            bound={f->owner,int(f-fields.data())};
        }
    }
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
#include "m3g_raster.h"
using m3g_raster::m3g_clamp;
using m3g_raster::m3g_pack;
using m3g_raster::m3g_unpack;
using m3g_raster::m3g_wrap;
using m3g_raster::m3g_composite;
struct M3GNativeTexture : m3g_raster::Texture {ArrayStorage* pixels=nullptr;};
namespace m3g_raster {
// Main-only capture hook. No ArrayStorage address enters a worker command.
inline bool (*cpu_capture)(int,const Command&,ArrayStorage*,ArrayStorage*,const std::array<ArrayStorage*,4>&)=nullptr;
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
    m3g_raster::Command command;
    command.w=w;command.h=h;command.ox=ox;command.oy=oy;command.vx=vx;command.vy=vy;command.l=l;command.t=t;command.r=r;command.b=b;command.depthEnabled=depthEnabled;command.alphaFactor=alphaFactor;command.mode=mode;command.test=test;command.write=write;command.colorWrite=colorWrite;command.alphaWrite=alphaWrite;command.threshold=threshold;command.factor=factor;command.units=units;command.hasColors=hasColors;command.perspective=perspective;command.defColor=defColor;command.count=count;
    for(int j=0;j<3;++j){command.x[j]=x[j];command.y[j]=y[j];command.z[j]=z[j];command.iw[j]=iw[j];command.co[j]=co[j];}
    std::array<ArrayStorage*,4> texture_sources{};
    for(int j=0;j<count;++j){command.textures[size_t(j)]=tex[j];texture_sources[size_t(j)]=tex[j].pixels;}
    if(m3g_raster::cpu_capture&&m3g_raster::cpu_capture(g,command,pixels,depths,texture_sources))return true;
    struct DirectIO {
        ArrayStorage *pixels,*depths;const std::array<ArrayStorage*,4>& textures;
        uint32_t color(size_t i)const{return uint32_t(pixels->get(i).i);}
        void color(size_t i,uint32_t v){pixels->set(i,Value::of_i(int32_t(v)));}
        int depth(size_t i)const{return depths->get(i).i;}
        void depth(size_t i,int v){depths->set(i,Value::of_i(v));}
        uint32_t texture(int unit,size_t i)const{return uint32_t(textures[size_t(unit)]->get(i).i);}
    } io{pixels,depths,texture_sources};
    const auto result=m3g_raster::draw(command,io);
    VMS_RS(g_m3g_native_triangles)+=result.triangles;VMS_RS(g_m3g_native_pixels)+=result.pixels;
    return result.ok;
}
#endif
