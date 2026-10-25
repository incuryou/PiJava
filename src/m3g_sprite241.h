#pragma once
#include "vm_rs_fallback.h"
// SPDX-License-Identifier: GPL-3.0-or-later
// Exact bounded port of the bundled Graphics3D.renderSprite pixel loop.
// Geometry, crop/flip, clipping and fog setup stay in Java. This intentionally
// retains the reference's Sprite-specific blending/write-mask behavior.
 VMS_RS_ACCESSOR(g_m3g_sprite241_enabled, bool)
namespace m3g_sprite241 {
constexpr int MaxPixels=1024;
enum Flags { DepthTest=1, DepthWrite=2, Fog=4 };
enum Decline { Shape, Callback, Alias, Coordinate, DeclineCount };
struct Stats { uint64_t attempts=0,calls=0,pixels=0,samples=0,depth_rejects=0,alpha_rejects=0,color_writes=0,depth_writes=0;
    uint64_t declines[DeclineCount]={},timed=0,sample_us=0,max_us=0; };
 VMS_RS_NSREF(vms_ref_stats, stats, Stats)
struct Input {int image=0,index=0,count=0,tex_row=0,left=0,width=0;float u=0,du=0;
    int z=0,flags=0,alpha=256,threshold=0,fog=255,fog_rb=0,fog_g=0;};
static bool draw(int g,int cid,const Input& in){
    if(!VMS_RS(g_m3g_sprite241_enabled))return false;
    bool timed=(++stats.attempts&63u)==1u;int64_t begin=timed?paint_clock_us():0;
    int reason=Shape;auto decline=[&](){++stats.declines[reason];return false;};
    using namespace m3g_texture238;
    if(in.count<1||in.count>MaxPixels||in.index<0||in.left<0||in.width<1||in.tex_row<0||
       in.flags<0||(in.flags&~7)||in.alpha<0||in.alpha>256||in.threshold<0||in.threshold>255||
       in.z<-32768||in.z>32767)return decline();
    if(!exact_class(g,"javax/microedition/m3g/Graphics3D")||!exact_class(in.image,"javax/microedition/m3g/Image2D")){reason=Callback;return decline();}
    auto* dst=m3g_array(m3g_ref(g,"rasterData","[I"),"[I",size_t(in.index)+in.count);
    auto* src=m3g_array(m3g_ref(in.image,"image","[I"),"[I",0);
    auto* depth=(in.flags&(DepthTest|DepthWrite))?m3g_array(m3g_ref(g,"depthBuffer","[S"),"[S",size_t(in.index)+in.count):nullptr;
    Value* paint=stat(cid,1,"paintPixel","I");
    if(!dst||!src||!paint||((in.flags&(DepthTest|DepthWrite))&&!depth))return decline();
    if(src==dst){reason=Alias;return decline();}
    int width,height;if(!m3g_int(in.image,"width","I",width)||!m3g_int(in.image,"height","I",height))return decline();
    if(width<1||height<1||width>2048||height>2048||int64_t(in.left)+in.width>width||
       in.tex_row%width||int64_t(in.tex_row)/width>=height||uint64_t(in.tex_row)+in.left+in.width>src->size())return decline();
    const char* names[]={"ALPHA","ALPHA_ADD","MODULATE","MODULATE_X2","PASSTHROUGH"};
    const int comp=kind(m3g_ref(g,"compBlender","Ljavax/microedition/m3g/Graphics3DPipelines$CompositingBlender;"),
        "javax/microedition/m3g/Graphics3DPipelines$CompositingBlenders","Ljavax/microedition/m3g/Graphics3DPipelines$CompositingBlender;",names,5);
    if(comp<0){reason=Callback;return decline();}
    uint32_t output[MaxPixels];bool writes[MaxPixels]={};
    uint32_t pixel=uint32_t(paint->i);float u=in.u;
    uint64_t dr=0,ar=0,ts=0,cw=0;
    for(int i=0;i<in.count;++i,u=add(u,in.du)){
        if((in.flags&DepthTest)&&depth->get(size_t(in.index)+i).i<in.z){++dr;continue;}
        // Conservative finite domain: no UB converting Java NaNs/infinities.
        // Late rejection discards the whole row, including paintPixel.
        if(!bounded(u)){reason=Coordinate;return decline();}
        int64_t x=int64_t(in.left)+int32_t(u);
        x=std::max<int64_t>(in.left,std::min<int64_t>(int64_t(in.left)+in.width-1,x));
        pixel=uint32_t(src->get(size_t(in.tex_row)+size_t(x)).i);++ts;
        int alpha=int(((pixel>>24)*uint32_t(in.alpha))>>8);
        if(alpha<in.threshold||alpha==0){++ar;continue;}
        if(in.flags&Fog){uint32_t rb=pixel&0x00ff00ffu,gr=(pixel>>8)&255u;
            uint32_t out_rb=(uint32_t(in.fog_rb)+uint32_t(sar((rb-uint32_t(in.fog_rb))*uint32_t(in.fog),8)))&0x00ff00ffu;
            uint32_t out_g=uint32_t(in.fog_g)+uint32_t(sar((gr-uint32_t(in.fog_g))*uint32_t(in.fog),8));
            pixel=(pixel&0xff000000u)|out_rb|(out_g<<8);}
        output[i]=m3g_span239::composite(uint32_t(dst->get(size_t(in.index)+i).i),pixel,alpha,comp);
        writes[i]=true;++cw;
    }
    for(int i=0;i<in.count;++i)if(writes[i]){
        dst->set(size_t(in.index)+i,Value::of_i(int_bits(output[i])));
        if(in.flags&DepthWrite)depth->set(size_t(in.index)+i,Value::of_i(in.z));
    }
    paint->i=int_bits(pixel);++stats.calls;stats.pixels+=in.count;stats.samples+=ts;
    stats.depth_rejects+=dr;stats.alpha_rejects+=ar;stats.color_writes+=cw;if(in.flags&DepthWrite)stats.depth_writes+=cw;
    if(timed){uint64_t t=uint64_t(std::max<int64_t>(0,paint_clock_us()-begin));++stats.timed;stats.sample_us+=t;stats.max_us=std::max(stats.max_us,t);}
    return true;
}
static void report(){log_line("M3GSPRITE schema=1 enabled="+std::to_string(VMS_RS(g_m3g_sprite241_enabled))+
    " attempts="+std::to_string(stats.attempts)+" calls="+std::to_string(stats.calls)+" pixels="+std::to_string(stats.pixels)+
    " texture_samples="+std::to_string(stats.samples)+" depth_rejects="+std::to_string(stats.depth_rejects)+" alpha_rejects="+std::to_string(stats.alpha_rejects)+
    " color_writes="+std::to_string(stats.color_writes)+" depth_writes="+std::to_string(stats.depth_writes)+
    " decline_shape="+std::to_string(stats.declines[Shape])+" decline_callback="+std::to_string(stats.declines[Callback])+
    " decline_alias="+std::to_string(stats.declines[Alias])+" decline_coordinate="+std::to_string(stats.declines[Coordinate])+
    " timed_calls="+std::to_string(stats.timed)+" sample_us="+std::to_string(stats.sample_us)+" max_sample_us="+std::to_string(stats.max_us)+
    " sample_period=64 bounded_pixels=1024 scope=process_cumulative");}
}
