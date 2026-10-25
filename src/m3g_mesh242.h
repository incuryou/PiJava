#pragma once
#include "vm_rs_fallback.h"
// SPDX-License-Identifier: GPL-3.0-or-later
// Arithmetic translated from the bundled SkinnedMesh/M3GMath. No approximation
// changes: preserve binary32 order, quantization, range checks, bone order.
// All pointers are invocation-local. No Java allocation/callback/GC/yield here.
 VMS_RS_ACCESSOR(g_m3g_mesh242_enabled, bool)
 VMS_RS_ACCESSOR(g_host_boundary242_enabled, bool)
namespace m3g_mesh242 {
using m3g_texture238::add; using m3g_texture238::sub; using m3g_texture238::mul;
using m3g_texture238::sar; using m3g_texture238::exact_class;
static float divf(float x,float y){volatile float r=x/y;return r;}
static float get(ArrayStorage* a,size_t i){return bits_f(a->get(i).i);}
static void put(ArrayStorage* a,size_t i,float x){a->set(i,Value::of_i(f_bits(x)));}
static float dot(float a,float x,float b,float y,float c,float z){return add(add(mul(a,x),mul(b,y)),mul(c,z));}
static float invsqrt(float x,bool fast=false){
    const uint32_t magic=fast?0x5f375a86u:0x5f375996u;
    float half=mul(0.5f,x),y=bits_f(int_bits(magic-uint32_t(sar(uint32_t(f_bits(x)),1))));
    y=mul(y,sub(1.5f,mul(mul(half,y),y)));
    if(!fast)y=mul(y,sub(1.5f,mul(mul(half,y),y)));
    return y;
}
static float reciprocal(float x){float y=bits_f(int_bits(0x7ef127eau-uint32_t(f_bits(x))));float e=sub(1.0f,mul(x,y));return mul(mul(y,add(1.0f,e)),add(1.0f,mul(e,e)));}
struct Stats {uint64_t skin_calls=0,skin_vertices=0,skin_weights=0,skin_declines=0,quantize_calls=0,quantize_values=0,quantize_declines=0,identities=0,inverses=0,transform_declines=0;} ;
 VMS_RS_NSREF(vms_ref_stats, stats, Stats)
// Restrict inherited private-field access to the bundled concrete classes.
// A game subclass can shadow fields even without overriding a getter.
static bool standard_transformable(int ref){
    if(ref<=0||ref>=int(VMS_HEAP(g_heap).size())||VMS_HEAP(g_heap)[ref].cls<0)return false;
    const std::string& n=VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name;
    for(const char* name:{"javax/microedition/m3g/Group","javax/microedition/m3g/World","javax/microedition/m3g/Mesh",
        "javax/microedition/m3g/SkinnedMesh","javax/microedition/m3g/MorphingMesh","javax/microedition/m3g/Light",
        "javax/microedition/m3g/Camera","javax/microedition/m3g/Sprite3D","javax/microedition/m3g/Texture2D"})
        if(n==name)return true;
    return false;
}
static bool eligible_skin(int p,int n,int dst,int dn,int posed){
    return VMS_RS(g_m3g_mesh242_enabled) && exact_class(p,"javax/microedition/m3g/VertexArray") &&
        (!n||exact_class(n,"javax/microedition/m3g/VertexArray")) && exact_class(dst,"javax/microedition/m3g/VertexArray") &&
        (!dn||exact_class(dn,"javax/microedition/m3g/VertexArray")) && exact_class(posed,"javax/microedition/m3g/VertexBuffer");
}
static bool vertex(int ref,ArrayStorage*& data,int& n,int& c,int& type){
    if(!exact_class(ref,"javax/microedition/m3g/VertexArray") ||
       !m3g_int(ref,"numVertices","I",n)||!m3g_int(ref,"numComponents","I",c)||!m3g_int(ref,"componentType","I",type)||
       n<1||n>65535||c<2||c>4||(type!=1&&type!=2))return false;
    data=m3g_array(m3g_ref(ref,type==1?"vertArrayByteSize":"vertArrayShortSize",type==1?"[B":"[S"),type==1?"[B":"[S",size_t(n)*c);
    return data!=nullptr;
}
static bool skin_block(int p,int nr,int matrices,int ranges,int sb,int output,int normal,int bounds,int first,int count){
    if(!VMS_RS(g_m3g_mesh242_enabled))return false;
    auto fail=[](){++stats.skin_declines;return false;};
    if(count<1||count>128||first<0)return fail();
    ArrayStorage *pos=nullptr,*norm=nullptr;int n,c,type,nn=0,nc=0,nt=0;
    if(!vertex(p,pos,n,c,type)||c>3||count>n||first>n-count)return fail();
    if(nr && (!vertex(nr,norm,nn,nc,nt)||nc!=3||c!=3||nn!=n))return fail();
    if(bool(nr)!=bool(normal))return fail();
    auto* mm=m3g_array(matrices,"[[F",0);auto* br=m3g_array(ranges,"[I",0);auto* scale=m3g_array(sb,"[F",4);
    auto* out=m3g_array(output,"[F",size_t(n)*c);auto* outn=normal?m3g_array(normal,"[F",size_t(n)*c):nullptr;
    auto* box=m3g_array(bounds,"[F",6);
    if(!mm||mm->size()>256||!br||br->size()!=mm->size()*3||!scale||!out||!box||(normal&&!outn))return fail();
    if(out==outn||out==box||out==scale||outn==box||outn==scale||box==scale)return fail();
    struct Bone{float m[16],w;int start,end;};Bone bones[256];
    for(size_t b=0;b<mm->size();++b){
        auto* ma=m3g_array(mm->get(b).ref,"[F",16);int s=br->get(3*b).i,k=br->get(3*b+1).i,w=br->get(3*b+2).i;
        if(!ma||ma==out||ma==outn||ma==box||s<0||k<1||k>n||s>n-k||w<=0)return fail();
        auto& bone=bones[b];bone.start=s;bone.end=s+k;bone.w=float(w);
        for(int j=0;j<16;++j){bone.m[j]=get(ma,j);if(!std::isfinite(bone.m[j]))return fail();}
    }
    float sc=get(scale,0),bias[3]={get(scale,1),get(scale,2),get(scale,3)},bb[6];
    for(int i=0;i<6;++i)bb[i]=get(box,i);
    float fp[128*3],fn[128*3];uint64_t weights=0;
    for(int v=first;v<first+count;++v){
        int off=v*c,dst=(v-first)*c;
        float raw[3]={float(pos->get(off).i),float(pos->get(off+1).i),c>2?float(pos->get(off+2).i):0.0f};
        float x=add(mul(raw[0],sc),bias[0]),y=add(mul(raw[1],sc),bias[1]),z=c>2?add(mul(raw[2],sc),bias[2]):0.0f;
        float accum[3]={},an[3]={},on[3]={},ws=0;
        if(norm)for(int j=0;j<3;++j)on[j]=float(norm->get(off+j).i);
        // Reorder independent vertices, NEVER the accumulation order of bones.
        for(size_t b=0;b<mm->size();++b){auto& bone=bones[b];if(v<bone.start||v>=bone.end)continue;auto* m=bone.m;float w=bone.w;
            for(int j=0;j<3;++j){int r=j*4;accum[j]=add(accum[j],mul(add(dot(m[r],x,m[r+1],y,m[r+2],z),m[r+3]),w));
                if(norm)an[j]=add(an[j],mul(dot(m[r],on[0],m[r+1],on[1],m[r+2],on[2]),w));}
            ws=add(ws,w);++weights;
        }
        float f[3]={x,y,z};if(ws>0.0001f){float iw=divf(1.0f,ws);for(int j=0;j<3;++j)f[j]=mul(accum[j],iw);}
        for(int j=0;j<c;++j){fp[dst+j]=f[j];bb[2*j]=bb[2*j]<f[j]?bb[2*j]:f[j];bb[2*j+1]=bb[2*j+1]>f[j]?bb[2*j+1]:f[j];}
        if(norm){float len=dot(an[0],an[0],an[1],an[1],an[2],an[2]);
            float original=dot(on[0],on[0],on[1],on[1],on[2],on[2]);
            for(int j=0;j<3;++j)fn[dst+j]=(ws>0.0001f&&len>0.0001f)?mul(mul(an[j],invsqrt(len)),mul(original,invsqrt(original))):on[j];}
    }
    for(int i=0;i<count*c;++i)if(!std::isfinite(fp[i])||(norm&&!std::isfinite(fn[i])))return fail();
    for(int i=0;i<count*c;++i){put(out,first*c+i,fp[i]);if(norm)put(outn,first*c+i,fn[i]);}
    for(int i=0;i<6;++i)put(box,i,bb[i]);
    ++stats.skin_calls;stats.skin_vertices+=count;stats.skin_weights+=weights;return true;
}
// Match the source's branchless signed-int min/max, including 32-bit wrap.
static int imin(int a,int b){uint32_t d=uint32_t(a)-uint32_t(b);return int_bits(uint32_t(b)+(d&uint32_t(sar(d,31))));}
static int imax(int a,int b){uint32_t d=uint32_t(a)-uint32_t(b);return int_bits(uint32_t(a)-(d&uint32_t(sar(d,31))));}
static bool quantize(int source,int target,int first,int count,int components,float inv,float bx,float by,float bz,bool positions){
    if(!VMS_RS(g_m3g_mesh242_enabled))return false;
    auto fail=[](){++stats.quantize_declines;return false;};
    if(first<0||count<1||count>1024||components<2||components>3)return fail();
    auto* a=m3g_array(source,"[F",size_t(first)+count);auto* b=m3g_array(target,"[B",size_t(first)+count);
    bool bytes=b!=nullptr;if(!b)b=m3g_array(target,"[S",size_t(first)+count);if(!a||!b)return fail();
    float bias[3]={bx,by,bz};int out[1024];
    for(int i=0;i<count;++i){float v=get(a,first+i);if(positions)v=mul(sub(v,bias[(first+i)%components]),inv);
        int r=java_fp_integer<int32_t>(v>0?add(v,0.5f):sub(v,0.5f));out[i]=imax(bytes?-128:-32768,imin(bytes?127:32767,r));}
    for(int i=0;i<count;++i)b->set(first+i,Value::of_i(out[i]));
    ++stats.quantize_calls;stats.quantize_values+=count;return true;
}
static bool identity(int matrix){
    if(!VMS_RS(g_m3g_mesh242_enabled))return false;auto* a=m3g_array(matrix,"[F",16);if(!a){++stats.transform_declines;return false;}
    for(int i=0;i<16;++i)put(a,i,i%5==0?1.0f:0.0f);++stats.identities;return true;
}
static bool inverse_core(const float m[16],float tmp[16]){
// Non-finite arithmetic stays in Java, preserving its raw NaN/sign behavior.
for(int i=0;i<16;++i)if(!std::isfinite(m[i]))return false;
float s0=sub(mul(m[0],m[5]),mul(m[4],m[1]));
float s1=sub(mul(m[0],m[6]),mul(m[4],m[2]));
float s2=sub(mul(m[0],m[7]),mul(m[4],m[3]));
float s3=sub(mul(m[1],m[6]),mul(m[5],m[2]));
float s4=sub(mul(m[1],m[7]),mul(m[5],m[3]));
float s5=sub(mul(m[2],m[7]),mul(m[6],m[3]));
float c0=sub(mul(m[8],m[13]),mul(m[12],m[9]));
float c1=sub(mul(m[8],m[14]),mul(m[12],m[10]));
float c2=sub(mul(m[8],m[15]),mul(m[12],m[11]));
float c3=sub(mul(m[9],m[14]),mul(m[13],m[10]));
float c4=sub(mul(m[9],m[15]),mul(m[13],m[11]));
float c5=sub(mul(m[10],m[15]),mul(m[14],m[11]));
float determDiv=add(sub(add(add(sub(mul(s0,c5),mul(s1,c4)),mul(s2,c3)),mul(s3,c2)),mul(s4,c1)),mul(s5,c0));
if(determDiv==0.0f){return false;}
float determinant=divf(1.0f,determDiv);
if(determinant==0.0f){return false;}
tmp[0]=mul(add(sub(mul(m[5],c5),mul(m[6],c4)),mul(m[7],c3)),determinant);
tmp[1]=mul(sub(add(mul((-m[1]),c5),mul(m[2],c4)),mul(m[3],c3)),determinant);
tmp[2]=mul(add(sub(mul(m[13],s5),mul(m[14],s4)),mul(m[15],s3)),determinant);
tmp[3]=mul(sub(add(mul((-m[9]),s5),mul(m[10],s4)),mul(m[11],s3)),determinant);
tmp[4]=mul(sub(add(mul((-m[4]),c5),mul(m[6],c2)),mul(m[7],c1)),determinant);
tmp[5]=mul(add(sub(mul(m[0],c5),mul(m[2],c2)),mul(m[3],c1)),determinant);
tmp[6]=mul(sub(add(mul((-m[12]),s5),mul(m[14],s2)),mul(m[15],s1)),determinant);
tmp[7]=mul(add(sub(mul(m[8],s5),mul(m[10],s2)),mul(m[11],s1)),determinant);
tmp[8]=mul(add(sub(mul(m[4],c4),mul(m[5],c2)),mul(m[7],c0)),determinant);
tmp[9]=mul(sub(add(mul((-m[0]),c4),mul(m[1],c2)),mul(m[3],c0)),determinant);
tmp[10]=mul(add(sub(mul(m[12],s4),mul(m[13],s2)),mul(m[15],s0)),determinant);
tmp[11]=mul(sub(add(mul((-m[8]),s4),mul(m[9],s2)),mul(m[11],s0)),determinant);
tmp[12]=mul(sub(add(mul((-m[4]),c3),mul(m[5],c1)),mul(m[6],c0)),determinant);
tmp[13]=mul(add(sub(mul(m[0],c3),mul(m[1],c1)),mul(m[2],c0)),determinant);
tmp[14]=mul(sub(add(mul((-m[12]),s3),mul(m[13],s1)),mul(m[14],s0)),determinant);
tmp[15]=mul(add(sub(mul(m[8],s3),mul(m[9],s1)),mul(m[10],s0)),determinant);
return true;
}
static bool invert(int matrix,int scratch){
    if(!VMS_RS(g_m3g_mesh242_enabled))return false;
    auto* a=m3g_array(matrix,"[F",16);auto* b=m3g_array(scratch,"[F",16);
    if(!a||!b||a==b){++stats.transform_declines;return false;}
    float m[16],tmp[16];for(int i=0;i<16;++i)m[i]=get(a,i);
if(!inverse_core(m,tmp)){++stats.transform_declines;return false;}
    for(int i=0;i<16;++i){put(b,i,tmp[i]);put(a,i,tmp[i]);}
    ++stats.inverses;return true;
}
 VMS_RS_NSREF(vms_ref_composite_calls, composite_calls, uint64_t) VMS_RS_NSREF(vms_ref_transform_to_calls, transform_to_calls, uint64_t) VMS_RS_NSREF(vms_ref_transform_to_nodes, transform_to_nodes, uint64_t)
static void identity_core(float out[16]){for(int i=0;i<16;++i)out[i]=i%5==0?1.0f:0.0f;}
static void multiply_core(const float l[16],const float r[16],float dst[16]){
    // Exactly the same expression as the existing m3g_multiply native island.
    float out[16];for(int row=0;row<16;row+=4)for(int col=0;col<4;++col)
        out[row+col]=l[row]*r[col]+l[row+1]*r[col+4]+l[row+2]*r[col+8]+l[row+3]*r[col+12];
    for(int i=0;i<16;++i)dst[i]=out[i];
}
static bool composite_core(int object,float result[16],ArrayStorage* forbidden=nullptr){
    if(!standard_transformable(object))return false;
    const char* fields[]={"translate","rotate","scale","matrix"};float inputs[4][16];
    for(int j=0;j<4;++j){int r=m3g_ref(object,fields[j],"Ljavax/microedition/m3g/Transform;");
        if(!exact_class(r,"javax/microedition/m3g/Transform"))return false;
        auto* a=m3g_array(m3g_ref(r,"matrix","[F"),"[F",16);
        if(!a||a==forbidden)return false;for(int k=0;k<16;++k){inputs[j][k]=get(a,k);if(!std::isfinite(inputs[j][k]))return false;}}
    identity_core(result);for(int j=0;j<4;++j)multiply_core(result,inputs[j],result);for(int k=0;k<16;++k)if(!std::isfinite(result[k]))return false;return true;
}
static bool composite(int object,int target){
    if(!VMS_RS(g_m3g_mesh242_enabled))return false;
    if(!exact_class(target,"javax/microedition/m3g/Transform")){++stats.transform_declines;return false;}
    auto* a=m3g_array(m3g_ref(target,"matrix","[F"),"[F",16);float out[16];
    if(!a||!composite_core(object,out,a)){++stats.transform_declines;return false;}
    for(int i=0;i<16;++i)put(a,i,out[i]);++composite_calls;VMS_RS(g_m3g_native_matrices)+=4;return true;
}
static int transform_to(int from,int target,int output){
    if(!VMS_RS(g_m3g_mesh242_enabled))return -1;
    auto fail=[](){++stats.transform_declines;return -1;};
    if(!exact_class(output,"javax/microedition/m3g/Transform"))return fail();
    auto* out=m3g_array(m3g_ref(output,"matrix","[F"),"[F",16);if(!out)return fail();
    int paths[2][128],lengths[2]={};
    for(int j=0;j<2;++j){int r=j?target:from;if(!r)return fail();
        while(r){if(lengths[j]>=128||!standard_transformable(r))return fail();
            for(int k=0;k<lengths[j];++k)if(paths[j][k]==r)return fail();
            auto* parent=m3g_field(r,"parent","Ljavax/microedition/m3g/Node;");if(!parent)return fail();
            paths[j][lengths[j]++]=r;r=parent->ref;}}
    if(from==target){float id[16];identity_core(id);for(int i=0;i<16;++i)put(out,i,id[i]);++transform_to_calls;return 1;}
    if(paths[0][lengths[0]-1]!=paths[1][lengths[1]-1])return 0;
    int i=lengths[0]-1,j=lengths[1]-1;while(i>=0&&j>=0&&paths[0][i]==paths[1][j]){--i;--j;}
    const char* cache[]={"thisToRoot","targetToRoot","temp"};ArrayStorage* work[4];
    for(int n=0;n<3;++n){int ref=m3g_ref(from,cache[n],"Ljavax/microedition/m3g/Transform;");
        if(!exact_class(ref,"javax/microedition/m3g/Transform"))return fail();
        work[n]=m3g_array(m3g_ref(ref,"matrix","[F"),"[F",16);
        if(n==1)work[3]=m3g_array(m3g_ref(ref,"scratch","[F"),"[F",16);}
    for(int a=0;a<4;++a){if(!work[a]||work[a]==out)return fail();for(int b=0;b<a;++b)if(work[a]==work[b])return fail();}
    float accumulated[2][16],temp[16],inv[16];identity_core(temp);identity_core(accumulated[0]);identity_core(accumulated[1]);
    int counts[]={i+1,j+1},nodes=0;
    for(int side=0;side<2;++side)for(int k=counts[side]-1;k>=0;--k){int ref=paths[side][k];
        Method* method=find_method(VMS_HEAP(g_heap)[ref].cls,"getCompositeTransform","(Ljavax/microedition/m3g/Transform;)V",true);
        if(!method||VMS_CL(g_classes)[method->class_id].name!="javax/microedition/m3g/Transformable")return fail();
        // These are private component matrices, but reject aliases rather than
        // assuming snapshots / malformed input could never produce them.
        const char* names[]={"translate","rotate","scale","matrix"};
        for(const char* name:names){int tr=m3g_ref(ref,name,"Ljavax/microedition/m3g/Transform;");auto* a=m3g_array(m3g_ref(tr,"matrix","[F"),"[F",16);
            for(auto* w:work)if(a==w)return fail();}
        if(!composite_core(ref,temp))return fail();multiply_core(accumulated[side],temp,accumulated[side]);++nodes;
    }
    if(!inverse_core(accumulated[1],inv))return fail();float result[16];multiply_core(inv,accumulated[0],result);
    for(int n=0;n<16;++n){put(work[0],n,accumulated[0][n]);put(work[1],n,result[n]);put(work[2],n,temp[n]);put(work[3],n,inv[n]);put(out,n,result[n]);}
    ++transform_to_calls;transform_to_nodes+=nodes;VMS_RS(g_m3g_native_matrices)+=uint64_t(nodes)*5+1;return 1;
}
static void report(){
    log_line("M3GMESH schema=1 enabled="+std::to_string(VMS_RS(g_m3g_mesh242_enabled))+
        " skin_calls="+std::to_string(stats.skin_calls)+" skin_vertices="+std::to_string(stats.skin_vertices)+
        " skin_weights="+std::to_string(stats.skin_weights)+" skin_declines="+std::to_string(stats.skin_declines)+
        " quantize_calls="+std::to_string(stats.quantize_calls)+" quantize_values="+std::to_string(stats.quantize_values)+
        " quantize_declines="+std::to_string(stats.quantize_declines)+" identities="+std::to_string(stats.identities)+
        " inverses="+std::to_string(stats.inverses)+" transform_declines="+std::to_string(stats.transform_declines)+
        " composite_calls="+std::to_string(composite_calls)+" transform_to_calls="+std::to_string(transform_to_calls)+" transform_to_nodes="+std::to_string(transform_to_nodes)+" max_skin_vertices=128 max_bones=256 max_quantize_values=1024 max_path_nodes=128 scope=process_cumulative");
}
} // namespace m3g_mesh242
