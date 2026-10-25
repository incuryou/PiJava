#include "vm_rs_fallback.h"
#ifndef J2ME_M3G_GEOMETRY_H
#define J2ME_M3G_GEOMETRY_H
#include "m3g_native.h"
 VMS_RS_ACCESSOR(g_m3g_native_vectors, uint64_t) VMS_RS_ACCESSOR(g_m3g_native_vertices, uint64_t) VMS_RS_ACCESSOR(g_m3g_native_matrices, uint64_t)
static bool m3g_read_matrix(int self,float m[16]) {
    ArrayStorage* a=m3g_array(m3g_ref(self,"matrix","[F"),"[F",16);if(!a)return false;
    for(int i=0;i<16;++i)m[i]=bits_f(a->get(i).i);
    return true;
}
static void m3g_apply_vector(const float m[16],ArrayStorage& out,size_t i,float x,float y,float z,float w) {
    // Match Java's row-major float evaluation order; copy matrix before mutations.
    for(int r=0;r<4;++r)out.set(i+r,Value::of_i(f_bits(m[4*r]*x+m[4*r+1]*y+m[4*r+2]*z+m[4*r+3]*w)));
}
static bool m3g_transform_vectors(int self,int array) {
    float m[16];ArrayStorage* a=m3g_array(array,"[F",0);
    if(!a||(a->size()&3)||!m3g_read_matrix(self,m))return false;
    for(size_t i=0;i<a->size();i+=4){float x=bits_f(a->get(i).i),y=bits_f(a->get(i+1).i),z=bits_f(a->get(i+2).i),w=bits_f(a->get(i+3).i);m3g_apply_vector(m,*a,i,x,y,z,w);}
    VMS_RS(g_m3g_native_vectors)+=a->size()/4;return true;
}
static bool m3g_transform_vertices(int self,int vertices,int output,bool W) {
    int n=0,d=0,type=0;float m[16];
    if(!m3g_int(vertices,"numVertices","I",n)||!m3g_int(vertices,"numComponents","I",d)||!m3g_int(vertices,"componentType","I",type)||n<0||(d!=2&&d!=3)||(type!=1&&type!=2)||!m3g_read_matrix(self,m))return false;
    ArrayStorage* out=m3g_array(output,"[F",size_t(n)*4);
    ArrayStorage* in=m3g_array(m3g_ref(vertices,type==1?"vertArrayByteSize":"vertArrayShortSize",type==1?"[B":"[S"),type==1?"[B":"[S",size_t(n)*d);
    if(!in||!out)return false;
    for(int i=0;i<n;++i){size_t s=size_t(i)*d;m3g_apply_vector(m,*out,size_t(i)*4,float(in->get(s).i),float(in->get(s+1).i),d==3?float(in->get(s+2).i):0.0f,W?1.0f:0.0f);}
    VMS_RS(g_m3g_native_vertices)+=n;return true;
}
static bool m3g_transform_texcoords(int self,int array) {
    float m[16];ArrayStorage* a=m3g_array(array,"[F",0);
    if(!a||(a->size()&1)||!m3g_read_matrix(self,m))return false;
    for(size_t i=0;i<a->size();i+=2){float s=bits_f(a->get(i).i),t=bits_f(a->get(i+1).i);a->set(i,Value::of_i(f_bits(m[0]*s+m[1]*t+m[3])));a->set(i+1,Value::of_i(f_bits(m[4]*s+m[5]*t+m[7])));}
    return true;
}
static bool m3g_multiply(int left,int right,int target) {
    ArrayStorage* a=m3g_array(left,"[F",16),*b=m3g_array(right,"[F",16),*out=m3g_array(target,"[F",16);
    if(!a||!b||!out)return false;
    float l[16],r[16];for(int i=0;i<16;++i){l[i]=bits_f(a->get(i).i);r[i]=bits_f(b->get(i).i);}
    for(int row=0;row<16;row+=4)for(int col=0;col<4;++col)out->set(row+col,Value::of_i(f_bits(l[row]*r[col]+l[row+1]*r[col+4]+l[row+2]*r[col+8]+l[row+3]*r[col+12])));
    ++VMS_RS(g_m3g_native_matrices);return true;
}
#include "m3g_background.h"
#endif
