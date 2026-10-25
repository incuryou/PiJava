#pragma once
#include "vm_rs_fallback.h"
// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded gather/cull and emission islands from Triangle.fromVertAndTris.
// Near-plane clipping, strip-history tracking, AA edges and sort remain Java.
namespace m3g_assembly242 {
using namespace m3g_mesh242;
 VMS_RS_NSREF(vms_ref_gathered, gathered, uint64_t) VMS_RS_NSREF(vms_ref_culled, culled, uint64_t) VMS_RS_NSREF(vms_ref_emitted, emitted, uint64_t) VMS_RS_NSREF(vms_ref_declines, declines, uint64_t)
static ArrayStorage* scratch(int cid,int key,const char* name,const char* desc,int minimum){auto* v=m3g_texture238::stat(cid,key,name,desc);return v?m3g_array(v->ref,desc,minimum):nullptr;}
static int gather(int cid,int vr,int tr,int colors,int vb,int ia,int ib,int ic,int cull,bool clockwise,int units){
    if(!VMS_RS(g_m3g_mesh242_enabled))return -1;auto fail=[](){++declines;return -1;};
    if(units<0||units>4||ia<0||ib<0||ic<0||!exact_class(vb,"javax/microedition/m3g/VertexBuffer"))return fail();
    int largest=std::max(ia,std::max(ib,ic));auto* vert=m3g_array(vr,"[F",size_t(largest)*4+4);auto* tex=m3g_array(tr,"[[F",std::max(1,units));
    if(!vert||!tex)return fail();int indices[]={ia,ib,ic};float v[12];for(int n=0;n<3;++n)for(int j=0;j<4;++j)v[n*4+j]=get(vert,size_t(indices[n])*4+j);
    float a=sub(mul(v[0],v[5]),mul(v[1],v[4])),b=sub(mul(v[4],v[9]),mul(v[5],v[8])),c=sub(mul(v[8],v[1]),mul(v[9],v[0]));
    bool ccw=add(add(mul(a,v[11]),mul(b,v[3])),mul(c,v[7]))>0.0f,front=clockwise^ccw;
    if((cull==160&&!front)||(cull==161&&front)){++culled;return 0;}
    for(int axis=0;axis<3;++axis)if((v[axis]<-v[3]&&v[4+axis]<-v[7]&&v[8+axis]<-v[11])||
        (v[axis]>v[3]&&v[4+axis]>v[7]&&v[8+axis]>v[11])){++culled;return 0;}
    auto* inV=scratch(cid,17,"inV","[F",12);auto* inT=scratch(cid,18,"inT","[[F",units);auto* inC=scratch(cid,19,"inC","[I",3);
    if(!inV||!inT||!inC||inV==vert)return fail();
    ArrayStorage* targets[4]={};float t[4][12];bool hasTex=tex->get(0).ref!=0;
    if(hasTex)for(int u=0;u<units;++u){int r=tex->get(u).ref;if(!r)continue;
        auto* src=m3g_array(r,"[F",size_t(largest)*4+4);auto* dst=m3g_array(inT->get(u).ref,"[F",12);
        if(!src||!dst||dst==vert||dst==inV||dst==src)return fail();
        for(int k=0;k<u;++k)if(targets[k]==dst)return fail();targets[u]=dst;
        for(int n=0;n<3;++n)for(int j=0;j<4;++j)t[u][n*4+j]=get(src,size_t(indices[n])*4+j);
    }
    int packed[3];if(colors){ArrayStorage* raw=nullptr;int n,c,type;
        if(!vertex(colors,raw,n,c,type)||type!=1||c<3||largest>=n)return fail();
        for(int j=0;j<3;++j){size_t off=size_t(indices[j])*c;uint32_t p=c==3?0xff000000u:(uint32_t(raw->get(off+3).i)&255u)<<24;
            p|=(uint32_t(raw->get(off).i)&255u)<<16;p|=(uint32_t(raw->get(off+1).i)&255u)<<8;p|=uint32_t(raw->get(off+2).i)&255u;packed[j]=int_bits(p);}}
    else{int color;if(!m3g_int(vb,"defaultColor","I",color))return fail();packed[0]=packed[1]=packed[2]=color;}
    for(int j=0;j<12;++j)put(inV,j,v[j]);for(int j=0;j<3;++j)inC->set(j,Value::of_i(packed[j]));
    for(int u=0;u<units;++u)if(targets[u])for(int j=0;j<12;++j)put(targets[u],j,t[u][j]);
    ++gathered;return front?1:2;
}
static bool emit(int triangle,int vr,int tr,int cr,int fan,bool hasTex,bool hasColors,bool perspective,int units){
    if(!VMS_RS(g_m3g_mesh242_enabled))return false;auto fail=[](){++declines;return false;};
    if(fan<0||fan>1||units<0||units>4||!exact_class(triangle,"javax/microedition/m3g/Triangle"))return fail();
    int offsets[]={0,(fan+1)*4,(fan+2)*4};auto* src=m3g_array(vr,"[F",offsets[2]+4);auto* coords=m3g_array(tr,"[[F",units);auto* colors=m3g_array(cr,"[I",fan+3);
    auto* v=m3g_array(m3g_ref(triangle,"v","[F"),"[F",12);auto* iw=m3g_array(m3g_ref(triangle,"invW","[F"),"[F",3);
    auto* ts=m3g_array(m3g_ref(triangle,"t","[[F"),"[[F",hasTex?units:0);auto* cs=m3g_array(m3g_ref(triangle,"colors","[I"),"[I",3);
    auto* sort=m3g_field(triangle,"sortZ","F");auto* hc=m3g_field(triangle,"hasVertexColors","Z");
    if(!src||!coords||!colors||!v||!iw||!ts||!cs||!sort||!hc||v==iw||v==src||iw==src||cs==colors)return fail();
    float data[12],inv[3],uv[4][6];ArrayStorage* targets[4]={};
    for(int n=0;n<3;++n){int off=offsets[n];inv[n]=divf(1.0f,get(src,off+3));for(int j=0;j<3;++j)data[n*4+j]=mul(get(src,off+j),inv[n]);data[n*4+3]=1.0f;}
    if(hasTex)for(int u=0;u<units;++u){int r=coords->get(u).ref;if(!r)continue;auto* a=m3g_array(r,"[F",offsets[2]+2);auto* b=m3g_array(ts->get(u).ref,"[F",6);
        if(!a||!b||a==b||b==v||b==iw||a==v||a==iw||b==src)return fail();for(int k=0;k<u;++k)if(targets[k]==b)return fail();targets[u]=b;
        for(int n=0;n<3;++n)for(int j=0;j<2;++j)uv[u][2*n+j]=perspective?mul(get(a,offsets[n]+j),inv[n]):get(a,offsets[n]+j);}
    float z=mul(add(add(data[2],data[6]),data[10]),0.33333334f);
    for(int i=0;i<12;++i)put(v,i,data[i]);for(int i=0;i<3;++i)put(iw,i,inv[i]);
    for(int u=0;u<units;++u)if(targets[u])for(int i=0;i<6;++i)put(targets[u],i,uv[u][i]);
    *sort=Value::of_i(f_bits(z));*hc=Value::of_i(hasColors);
    if(hasColors){cs->set(0,colors->get(0));cs->set(1,colors->get(fan+1));cs->set(2,colors->get(fan+2));}
    ++emitted;return true;
}
static void report(){log_line("M3GASSEMBLY schema=1 enabled="+std::to_string(VMS_RS(g_m3g_mesh242_enabled))+" gathered="+std::to_string(gathered)+" culled="+std::to_string(culled)+" emitted="+std::to_string(emitted)+" declines="+std::to_string(declines)+" max_vertices=3 max_texture_units=4 scope=process_cumulative");}
}
