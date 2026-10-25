#include "vm_rs_fallback.h"
#ifndef J2ME_M3G_ROW240_H
#define J2ME_M3G_ROW240_H
// SPDX-License-Identifier: GPL-3.0-or-later
// Stage240: one bounded scanline invocation, one prepare, one atomic commit.
// Arithmetic translated from bundled FreeJ2ME Graphics3D and M3GMath.
// The unchanged stage239 kernel remains an independent fallback.
// Exact eight-pixel alignment/perspective/fog arithmetic is retained. No FMA,
// reciprocal substitution, resolution reduction, skipped effects or callbacks.
// All heap references are invocation-local; there is no Java allocation, GC,
// suspension, re-entry, persistent pointer cache, or frame publication here.
#ifndef J2ME_OPT_M3G_ROW240
#define J2ME_OPT_M3G_ROW240 1
#endif
 VMS_RS_ACCESSOR(g_m3g_row240_enabled, bool)
namespace m3g_row240 {
using namespace m3g_span239;
constexpr int MaxPixels=1024;
struct Stats : m3g_span239::Stats { uint64_t subspans=0,max_pixels=0; };
 VMS_RS_NSREF(vms_ref_stats, stats, Stats)
struct Input : m3g_span239::Input {
    bool perspective=false;
    int fog_mode=0;
    float fog_far_norm=0,inv_fog_div=0,fog_density=0;
};
// Java f2i is defined for NaN, infinities and overflow; avoid C++ undefined casts.
static int32_t java_f2i(float f){
    return std::isnan(f)?0:f>=2147483648.0f?INT32_MAX:f<=-2147483648.0f?INT32_MIN:int32_t(f);
}
static float java_exp(float f){
    // Intentionally preserve M3GMath.exp(float), not libm expf.
    return bits_f(java_f2i(add(mul(12102203.0f,f),float(1064866805))));
}
static float clamp_fog(float f){return f<0.0f?0.0f:f>255.0f?255.0f:f;}
static bool draw(int g,int cid,const Input& in) {
    if(!VMS_RS(g_m3g_row240_enabled))return false;
    const bool timed=(++stats.attempts&63u)==1u;
    int64_t start=timed?paint_clock_us():0,prepared=0,rendered=0;
    int reason=Shape;
    auto decline=[&](){++stats.declines[reason];return false;};
    if(in.count<1||in.count>MaxPixels||in.index<0||in.x<0||in.x>1023||in.y<0||in.y>1023||
       in.count>MaxPixels-in.x||in.flags<0||(in.flags&~63))return decline();
    if(!exact_class(g,"javax/microedition/m3g/Graphics3D")){reason=Callback;return decline();}
    auto* pixels=m3g_array(m3g_ref(g,"rasterData","[I"),"[I",size_t(in.index)+in.count);
    auto* depths=m3g_array(m3g_ref(g,"depthBuffer","[S"),"[S",size_t(in.index)+in.count);
    Value* paint=stat(cid,1,"paintPixel","I");
    if(!pixels||!depths||!paint)return decline();
    // Fog lives outside the Java y-loop. Return its POST-row value and final
    // subspan step. No native pointer or row-local value survives this call.
    Value* of=m3g_field(g,"rowFog240","I"),*ofs=m3g_field(g,"rowFogStep240","I");
    if(!of||!ofs)return decline();
    float inv_span[9]={};
    ArrayStorage* inv_table=nullptr;
    if(in.perspective){
        auto* table=stat(cid,19,"INV_SPAN_TABLE","[F");
        auto* values=table?m3g_array(table->ref,"[F",9):nullptr;
        if(!values)return decline();
        inv_table=values;
        for(int i=1;i<=8;++i)inv_span[i]=bits_f(values->get(i).i);
    }
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
        if(inv_table && (inv_table==batch.cur_s || inv_table==batch.cur_t)){reason=Alias;return decline();}
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
    float z=in.z,pw=in.pw,inv=in.inv_pw,inv_step=in.inv_pw_step;
    int fog_step=in.fog_step;
    int next_perspective=0;
    uint64_t dr=0,ar=0,ts=0,ms=0,cw=0,dw=0,subspans=0;
    for(int k=0;k<in.count;++k){
        if(k){z=add(z,in.z_step);pw=add(pw,in.pw_step);inv=add(inv,inv_step);fog+=uint32_t(fog_step);}
        if(in.perspective && k==next_perspective){
            const int len=std::min(in.count-k,8-((in.x+k)&7));
            const float denom=mul(pw,add(pw,mul(in.pw_step,float(len))));
            float inv_denom=bits_f(int_bits(0x7ef127eau-uint32_t(f_bits(denom))));
            inv_denom=mul(inv_denom,sub(2.0f,mul(denom,inv_denom)));
            inv_step=mul(-in.pw_step,inv_denom);
            if(in.flags&Fog){
                const float end=add(inv,mul(inv_step,float(len)));
                float fs,fe;
                if(in.fog_mode==81){ // javax.microedition.m3g.Fog.LINEAR
                    fs=mul(sub(in.fog_far_norm,mul(inv,in.inv_fog_div)),256.0f);
                    fe=mul(sub(in.fog_far_norm,mul(end,in.inv_fog_div)),256.0f);
                }else{
                    fs=mul(java_exp(mul(-in.fog_density,inv)),256.0f);
                    fe=mul(java_exp(mul(-in.fog_density,end)),256.0f);
                }
                fog=uint32_t(java_f2i(mul(clamp_fog(fs),256.0f)));
                fog_step=java_f2i(mul(mul(sub(clamp_fog(fe),mul(float(int_bits(fog)),0.00390625f)),inv_span[len]),256.0f));
            }
            next_perspective+=len;++subspans;
        }
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
    *of=Value::of_i(int_bits(fog+uint32_t(fog_step)));*ofs=Value::of_i(fog_step);
    stats.subspans+=subspans;
    stats.max_pixels=std::max(stats.max_pixels,uint64_t(in.count));
    ++stats.calls;stats.pixels+=in.count;stats.texture_samples+=ts;stats.mip_samples+=ms;
    stats.depth_rejects+=dr;stats.alpha_rejects+=ar;stats.color_writes+=cw;stats.depth_writes+=dw;
    if(timed){const int64_t end=paint_clock_us();++stats.timed_calls;
        stats.prepare_us+=uint64_t(prepared-start);stats.raster_us+=uint64_t(rendered-prepared);stats.commit_us+=uint64_t(end-rendered);
        stats.max_sample_us=std::max(stats.max_sample_us,uint64_t(end-start));}
    return true;
}
} // namespace m3g_row240
#endif
