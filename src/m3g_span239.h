#include "vm_rs_fallback.h"
#ifndef J2ME_M3G_SPAN239_H
#define J2ME_M3G_SPAN239_H
// SPDX-License-Identifier: GPL-3.0-or-later
// Pixel arithmetic is translated from the bundled FreeJ2ME Java pipeline.
// Bounded port of Graphics3D's existing scanline pixel body, not a new triangle
// rasterizer. At most 8 pixels / 4 texture units. No Java allocation, callbacks,
// yield, GC or publication. Heap pointers live only for this invocation.
// Prepare and render to native scratch, then commit; a decline changes no Java
// state. In particular, a texture which aliases the destination must decline.
#ifndef J2ME_OPT_M3G_SPAN239
#define J2ME_OPT_M3G_SPAN239 1
#endif
 VMS_RS_ACCESSOR(g_m3g_span239_enabled, bool)
namespace m3g_span239 {
using m3g_texture238::add;
using m3g_texture238::sub;
using m3g_texture238::mul;
using m3g_texture238::sar;
using m3g_texture238::stat;
using m3g_texture238::kind;
using m3g_texture238::exact_class;
using m3g_texture238::bounded;
constexpr int MaxPixels = 8;
constexpr int MaxUnits = 4;
constexpr int MaxLevels = 32;
enum Flags { Colors=1, Texture=2, DepthWrite=4, ColorWrite=8, Dither=16, Fog=32 };
enum Decline { Shape=0, Callback=1, Alias=2, Coordinate=3, DeclineCount=4 };
struct Stats {
    uint64_t attempts=0, calls=0, pixels=0, texture_samples=0, mip_samples=0;
    uint64_t depth_rejects=0, alpha_rejects=0, color_writes=0, depth_writes=0;
    uint64_t declines[DeclineCount]={};
    uint64_t timed_calls=0, prepare_us=0, raster_us=0, commit_us=0, max_sample_us=0;
};
 VMS_RS_NSREF(vms_ref_stats, stats, Stats)
struct Input {
    int index=0, x=0, y=0, count=0, color=0, flags=0, alpha_threshold=0;
    float z=0, z_step=0, pw=0, pw_step=0, inv_pw=1, inv_pw_step=0;
    int fog=0, fog_step=0, fog_rb=0, fog_g=0;
};
struct Level {
    ArrayStorage* pixels=nullptr;
    int w=0, h=0, shift=0;
};
struct TextureUnit {
    m3g_texture238::Unit sampler;
    Level base, levels[MaxLevels];
    int mip=0, level_count=0;
    float s=0,t=0,ds=0,dt=0,sx=0,tx=0,sy=0,ty=0;
};
struct TextureBatch {
    TextureUnit units[MaxUnits];
    ArrayStorage *cur_s=nullptr,*cur_t=nullptr;
    int count=0;
    float wx=0,wy=0;
};
static bool level(int ref, ArrayStorage* target, Level& out, int& reason) {
    if(!exact_class(ref,"javax/microedition/m3g/Image2D")) {reason=Callback;return false;}
    int pot;
    if(!m3g_int(ref,"width","I",out.w)||!m3g_int(ref,"height","I",out.h)||
       !m3g_int(ref,"widthShift","I",out.shift)||!m3g_int(ref,"isPOT","Z",pot))return false;
    // Same conservative domain as the s1 texture island; larger images fall back.
    if(out.w<1||out.h<1||out.w>1024||out.h>1024)return false;
    if(pot&&(out.shift<0||out.shift>10||(1<<out.shift)!=out.w))return false;
    out.pixels=m3g_array(m3g_ref(ref,"image","[I"),"[I",size_t(out.w)*out.h);
    if(!out.pixels)return false;
    if(out.pixels==target){reason=Alias;return false;}
    return true;
}
static bool prepare_texture(int cid,int count,ArrayStorage* target,TextureBatch& b,int& reason) {
    if(count<1||count>MaxUnits)return false;
    b.count=count;
    const char* fn[]={"curS","curT","stepS","stepT","sStepX","tStepX","sStepY","tStepY"};
    ArrayStorage* floats[8];int refs[8];
    for(int j=0;j<8;++j){
        auto* v=stat(cid,4+j,fn[j],"[F");if(!v)return false;
        refs[j]=v->ref;floats[j]=m3g_array(refs[j],"[F",count);if(!floats[j])return false;
        for(int k=0;k<j;++k)if(refs[k]==refs[j]){reason=Alias;return false;}
    }
    b.cur_s=floats[0];b.cur_t=floats[1];
    auto* dx=stat(cid,2,"dwdx","F");auto* dy=stat(cid,3,"dwdy","F");if(!dx||!dy)return false;
    b.wx=bits_f(dx->i);b.wy=bits_f(dy->i);
    if(!std::isfinite(b.wx)||!std::isfinite(b.wy)){reason=Coordinate;return false;}
    const char* rn[]={"textures","texWrappers","texFilter","mipModes","texBlenders","texRepeatS","texRepeatT"};
    const char* rd[]={"[Ljavax/microedition/m3g/Texture2D;","[Ljavax/microedition/m3g/Graphics3DPipelines$TextureWrapper;","[Ljavax/microedition/m3g/Graphics3D$TextureFilter;","[Ljavax/microedition/m3g/Graphics3DPipelines$MipmapMode;","[Ljavax/microedition/m3g/Graphics3DPipelines$TextureBlender;","[Z","[Z"};
    ArrayStorage* arrays[7];
    for(int j=0;j<7;++j){auto* v=stat(cid,12+j,rn[j],rd[j]);arrays[j]=v?m3g_array(v->ref,rd[j],count):nullptr;if(!arrays[j])return false;}
    const char* wraps[]={"POT_REPEAT","CLAMP","NPOT_REPEAT"};
    const char* filters[]={"NEAREST","BILINEAR"};const char* mips[]={"NEAREST","LINEAR"};
    const char* blends[]={"PASSTHROUGH","REPLACE_ALPHA","ADD_RGB","ADD_RGBA","ALPHA_MUL","BLEND_RGBA","BLEND_RGB","DECAL_RGBA","MODULATE_RGBA","MODULATE_RGB"};
    for(int i=0;i<count;++i){
        auto& u=b.units[i];const int tr=arrays[0]->get(i).ref;
        if(!exact_class(tr,"javax/microedition/m3g/Texture2D")){reason=Callback;return false;}
        auto& s=u.sampler;
        s.wrapper=kind(arrays[1]->get(i).ref,"javax/microedition/m3g/Graphics3DPipelines$TextureWrappers","Ljavax/microedition/m3g/Graphics3DPipelines$TextureWrapper;",wraps,3);
        int filter=kind(arrays[2]->get(i).ref,"javax/microedition/m3g/Graphics3D$TextureFilters","Ljavax/microedition/m3g/Graphics3D$TextureFilter;",filters,2);
        u.mip=kind(arrays[3]->get(i).ref,"javax/microedition/m3g/Graphics3DPipelines$MipmapModes","Ljavax/microedition/m3g/Graphics3DPipelines$MipmapMode;",mips,2);
        s.blend=kind(arrays[4]->get(i).ref,"javax/microedition/m3g/Graphics3DPipelines$TextureBlenders","Ljavax/microedition/m3g/Graphics3DPipelines$TextureBlender;",blends,10);
        if(s.wrapper<=0||filter<=0||u.mip<0||s.blend<0){reason=Callback;return false;}
        s.linear=filter==2;s.rs=arrays[5]->get(i).i!=0;s.rt=arrays[6]->get(i).i!=0;
        int color;if(!m3g_int(tr,"blendColor","I",color))return false;s.blendColor=uint32_t(color);
        float* fs[]={&u.s,&u.t,&u.ds,&u.dt,&u.sx,&u.tx,&u.sy,&u.ty};
        for(int j=0;j<8;++j){*fs[j]=bits_f(floats[j]->get(i).i);if(!std::isfinite(*fs[j])){reason=Coordinate;return false;}}
        if(!level(m3g_ref(tr,"texImage","Ljavax/microedition/m3g/Image2D;"),target,u.base,reason))return false;
        if(u.mip){
            auto* ls=m3g_array(m3g_ref(tr,"mipmaps","[Ljavax/microedition/m3g/Image2D;"),"[Ljavax/microedition/m3g/Image2D;",1);
            if(!ls||ls->size()>MaxLevels)return false;
            u.level_count=int(ls->size());
            for(int j=0;j<u.level_count;++j){
                if(!level(ls->get(j).ref,target,u.levels[j],reason))return false;
                const int shift=u.base.shift-u.levels[j].shift;
                if(shift<0||shift>30)return false;
            }
        }
    }
    return true;
}
static bool sample_unit(TextureUnit& u,const TextureBatch& b,float inv,int x,int y,uint32_t& paint) {
    auto s=u.sampler;s.s=mul(u.s,inv);s.t=mul(u.t,inv);
    if(!bounded(s.s)||!bounded(s.t))return false;
    const Level* image=&u.base;
    if(u.mip){
        float dx=mul(sub(u.sx,mul(s.s,b.wx)),inv),tx=mul(sub(u.tx,mul(s.t,b.wx)),inv);
        float dy=mul(sub(u.sy,mul(s.s,b.wy)),inv),ty=mul(sub(u.ty,mul(s.t,b.wy)),inv);
        float measure;
        if(u.mip==1){float xx=add(mul(dx,dx),mul(tx,tx)),yy=add(mul(dy,dy),mul(ty,ty));measure=xx>yy?xx:yy;}
        else measure=sub(mul(dx,ty),mul(tx,dy));
        if(!std::isfinite(measure))return false;
        uint32_t raw=uint32_t(f_bits(measure));if(u.mip==2)raw&=0x7fffffffu;raw-=0x3f800000u;
        int lod=int((raw&~uint32_t(sar(raw,31)))>>24);
        if(u.mip==2){int frac=int((raw>>16)&255)&~sar(raw,31);int threshold=int((uint32_t(x)*131u+uint32_t(y)*197u)&255);lod-=sar(uint32_t(threshold-frac),31);}
        if(lod<0)return false;
        image=&u.levels[std::min(lod,u.level_count-1)];
        const unsigned shift=unsigned(u.base.shift-image->shift);
        s.s=float(sar(uint32_t(int32_t(s.s)),shift));s.t=float(sar(uint32_t(int32_t(s.t)),shift));
    }
    if(s.wrapper==3&&(s.s<-32000||s.s>32000||s.t<-32000||s.t>32000))return false;
    s.w=image->w;s.h=image->h;s.pixels=image->pixels;
    paint=m3g_texture238::blend(paint,m3g_texture238::sample(s),s);
    return true;
}
// Java f2i then i2s, including saturation, NaN and short wrap. No C++ UB.
static int depth_short(float f) {
    int32_t i=std::isnan(f)?0:f>=2147483648.0f?INT32_MAX:f<=-2147483648.0f?INT32_MIN:int32_t(f);
    int v=int(uint32_t(i)&65535u);return v>=32768?v-65536:v;
}
static uint32_t composite(uint32_t bg,uint32_t fg,int alpha,int mode) {
    constexpr uint32_t m=0x00ff00ffu;
    if(mode==0)return fg;
    if(mode==5)return bg;
    if(mode==1){
        if(alpha<=0)return bg;if(alpha>=255)return fg;
        uint32_t inv=255-alpha;
        uint32_t rb=(((fg&m)*uint32_t(alpha)+(bg&m)*inv)>>8)&m;
        uint32_t g=((((fg>>8)&255)*uint32_t(alpha)+((bg>>8)&255)*inv)>>8)&255;
        uint32_t a=uint32_t(alpha)+(((bg>>24)*inv)>>8);
        return (a<<24)|(g<<8)|rb;
    }
    if(mode==2){
        if(alpha==0)return bg;
        uint32_t rb=(bg&m)+((((fg&m)*uint32_t(alpha))>>8)&m);
        uint32_t g=(bg&0xff00u)+((((fg&0xff00u)*uint32_t(alpha))>>8)&0xff00u);
        uint32_t a=(bg>>24)+((uint32_t(alpha)*(255-(bg>>24)))>>8);
        uint32_t over=rb&0x01000100u;rb=(rb|(over-(over>>8)))&m;
        over=g&0x10000u;g=(g|(over-(over>>8)))&0xff00u;
        a=(a-(0u))|(0u-(a>>8));return ((a&255)<<24)|rb|g;
    }
    uint32_t out=0;
    for(unsigned sh:{24u,16u,8u,0u}){
        uint32_t v=(((bg>>sh)&255)*((fg>>sh)&255))>>(mode==4?7:8);
        if(mode==4)v=(v|(0u-(v>>8)))&255;
        out|=v<<sh;
    }
    return out;
}
static bool draw(int g,int cid,const Input& in) {
    if(!VMS_RS(g_m3g_span239_enabled))return false;
    const bool timed=(++stats.attempts&63u)==1u;
    int64_t start=timed?paint_clock_us():0,prepared=0,rendered=0;
    int reason=Shape;
    auto decline=[&](){++stats.declines[reason];return false;};
    if(in.count<1||in.count>MaxPixels||in.index<0||in.x<0||in.x>1023||in.y<0||in.y>1023||
       in.count>MaxPixels-(in.x&7)||in.flags<0||(in.flags&~63))return decline();
    if(!exact_class(g,"javax/microedition/m3g/Graphics3D")){reason=Callback;return decline();}
    auto* pixels=m3g_array(m3g_ref(g,"rasterData","[I"),"[I",size_t(in.index)+in.count);
    auto* depths=m3g_array(m3g_ref(g,"depthBuffer","[S"),"[S",size_t(in.index)+in.count);
    Value* paint=stat(cid,1,"paintPixel","I");
    if(!pixels||!depths||!paint)return decline();
    // Scalars handed back are the LAST pixel's input values (before the Java
    // for-loop's update). Texture/color deltas, pixels and depth are POST-span.
    Value* oz=m3g_field(g,"spanZ239","F"),*opw=m3g_field(g,"spanPw239","F");
    Value* oi=m3g_field(g,"spanInvPw239","F"),*of=m3g_field(g,"spanFog239","I");
    if(!oz||!opw||!oi||!of)return decline();
    int alpha_factor;if(!m3g_int(g,"currAlphaFactor","I",alpha_factor)||alpha_factor<0||alpha_factor>256)return decline();
    const char* cn[]={"ALPHA","ALPHA_ADD","MODULATE","MODULATE_X2","PASSTHROUGH"};
    const int comp=kind(m3g_ref(g,"compBlender","Ljavax/microedition/m3g/Graphics3DPipelines$CompositingBlender;"),"javax/microedition/m3g/Graphics3DPipelines$CompositingBlenders","Ljavax/microedition/m3g/Graphics3DPipelines$CompositingBlender;",cn,5);
    const char* tn[]={"NO_UNIT","SINGLE_UNIT","MULTI_UNIT"};
    const int tex=kind(m3g_ref(g,"texMode","Ljavax/microedition/m3g/Graphics3D$TexturingMode;"),"javax/microedition/m3g/Graphics3D$TexturingModes","Ljavax/microedition/m3g/Graphics3D$TexturingMode;",tn,3);
    if(comp<0||tex<=0){reason=Callback;return decline();}
    TextureBatch batch;
    if(in.flags&Texture){
        auto* active=stat(cid,0,"ACTIVE_TEXTURE_UNITS","B");if(!active)return decline();
        if((tex==2&&active->i!=1)||(tex==3&&(active->i<2||active->i>4))||tex==1){reason=Callback;return decline();}
        if(!prepare_texture(cid,active->i,pixels,batch,reason))return decline();
    }else if(tex!=1){reason=Callback;return decline();}
    Value* delta[4]={};uint32_t colors[4]={},steps[4]={};
    const char* dn[]={"deltaA","deltaR","deltaG","deltaB"};const char* sn[]={"stepA","stepR","stepG","stepB"};
    if(in.flags&Colors)for(int k=0;k<4;++k){
        delta[k]=m3g_field(g,dn[k],"I");auto* s=m3g_field(g,sn[k],"I");if(!delta[k]||!s)return decline();
        colors[k]=uint32_t(delta[k]->i);steps[k]=uint32_t(s->i);
    }
    if(timed)prepared=paint_clock_us();
    uint32_t output[MaxPixels]={};int z_out[MaxPixels]={};
    bool color_write[MaxPixels]={},depth_write[MaxPixels]={};
    uint32_t p=uint32_t(paint->i),fog=uint32_t(in.fog);
    float z=in.z,pw=in.pw,inv=in.inv_pw;
    uint64_t dr=0,ar=0,ts=0,ms=0,cw=0,dw=0;
    for(int k=0;k<in.count;++k){
        if(k){z=add(z,in.z_step);pw=add(pw,in.pw_step);inv=add(inv,in.inv_pw_step);fog+=uint32_t(in.fog_step);}
        const int depth=depth_short(z);
        const bool rejected=depths->get(size_t(in.index)+k).i<depth;
        if(in.flags&Colors){
            if(!rejected){p=0;for(int c=0;c<4;++c)p|=((colors[c]>>16)&255)<<(24-c*8);}
            for(int c=0;c<4;++c)colors[c]+=steps[c];
        }else if(!rejected)p=uint32_t(in.color);
        if(batch.count){
            for(int unit=0;unit<batch.count;++unit){
                auto& u=batch.units[unit];
                if(!rejected){
                    if(!sample_unit(u,batch,inv,in.x+k,in.y,p)){reason=Coordinate;return decline();}
                    ++ts;if(u.mip)++ms;
                }
                u.s=add(u.s,u.ds);u.t=add(u.t,u.dt);
                if(!std::isfinite(u.s)||!std::isfinite(u.t)){reason=Coordinate;return decline();}
            }
        }
        if(rejected){++dr;continue;}
        const int alpha=int(((p>>24)*uint32_t(alpha_factor))>>8);
        if(alpha<in.alpha_threshold){++ar;continue;}
        if(alpha==0)p|=0xff000000u;
        if(in.flags&DepthWrite){depth_write[k]=true;z_out[k]=depth;++dw;}
        if(!(in.flags&ColorWrite))continue;
        if(in.flags&Fog){
            uint32_t amount=uint32_t(sar(fog,8));
            uint32_t rb=(uint32_t(in.fog_rb)+uint32_t(sar(((p&0x00ff00ffu)-uint32_t(in.fog_rb))*amount,8)))&0x00ff00ffu;
            uint32_t gg=uint32_t(in.fog_g)+uint32_t(sar((((p>>8)&255)-uint32_t(in.fog_g))*amount,8));
            p=(p&0xff000000u)|rb|(gg<<8);
        }
        if(in.flags&Dither){
            uint32_t x=uint32_t(in.x+k),y=uint32_t(in.y);
            int d=int((((x^y)&1)<<3)|((y&1)<<2)|(((x^y)&2)<<1)|((y&2)>>1));
            uint32_t out=p&0xff000000u;
            for(unsigned sh:{16u,8u,0u}){int v=int((p>>sh)&255)+d;v=(v|sar(uint32_t(255-v),31))&255;out|=uint32_t(v)<<sh;}
            p=out;
        }
        output[k]=composite(uint32_t(pixels->get(size_t(in.index)+k).i),p,alpha,comp);color_write[k]=true;++cw;
    }
    if(timed)rendered=paint_clock_us();
    // No operation below can fail or invoke Java. All output locations and all
    // read dependencies were validated, and native scratch is now complete.
    for(int k=0;k<in.count;++k){
        if(depth_write[k])depths->set(size_t(in.index)+k,Value::of_i(z_out[k]));
        if(color_write[k])pixels->set(size_t(in.index)+k,Value::of_i(int_bits(output[k])));
    }
    *paint=Value::of_i(int_bits(p));
    if(in.flags&Colors)for(int k=0;k<4;++k)*delta[k]=Value::of_i(int_bits(colors[k]));
    for(int k=0;k<batch.count;++k){batch.cur_s->set(k,Value::of_i(f_bits(batch.units[k].s)));batch.cur_t->set(k,Value::of_i(f_bits(batch.units[k].t)));}
    *oz=Value::of_i(f_bits(z));*opw=Value::of_i(f_bits(pw));*oi=Value::of_i(f_bits(inv));*of=Value::of_i(int_bits(fog));
    ++stats.calls;stats.pixels+=in.count;stats.texture_samples+=ts;stats.mip_samples+=ms;
    stats.depth_rejects+=dr;stats.alpha_rejects+=ar;stats.color_writes+=cw;stats.depth_writes+=dw;
    if(timed){const int64_t end=paint_clock_us();++stats.timed_calls;
        stats.prepare_us+=uint64_t(prepared-start);stats.raster_us+=uint64_t(rendered-prepared);stats.commit_us+=uint64_t(end-rendered);
        stats.max_sample_us=std::max(stats.max_sample_us,uint64_t(end-start));}
    return true;
}
} // namespace m3g_span239
#endif
