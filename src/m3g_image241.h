#pragma once
#include "vm_rs_fallback.h"
// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded translations of the bundled Image2D and Texture2D reference loops.
// No Java allocation/callback, persistent heap reference, yield or publication.
// Declines are transactional. The Java caller retains validation/exceptions.
 VMS_RS_ACCESSOR(g_m3g_prep241_enabled, bool)
namespace m3g_image241 {
constexpr int MaxPixels=1024;
enum Op { Convert, Decode, Palette, Mipmap, OpCount };
struct Stats { uint64_t attempts[OpCount]={},calls[OpCount]={},pixels[OpCount]={},declines[OpCount]={};
    uint64_t timed=0,sample_us=0,max_us=0; };
 VMS_RS_NSREF(vms_ref_stats, stats, Stats)
struct Probe {
    Op op; int count; bool ok=false,timed; int64_t start;
    Probe(Op o,int n):op(o),count(n),timed((++stats.attempts[o]&63u)==1u),start(timed?paint_clock_us():0){}
    ~Probe(){if(ok){++stats.calls[op];stats.pixels[op]+=count;}else ++stats.declines[op];
        if(timed){uint64_t t=uint64_t(std::max<int64_t>(0,paint_clock_us()-start));++stats.timed;stats.sample_us+=t;stats.max_us=std::max(stats.max_us,t);}}
};
static bool range(ArrayStorage* a,int start,int count){return a&&start>=0&&count>0&&count<=MaxPixels&&uint64_t(start)+count<=a->size();}
static int bpp(int f){return f==96||f==97?1:f==98?2:f==99?3:f==100?4:0;}
static uint32_t decode_pixel(ArrayStorage& a,size_t o,int fmt){
    auto byte=[&](size_t i){return uint32_t(a.get(i).i)&255u;};
    const uint32_t r=byte(o);
    if(fmt==96)return r<<24;
    if(fmt==97||fmt==98)return (fmt==97?0xff000000u:byte(o+1)<<24)|r*0x010101u;
    return (fmt==99?0xff000000u:byte(o+3)<<24)|(r<<16)|(byte(o+1)<<8)|byte(o+2);
}
static bool convert(int data,int start,int count,int fmt){
    if(!VMS_RS(g_m3g_prep241_enabled))return false;Probe p(Convert,count);
    auto* a=m3g_array(data,"[I",0);if(!bpp(fmt)||!range(a,start,count))return false;
    for(int i=0;i<count;++i){uint32_t v=uint32_t(a->get(start+i).i);
        if(fmt==96)v&=0xff000000u;
        else if(fmt==97||fmt==98){uint32_t l=(((v>>16)&255)+((v>>8)&255)+(v&255))/3;
            v=(fmt==97?0xff000000u:v&0xff000000u)|l*0x010101u;}
        else if(fmt==99)v|=0xff000000u;
        a->set(start+i,Value::of_i(int_bits(v)));
    }p.ok=true;return true;
}
static bool decode(int source,int src_offset,int palette,int destination,int dst_offset,int count,int fmt,bool opaque){
    if(!VMS_RS(g_m3g_prep241_enabled))return false;Probe p(palette?Palette:Decode,count);
    auto* src=m3g_array(source,"[B",0);auto* dst=m3g_array(destination,"[I",0);
    const int channels=bpp(fmt);
    if(!channels||!src||src_offset<0||!range(dst,dst_offset,count))return false;
    const uint64_t bytes=uint64_t(count)*(palette?1:channels);
    if(uint64_t(src_offset)+bytes>src->size())return false;
    auto* pal=palette?m3g_array(palette,"[B",0):nullptr;
    if(palette&&!pal)return false;
    // Palette references are checked for the WHOLE block before the first write.
    // Invalid indices fall back to scalar Java, preserving partial-progress and
    // exception order of the reference rather than masking malformed resources.
    if(pal)for(int i=0;i<count;++i){size_t o=(uint32_t(src->get(src_offset+i).i)&255u)*channels;
        if(o+channels>pal->size())return false;}
    for(int i=0;i<count;++i){size_t o=pal?(uint32_t(src->get(src_offset+i).i)&255u)*channels:size_t(src_offset)+size_t(i)*channels;
        uint32_t v=decode_pixel(pal?*pal:*src,o,fmt);if(pal&&opaque)v|=0xff000000u;
        dst->set(dst_offset+i,Value::of_i(int_bits(v)));}
    p.ok=true;return true;
}
static bool mip_row(int source,int destination,int y){
    if(!VMS_RS(g_m3g_prep241_enabled))return false;Probe p(Mipmap,0);
    using m3g_texture238::exact_class;
    if(!exact_class(source,"javax/microedition/m3g/Image2D")||!exact_class(destination,"javax/microedition/m3g/Image2D"))return false;
    int sw,sh,dw,dh,sp,ss,dp,ds;
    if(!m3g_int(source,"width","I",sw)||!m3g_int(source,"height","I",sh)||
       !m3g_int(destination,"width","I",dw)||!m3g_int(destination,"height","I",dh)||
       !m3g_int(source,"isPOT","Z",sp)||!m3g_int(source,"widthShift","I",ss)||
       !m3g_int(destination,"isPOT","Z",dp)||!m3g_int(destination,"widthShift","I",ds))return false;
    if(sw<1||sh<1||sw>2048||sh>2048||dw!=std::max(1,sw>>1)||dh!=std::max(1,sh>>1)||y<0||y>=dh||dw>MaxPixels)return false;
    if((sp&&(ss<0||ss>11||(1<<ss)!=sw))||(dp&&(ds<0||ds>10||(1<<ds)!=dw)))return false;
    auto* src=m3g_array(m3g_ref(source,"image","[I"),"[I",size_t(sw)*sh);
    auto* dst=m3g_array(m3g_ref(destination,"image","[I"),"[I",size_t(dw)*dh);
    if(!src||!dst||src==dst)return false;
    const int sy=y*2,sy2=std::min(sy+1,sh-1);p.count=dw;
    for(int x=0;x<dw;++x){int sx=x*2,sx2=std::min(sx+1,sw-1);
        uint32_t a=uint32_t(src->get(size_t(sy)*sw+sx).i),b=uint32_t(src->get(size_t(sy)*sw+sx2).i);
        uint32_t c=uint32_t(src->get(size_t(sy2)*sw+sx).i),d=uint32_t(src->get(size_t(sy2)*sw+sx2).i),out=0;
        for(unsigned s:{0u,8u,16u,24u})out|=(( ((a>>s)&255)+((b>>s)&255)+((c>>s)&255)+((d>>s)&255)+2)>>2)<<s;
        dst->set(size_t(y)*dw+x,Value::of_i(int_bits(out)));
    }p.ok=true;return true;
}
static void report(){const char* names[]={"convert","decode","palette","mipmap"};
    std::string s="M3GPREP schema=1 enabled="+std::to_string(VMS_RS(g_m3g_prep241_enabled));
    for(int i=0;i<OpCount;++i)s+=" "+std::string(names[i])+"_calls="+std::to_string(stats.calls[i])+" "+names[i]+"_pixels="+std::to_string(stats.pixels[i])+" "+names[i]+"_declines="+std::to_string(stats.declines[i]);
    log_line(s+" timed_calls="+std::to_string(stats.timed)+" sample_us="+std::to_string(stats.sample_us)+" max_sample_us="+std::to_string(stats.max_us)+" sample_period=64 bounded_pixels=1024 scope=process_cumulative");
}
}
