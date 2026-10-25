#pragma once
#include "vm_rs_fallback.h"
// SPDX-License-Identifier: GPL-3.0-or-later
// Arithmetic translated from the bundled FreeJ2ME Graphics3D/Graphics3DPipelines.
/* Stage 0.2.38-s1: bounded texture-only native island.
 * Translates the bundled 0.2.36 Graphics3D texturing arithmetic, including its
 * LOD dithering and coordinate conventions. NOT a new triangle rasterizer.
 * No Java allocation/callback/yield, no retained heap pointer, no framebuffer
 * publication. Validate every unit before modifying the Java pipeline state.
 * Java reference path remains immediately below each call site.
 */
#ifndef J2ME_OPT_M3G_TEXTURE238
#define J2ME_OPT_M3G_TEXTURE238 1
#endif
 VMS_RS_ACCESSOR(g_m3g_texture238_enabled, bool)
 VMS_RS_ACCESSOR(g_m3g_texture238_calls, uint64_t) VMS_RS_ACCESSOR(g_m3g_texture238_units, uint64_t)
 VMS_RS_ACCESSOR(g_m3g_texture238_mips, uint64_t) VMS_RS_ACCESSOR(g_m3g_texture238_declined, uint64_t)
namespace m3g_texture238 {
// Force binary32 rounding, including on an FMA-capable AArch64 compiler.
static float mul(float a,float b){volatile float v=a*b;return v;}
static float add(float a,float b){volatile float v=a+b;return v;}
static float sub(float a,float b){volatile float v=a-b;return v;}
static int32_t sar(uint32_t x,unsigned n){n&=31;if(!n)return int_bits(x);return int_bits((x>>n)|((x&0x80000000u)?(~0u<<(32-n)):0u));}
static bool bounded(float x){return std::isfinite(x)&&x>-8000000.0f&&x<8000000.0f;}
static Value* stat(int cid,int key,const char* name,const char* desc){
    if(cid<0||size_t(cid)>=VMS_CL(g_classes).size()||key<0||key>=48)return nullptr;
    auto& cache=VMS_CL(g_classes)[cid].m3g_slots238;
    if(cache.empty())cache.assign(48,{-2,-2});
    auto& bound=cache[key];
    if(bound.first==-2){
        auto* f=find_field(cid,name,desc);
        bound=f&&f->is_static?std::make_pair(f->owner,f->slot):std::make_pair(-1,-1);
    }
    if(bound.first<0||size_t(bound.first)>=VMS_CL(g_classes).size()||bound.second<0||size_t(bound.second)>=VMS_CL(g_classes)[bound.first].statics.size())return nullptr;
    return &VMS_CL(g_classes)[bound.first].statics[bound.second];
}
static int class_id(const char* name){auto it=VMS_CL(g_class_index).find(name);return it==VMS_CL(g_class_index).end()?-1:it->second;}
static int kind(int obj,const char* cls,const char* desc,const char* const* names,int n){
    if(!obj)return 0;
    const int cid=class_id(cls);if(cid<0)return -1;
    for(int i=0;i<n;++i){auto* v=stat(cid,32+i,names[i],desc);if(v&&v->ref==obj)return i+1;}
    return -1;
}
static bool exact_class(int ref,const char* name){
    return ref>0&&size_t(ref)<VMS_HEAP(g_heap).size()&&VMS_HEAP(g_heap)[ref].cls>=0&&VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name==name;
}
static bool scalar(int obj,const char* name,const char* desc,int& v){return m3g_int(obj,name,desc,v);}
struct Unit {ArrayStorage* pixels=nullptr;int w=0,h=0,shift=0,wrapper=0,blend=0;bool pot=false,linear=false,rs=false,rt=false;uint32_t blendColor=0;float s=0,t=0,nextS=0,nextT=0;bool mip=false;};
static int wrap(int32_t c,int size,int k){
    if(k==1)return int(uint32_t(c)&uint32_t(size-1)); // source POT_REPEAT
    if(k==2)return c<0?0:(c>=size?size-1:c); // CLAMP
    // Normalized signed modulo: also used by the corrected Java reference.
    // The former signed Barrett reduction failed at negative multiples.
    int v=c%size;return v<0?v+size:v;
}
static uint32_t sample(const Unit& u){
    int32_t sx,sy;int fx=0,fy=0;
    if(u.linear){sx=int_bits(uint32_t(int32_t(mul(u.s,256.0f)))-128u);sy=int_bits(uint32_t(int32_t(mul(u.t,256.0f)))-128u);fx=uint32_t(sx)&255;fy=uint32_t(sy)&255;sx=sar(uint32_t(sx),8);sy=sar(uint32_t(sy),8);}
    else{sx=int32_t(add(u.s,32768.0f))-32768;sy=int32_t(add(u.t,32768.0f))-32768;}
    const int x0=wrap(sx,u.w,u.wrapper),y0=wrap(sy,u.h,u.wrapper);
    auto pix=[&](int x,int y){return uint32_t(u.pixels->get(size_t(y)*u.w+x).i);};
    const uint32_t a=pix(x0,y0);if(!u.linear)return a;
    const int x1=x0+1<u.w?x0+1:(u.rs?0:x0),y1=y0+1<u.h?y0+1:(u.rt?0:y0);
    const uint32_t b=pix(x1,y0),c=pix(x0,y1),d=pix(x1,y1);
    const uint32_t mask=0x00ff00ffu,ix=256-fx,iy=256-fy;
    const uint32_t rb0=(((a&mask)*ix+(b&mask)*fx)>>8)&mask,ag0=((((a>>8)&mask)*ix+((b>>8)&mask)*fx)>>8)&mask;
    const uint32_t rb1=(((c&mask)*ix+(d&mask)*fx)>>8)&mask,ag1=((((c>>8)&mask)*ix+((d>>8)&mask)*fx)>>8)&mask;
    return (((ag0*iy+ag1*fy)>>8)&mask)<<8 | (((rb0*iy+rb1*fy)>>8)&mask);
}
static uint32_t blend(uint32_t bg,uint32_t fg,const Unit& u){
    const uint32_t m=0x00ff00ffu;const int ba=bg>>24,fa=fg>>24;
    if(u.blend==0)return fg;
    if(u.blend==1)return bg;
    if(u.blend==2)return (fg&0xff000000u)|(bg&0xffffffu);
    if(u.blend==3||u.blend==4){
        uint32_t rb=(bg&m)+(fg&m),og=(bg&0xff00u)+(fg&0xff00u);
        rb=(rb|((rb&0x01000100u)-((rb&0x01000100u)>>8)))&m;
        og=(og|((og&0x00010000u)-((og&0x00010000u)>>8)))&0xff00u;
        uint32_t a=u.blend==3?bg&0xff000000u:uint32_t((ba*fa+128)>>8)<<24;
        return a|og|rb;
    }
    if(u.blend==5)return uint32_t((ba*fa+128)>>8)<<24 | (bg&0xffffffu);
    if(u.blend==6||u.blend==7){
        uint32_t out=u.blend==6?uint32_t((ba*fa+128)>>8)<<24:bg&0xff000000u;
        for(unsigned sh: {16u,8u,0u}){int f=(bg>>sh)&255,c=(u.blendColor>>sh)&255,t=(fg>>sh)&255;int v=f+sar(uint32_t((c-f)*t+128),8);out|=uint32_t(v&255)<<sh;}
        return out;
    }
    if(u.blend==8){
        uint32_t f=bg&m,t=fg&m;uint32_t rb=(f+(uint32_t(sar((t-f)*uint32_t(fa),8))&m))&m;
        f=(bg>>8)&m;t=(fg>>8)&m;uint32_t ag=(f+(uint32_t(sar((t-f)*uint32_t(fa),8))&m))&m;
        return (bg&0xff000000u)|((rb|(ag<<8))&0xffffffu);
    }
    uint32_t out=u.blend==9?uint32_t((ba*fa+ba)>>8)<<24:bg&0xff000000u;
    for(unsigned sh:{16u,8u,0u}){int v=int((bg>>sh)&255)*int((fg>>sh)&255);if(u.blend==9)v+=128;out|=uint32_t(v>>8)<<sh;}
    return out;
}
static bool prepare(int cid,float inv,int x,int y,int count,Unit (&u)[4],ArrayStorage*& cs,ArrayStorage*& ct,Value*& paint){
    if(count<1||count>4||!std::isfinite(inv))return false;
    auto* active=stat(cid,0,"ACTIVE_TEXTURE_UNITS","B");paint=stat(cid,1,"paintPixel","I");
    if(!active||active->i!=count||!paint)return false;
    const char* fn[]={"curS","curT","stepS","stepT","sStepX","tStepX","sStepY","tStepY"};
    ArrayStorage* floats[8];int refs[8];
    for(int j=0;j<8;++j){auto* v=stat(cid,4+j,fn[j],"[F");if(!v)return false;refs[j]=v->ref;floats[j]=m3g_array(v->ref,"[F",count);if(!floats[j])return false;for(int k=0;k<j;++k)if(refs[k]==refs[j])return false;}
    cs=floats[0];ct=floats[1];
    auto* dx=stat(cid,2,"dwdx","F");auto* dy=stat(cid,3,"dwdy","F");if(!dx||!dy)return false;
    float wx=bits_f(dx->i),wy=bits_f(dy->i);if(!std::isfinite(wx)||!std::isfinite(wy))return false;
    const char* rn[]={"textures","texWrappers","texFilter","mipModes","texBlenders","texRepeatS","texRepeatT"};
    const char* rd[]={"[Ljavax/microedition/m3g/Texture2D;","[Ljavax/microedition/m3g/Graphics3DPipelines$TextureWrapper;","[Ljavax/microedition/m3g/Graphics3D$TextureFilter;","[Ljavax/microedition/m3g/Graphics3DPipelines$MipmapMode;","[Ljavax/microedition/m3g/Graphics3DPipelines$TextureBlender;","[Z","[Z"};
    ArrayStorage* arrays[7];for(int j=0;j<7;++j){auto* v=stat(cid,12+j,rn[j],rd[j]);arrays[j]=v?m3g_array(v->ref,rd[j],count):nullptr;if(!arrays[j])return false;}
    const char* wraps[]={"POT_REPEAT","CLAMP","NPOT_REPEAT"};const char* filters[]={"NEAREST","BILINEAR"};const char* mips[]={"NEAREST","LINEAR"};
    const char* blends[]={"PASSTHROUGH","REPLACE_ALPHA","ADD_RGB","ADD_RGBA","ALPHA_MUL","BLEND_RGBA","BLEND_RGB","DECAL_RGBA","MODULATE_RGBA","MODULATE_RGB"};
    for(int i=0;i<count;++i){
        const int texture=arrays[0]->get(i).ref;int image=m3g_ref(texture,"texImage","Ljavax/microedition/m3g/Image2D;");if(!exact_class(texture,"javax/microedition/m3g/Texture2D")||!exact_class(image,"javax/microedition/m3g/Image2D"))return false;
        u[i].wrapper=kind(arrays[1]->get(i).ref,"javax/microedition/m3g/Graphics3DPipelines$TextureWrappers","Ljavax/microedition/m3g/Graphics3DPipelines$TextureWrapper;",wraps,3);
        int filter=kind(arrays[2]->get(i).ref,"javax/microedition/m3g/Graphics3D$TextureFilters","Ljavax/microedition/m3g/Graphics3D$TextureFilter;",filters,2);
        int mip=kind(arrays[3]->get(i).ref,"javax/microedition/m3g/Graphics3DPipelines$MipmapModes","Ljavax/microedition/m3g/Graphics3DPipelines$MipmapMode;",mips,2);
        u[i].blend=kind(arrays[4]->get(i).ref,"javax/microedition/m3g/Graphics3DPipelines$TextureBlenders","Ljavax/microedition/m3g/Graphics3DPipelines$TextureBlender;",blends,10);
        if(u[i].wrapper<=0||filter<=0||mip<0||u[i].blend<0)return false;
        u[i].linear=filter==2;u[i].rs=arrays[5]->get(i).i!=0;u[i].rt=arrays[6]->get(i).i!=0;
        int color;if(!scalar(texture,"blendColor","I",color))return false;u[i].blendColor=uint32_t(color);
        const float s0=bits_f(cs->get(i).i),t0=bits_f(ct->get(i).i);
        u[i].s=mul(s0,inv);u[i].t=mul(t0,inv);
        u[i].nextS=add(s0,bits_f(floats[2]->get(i).i));u[i].nextT=add(t0,bits_f(floats[3]->get(i).i));
        if(!bounded(u[i].s)||!bounded(u[i].t)||!std::isfinite(u[i].nextS)||!std::isfinite(u[i].nextT))return false;
        if(mip){
            float dsdx=mul(sub(bits_f(floats[4]->get(i).i),mul(u[i].s,wx)),inv),dtdx=mul(sub(bits_f(floats[5]->get(i).i),mul(u[i].t,wx)),inv);
            float dsdy=mul(sub(bits_f(floats[6]->get(i).i),mul(u[i].s,wy)),inv),dtdy=mul(sub(bits_f(floats[7]->get(i).i),mul(u[i].t,wy)),inv);
            float measure;
            if(mip==1){float xx=add(mul(dsdx,dsdx),mul(dtdx,dtdx)),yy=add(mul(dsdy,dsdy),mul(dtdy,dtdy));measure=xx>yy?xx:yy;}
            else measure=sub(mul(dsdx,dtdy),mul(dtdx,dsdy));
            if(!std::isfinite(measure))return false;
            uint32_t raw=uint32_t(f_bits(measure));if(mip==2)raw&=0x7fffffffu;raw-=0x3f800000u;
            const uint32_t nonneg=raw&~uint32_t(sar(raw,31));int lod=int(nonneg>>24);
            if(mip==2){int frac=int((raw>>16)&255)&~sar(raw,31);int threshold=int((uint32_t(x)*131u+uint32_t(y)*197u)&255);lod-=sar(uint32_t(threshold-frac),31);}
            int mref=m3g_ref(texture,"mipmaps","[Ljavax/microedition/m3g/Image2D;");auto* levels=m3g_array(mref,"[Ljavax/microedition/m3g/Image2D;",1);
            if(!levels||levels->size()>32||lod<0)return false;
            const int target=levels->get(std::min<size_t>(size_t(lod),levels->size()-1)).ref;
            if(!exact_class(target,"javax/microedition/m3g/Image2D"))return false;
            int baseShift,targetShift;
            if(!scalar(image,"widthShift","I",baseShift)||!scalar(target,"widthShift","I",targetShift))return false;
            int shift=baseShift-targetShift;if(shift<0||shift>30)return false;
            u[i].s=float(sar(uint32_t(int32_t(u[i].s)),unsigned(shift)));u[i].t=float(sar(uint32_t(int32_t(u[i].t)),unsigned(shift)));
            image=target;u[i].mip=true;
        }
        int pot;
        if(!scalar(image,"width","I",u[i].w)||!scalar(image,"height","I",u[i].h)||!scalar(image,"isPOT","Z",pot)||!scalar(image,"widthShift","I",u[i].shift))return false;
        if(u[i].w<=0||u[i].h<=0||u[i].w>1024||u[i].h>1024)return false;
        if(pot&&(u[i].shift<0||u[i].shift>10||(1<<u[i].shift)!=u[i].w))return false;
        u[i].pixels=m3g_array(m3g_ref(image,"image","[I"),"[I",size_t(u[i].w)*u[i].h);if(!u[i].pixels)return false;
        // Restrict NPOT fast reduction to its documented source coordinate domain.
        if(u[i].wrapper==3&&(u[i].s<-32000||u[i].s>32000||u[i].t<-32000||u[i].t>32000))return false;
    }
    return true;
}
} // namespace m3g_texture238
static bool m3g_apply_texture238(int cid,float inv,int x,int y,int count){
    if(!VMS_RS(g_m3g_texture238_enabled))return false;
    m3g_texture238::Unit units[4];ArrayStorage *s=nullptr,*t=nullptr;Value* paint=nullptr;
    if(!m3g_texture238::prepare(cid,inv,x,y,count,units,s,t,paint)){++VMS_RS(g_m3g_texture238_declined);return false;}
    uint32_t out=uint32_t(paint->i);
    for(int i=0;i<count;++i)out=m3g_texture238::blend(out,m3g_texture238::sample(units[i]),units[i]);
    // Atomic from Java's perspective: all eligibility checks have finished.
    *paint=Value::of_i(int_bits(out));
    for(int i=0;i<count;++i){s->set(i,Value::of_i(f_bits(units[i].nextS)));t->set(i,Value::of_i(f_bits(units[i].nextT)));VMS_RS(g_m3g_texture238_mips)+=units[i].mip;}
    ++VMS_RS(g_m3g_texture238_calls);VMS_RS(g_m3g_texture238_units)+=count;return true;
}
