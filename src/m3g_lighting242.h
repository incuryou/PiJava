#pragma once
#include "vm_rs_fallback.h"
// SPDX-License-Identifier: GPL-3.0-or-later
// Per-triangle port of Triangle.calculateLighting, retaining its strip reuse,
// flat-shading convention, material tracking and existing approximate math.
namespace m3g_lighting242 {
using namespace m3g_mesh242;
 VMS_RS_NSREF(vms_ref_calls, calls, uint64_t) VMS_RS_NSREF(vms_ref_vertices, vertices, uint64_t) VMS_RS_NSREF(vms_ref_light_visits, light_visits, uint64_t) VMS_RS_NSREF(vms_ref_declines, declines, uint64_t)
struct Input {int eye,norm,matrix,material,shade;bool flip,local;int lights,positions,directions,scope,tris,offset,prev[3],colors[3],out;};
static Value* field(int cid,int key,const char* name,const char* desc){return m3g_texture238::stat(cid,key,name,desc);}
static bool draw(int cid,const Input& in){
    if(!VMS_RS(g_m3g_mesh242_enabled))return false;
    auto fail=[](){++declines;return false;};
    if(!exact_class(in.norm,"javax/microedition/m3g/VertexArray")||!exact_class(in.matrix,"javax/microedition/m3g/Transform")||
       !exact_class(in.material,"javax/microedition/m3g/Material")||!exact_class(in.lights,"javax/microedition/m3g/M3GVector"))return fail();
    ArrayStorage* norms=nullptr;int nn,nc,nt;
    if(!vertex(in.norm,norms,nn,nc,nt)||nc!=3||in.offset<0)return fail();
    auto* tri=m3g_array(in.tris,"[I",size_t(in.offset)+3);auto* output=m3g_array(in.out,"[I",3);
    auto* eye=m3g_array(in.eye,"[F",0);auto* lp=m3g_array(in.positions,"[F",0);auto* ld=m3g_array(in.directions,"[F",0);
    if(!tri||!output||!eye||!lp||!ld||tri==output)return fail();
    int idx[3],colors[3];for(int i=0;i<3;++i){idx[i]=tri->get(in.offset+i).i;colors[i]=output->get(i).i;
        if(idx[i]<0||idx[i]>=nn||size_t(idx[i])*4+3>eye->size())return fail();}
    int tracking=0;if(!m3g_int(in.material,"tracking","Z",tracking))return fail();
    int vec=m3g_ref(in.lights,"elements","Ljava/util/Vector;"),ln=0;
    if(!exact_class(vec,"java/util/Vector")||!m3g_int(vec,"elementCount","I",ln)||ln<0||ln>32)return fail();
    // Do not bypass an outstanding synchronized Vector getter.
    auto monitor=VMS_EXEC(g_monitors).find(vec);
    if(monitor!=VMS_EXEC(g_monitors).end()&&monitor->second.depth>0&&monitor->second.owner!=&current_context())return fail();
    auto* lights=m3g_array(m3g_ref(vec,"elementData","[Ljava/lang/Object;"),"[Ljava/lang/Object;",ln);
    if(!lights||lp->size()<size_t(ln)*4||ld->size()<size_t(ln)*4)return fail();
    struct LightData {int mode;bool enabled;float r,g,b,pos[3],dir[3],constant,linear,quadratic,cutoff,exponent;};
    LightData ls[32];
    for(int l=0;l<ln;++l){int ref=lights->get(l).ref,rendering=0,scope=0,color=0,bits=0;auto& d=ls[l];
        if(!exact_class(ref,"javax/microedition/m3g/Light")||!m3g_int(ref,"rendering","Z",rendering)||!m3g_int(ref,"scope","I",scope))return fail();
        d.enabled=rendering&&(scope&in.scope)!=0;if(!d.enabled)continue;
        if(!m3g_int(ref,"mode","I",d.mode)||!m3g_int(ref,"color","I",color)||!m3g_int(ref,"intensity","F",bits))return fail();
        float intensity=bits_f(bits);d.r=mul(mul(float((uint32_t(color)>>16)&255),0.003921569f),intensity);
        d.g=mul(mul(float((uint32_t(color)>>8)&255),0.003921569f),intensity);d.b=mul(mul(float(uint32_t(color)&255),0.003921569f),intensity);
        for(int j=0;j<3;++j){d.pos[j]=get(lp,4*l+j);d.dir[j]=get(ld,4*l+j);}
        const char* names[]={"constant","linear","quadratic","cutoffCos","exponent"};float* outs[]={&d.constant,&d.linear,&d.quadratic,&d.cutoff,&d.exponent};
        for(int j=0;j<5;++j){if(!m3g_int(ref,names[j],"F",bits))return fail();*outs[j]=bits_f(bits);}
    }
    const char* names[]={"maR","mdR","msR","meR","maG","mdG","msG","meG","maB","mdB","msB","meB","shininess"};
    Value* scalars[13];float material[13];
    for(int j=0;j<13;++j){scalars[j]=field(cid,3+j,names[j],"F");if(!scalars[j])return fail();material[j]=bits_f(scalars[j]->i);}
    auto* alpha=field(cid,16,"lightAlpha","I");if(!alpha)return fail();int lightAlpha=alpha->i;
    auto* nv=field(cid,0,"N_EYE","[F");auto* vv=field(cid,1,"V_EYE","[F");auto* mv=field(cid,2,"L_MAT","[F");
    auto* ne=nv?m3g_array(nv->ref,"[F",4):nullptr;auto* ve=vv?m3g_array(vv->ref,"[F",4):nullptr;auto* mat=mv?m3g_array(mv->ref,"[F",16):nullptr;
    if(!ne||!ve||!mat||ne==ve||ne==mat||ve==mat||ne==eye||ve==eye||mat==eye||ne==lp||ve==lp||mat==lp||ne==ld||ve==ld||mat==ld)return fail();
    float m[16];if(!m3g_read_matrix(in.matrix,m))return fail();
    float normal[3]={get(ne,0),get(ne,1),get(ne,2)},vEye[3]={get(ve,0),get(ve,1),get(ve,2)};
    uint64_t done=0,visits=0;
    // PolygonMode.SHADE_FLAT = 164; the original uses vertex 2, not vertex 0.
    for(int v=in.shade==164?2:0;v<=2;++v){
        if(in.shade!=164){bool reused=false;for(int j=0;j<3;++j)if(idx[v]==in.prev[j]){colors[v]=in.colors[j];reused=true;break;}if(reused)continue;}
        if(tracking){uint32_t color=uint32_t(colors[v]);lightAlpha=int(color>>24);for(int j=0;j<3;++j){float c=mul(float((color>>(16-8*j))&255),0.003921569f);material[4*j]=material[4*j+1]=c;}}
        float src[3];for(int j=0;j<3;++j)src[j]=mul(float(norms->get(size_t(idx[v])*3+j).i),nt==1?0.007874016f:3.051851E-5f);
        for(int j=0;j<3;++j)normal[j]=dot(m[j*4],src[0],m[j*4+1],src[1],m[j*4+2],src[2]);
        float length=dot(normal[0],normal[0],normal[1],normal[1],normal[2],normal[2]);
        if(length>1.0e-30f){float inv=invsqrt(length,true);for(int j=0;j<3;++j)normal[j]=mul(normal[j],inv);}else{normal[0]=normal[1]=0.0f;normal[2]=1.0f;}
        if(in.flip)for(float& x:normal)x=-x;
        for(int j=0;j<3;++j)vEye[j]=get(eye,size_t(idx[v])*4+j);
        float rgb[3]={material[3],material[7],material[11]},view[3]={0,0,1};
        if(in.local){for(int j=0;j<3;++j)view[j]=-vEye[j];float len=invsqrt(dot(view[0],view[0],view[1],view[1],view[2],view[2]),true);
            if(len>0.0001f)for(float& x:view)x=mul(x,len);}
        for(int l=0;l<ln;++l){const auto& light=ls[l];if(!light.enabled)continue;++visits;float lc[3]={light.r,light.g,light.b};
            if(light.mode==128){for(int j=0;j<3;++j)rgb[j]=add(rgb[j],mul(material[4*j],lc[j]));continue;}
            float direction[3],attenuation=1.0f;
            if(light.mode==129){for(int j=0;j<3;++j)direction[j]=-light.dir[j];}
            else{
                float delta[3];for(int j=0;j<3;++j)delta[j]=sub(light.pos[j],vEye[j]);
                float d2=dot(delta[0],delta[0],delta[1],delta[1],delta[2],delta[2]),invDist=invsqrt(d2,true);
                if(invDist>0.0001f){for(int j=0;j<3;++j)direction[j]=mul(delta[j],invDist);
                    float dist=mul(d2,invDist);attenuation=reciprocal(add(add(light.constant,mul(light.linear,dist)),mul(light.quadratic,d2)));}
                else{direction[0]=direction[1]=0;direction[2]=1;attenuation=reciprocal(light.constant);}
                if(light.mode==131){float spot=dot(-direction[0],light.dir[0],-direction[1],light.dir[1],-direction[2],light.dir[2]);
                    if(spot>=light.cutoff)attenuation=mul(attenuation,float(std::pow(double(spot),double(light.exponent))));else attenuation=0.0f;}
            }
            if(attenuation<=0.0f)continue;
            float ndl=dot(normal[0],direction[0],normal[1],direction[1],normal[2],direction[2]);
            if(ndl>0.0f){float diff=mul(ndl,attenuation);for(int j=0;j<3;++j)rgb[j]=add(rgb[j],mul(mul(material[4*j+1],lc[j]),diff));
                float h[3];for(int j=0;j<3;++j)h[j]=add(direction[j],view[j]);float len=invsqrt(dot(h[0],h[0],h[1],h[1],h[2],h[2]),true);
                if(len>0.0001f){for(float& x:h)x=mul(x,len);float ndh=dot(normal[0],h[0],normal[1],h[1],normal[2],h[2]);
                    if(ndh>0.0f){float factor=mul(float(std::pow(double(ndh),double(material[12]))),attenuation);
                        for(int j=0;j<3;++j)rgb[j]=add(rgb[j],mul(mul(material[4*j+2],lc[j]),factor));}}
            }
        }
        uint32_t packed=(uint32_t(lightAlpha)&255)<<24;
        for(int j=0;j<3;++j){int c=java_fp_integer<int32_t>(mul(rgb[j],255.0f));c=c<0?0:std::min(c,255);packed|=uint32_t(c)<<(16-j*8);}
        colors[v]=int_bits(packed);++done;
        if(in.shade==164){colors[0]=colors[1]=colors[v];break;}
    }
    // Single commit after validation/computation; no intermediate Java state.
    for(int j=0;j<3;++j){output->set(j,Value::of_i(colors[j]));put(ne,j,normal[j]);put(ve,j,vEye[j]);}
    for(int j=0;j<16;++j)put(mat,j,m[j]);for(int j=0;j<13;++j)*scalars[j]=Value::of_i(f_bits(material[j]));*alpha=Value::of_i(lightAlpha);
    ++calls;vertices+=done;light_visits+=visits;return true;
}
static void report(){log_line("M3GLIGHT schema=1 enabled="+std::to_string(VMS_RS(g_m3g_mesh242_enabled))+" calls="+std::to_string(calls)+" vertices="+std::to_string(vertices)+" light_visits="+std::to_string(light_visits)+" declines="+std::to_string(declines)+" max_vertices=3 max_lights=32 scope=process_cumulative");}
}
