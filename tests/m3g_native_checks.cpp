// Standalone VM-heap fixture. No JNI, native graphics context, or parallel state.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
struct Value {
 int32_t i=0,ref=0;int64_t l=0;bool is_long=false;
 static Value of_i(int32_t n){Value v;v.i=n;return v;}
 static Value of_ref(int32_t n){Value v;v.ref=n;return v;}
 static Value of_l(int64_t n){Value v;v.l=n;v.is_long=true;return v;}
};
#include "../src/array_storage.h"
struct Obj {int cls=-1;std::vector<Value> fields;ArrayStorage elems;std::string array_desc;bool is_array=false;};
struct Field {std::string name,desc;int slot=-1,owner=-1;bool is_static=false;};
struct Class {int super_id=-1;std::vector<Field> fields;std::vector<Value> statics;int slots=0;};
static std::deque<Obj> g_heap(1);static std::deque<Class> VMS_CL(g_classes);
static Field* find_field(int cls,const std::string& n,const std::string& d){for(int c=cls;c>=0;c=VMS_CL(g_classes)[c].super_id)for(auto& f:VMS_CL(g_classes)[c].fields)if(f.name==n&&f.desc==d)return &f;return nullptr;}
static float bits_f(int32_t i){float f;std::memcpy(&f,&i,4);return f;}
static int32_t f_bits(float f){int32_t i;std::memcpy(&i,&f,4);return i;}
#include "../src/m3g_geometry.h"
static int object(){VMS_CL(g_classes).emplace_back();VMS_HEAP(g_heap).emplace_back();int r=VMS_HEAP(g_heap).size()-1;VMS_HEAP(g_heap)[r].cls=VMS_CL(g_classes).size()-1;return r;}
static void field(int o,const char* name,const char* desc,Value v,bool st=false){int c=VMS_HEAP(g_heap)[o].cls;auto& cl=VMS_CL(g_classes)[c];auto& values=st?cl.statics:VMS_HEAP(g_heap)[o].fields;int slot=values.size();values.push_back(v);cl.fields.push_back({name,desc,slot,c,st});}
static void fi(int o,const char* n,const char* d,int v,bool st=false){field(o,n,d,Value::of_i(v),st);}
static void fr(int o,const char* n,const char* d,int v,bool st=false){field(o,n,d,Value::of_ref(v),st);}
static void set(int o,const char* n,const char* d,int v){m3g_field(o,n,d)->i=v;}
static int array(const char* desc,size_t n){VMS_HEAP(g_heap).emplace_back();int r=VMS_HEAP(g_heap).size()-1;VMS_HEAP(g_heap)[r].is_array=true;VMS_HEAP(g_heap)[r].array_desc=desc;assert(VMS_HEAP(g_heap)[r].elems.reset(desc,n));return r;}
static int floats(std::initializer_list<float> v){int a=array("[F",v.size());size_t i=0;for(float f:v)VMS_HEAP(g_heap)[a].elems.set(i++,Value::of_i(f_bits(f)));return a;}
static void af(int a,int i,float f){VMS_HEAP(g_heap)[a].elems.set(i,Value::of_i(f_bits(f)));}
static float fa(int a,int i){return bits_f(VMS_HEAP(g_heap)[a].elems.get(i).i);}
static void fill(int a,int v){for(size_t i=0;i<VMS_HEAP(g_heap)[a].elems.size();++i)VMS_HEAP(g_heap)[a].elems.set(i,Value::of_i(v));}
int main(){
 int g=object(),p=array("[I",64),d=array("[S",64),a=object(),c=object(),tri=object();
 fi(g,"hints","I",0);fi(g,"canvasWidth","I",8,true);fi(g,"canvasHeight","I",8,true);fi(g,"originX","I",0);fi(g,"originY","I",0);fi(g,"viewx","I",0);fi(g,"viewy","I",0);
 fi(g,"viewClipL","I",0);fi(g,"viewClipT","I",0);fi(g,"viewClipR","I",8);fi(g,"viewClipB","I",8);fi(g,"depthEnabled","Z",1);fi(g,"currAlphaFactor","I",256);
 fr(g,"rasterData","[I",p);fr(g,"depthBuffer","[S",d);fr(g,"defaultCompositing","Ljavax/microedition/m3g/CompositingMode;",c);
 fi(c,"blending","I",68);fi(c,"depthTest","Z",1);fi(c,"depthWrite","Z",1);fi(c,"colorWrite","Z",1);fi(c,"alphaWrite","Z",1);fi(c,"alphaThreshold","F",f_bits(0));fi(c,"depthOffsetFactor","F",f_bits(0));fi(c,"depthOffsetUnits","F",f_bits(0));
 fr(a,"compositingMode","Ljavax/microedition/m3g/CompositingMode;",0);fr(a,"fog","Ljavax/microedition/m3g/Fog;",0);
 int v=floats({1,1,10,1,5,1,10,1,1,5,10,1}),col=array("[I",3),iw=floats({1,1,1});fr(tri,"v","[F",v);fr(tri,"colors","[I",col);fr(tri,"invW","[F",iw);fill(col,int32_t(0xff102030));fill(d,32767);
 assert(m3g_draw_triangle(g,tri,a,0xff123456,false,false,false));assert(VMS_RS(g_m3g_native_pixels)==6);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==0xff123456u);assert(VMS_HEAP(g_heap)[d].elems.get(9).i==10);
 // Winding invariance and top-left shared edge ownership.
 std::vector<int> before;for(int i=0;i<64;++i)before.push_back(VMS_HEAP(g_heap)[p].elems.get(i).i);fill(p,0);af(v,4,1);af(v,5,5);af(v,8,5);af(v,9,1);assert(m3g_draw_triangle(g,tri,a,0xff123456,false,false,false));for(int i=0;i<64;++i)assert(before[i]==VMS_HEAP(g_heap)[p].elems.get(i).i);
 fill(p,0);fill(d,32767);uint64_t old=VMS_RS(g_m3g_native_pixels);
 assert(m3g_draw_triangle(g,tri,a,0xff123456,false,false,false));af(v,0,5);af(v,1,5);assert(m3g_draw_triangle(g,tri,a,0xff123456,false,false,false));assert(VMS_RS(g_m3g_native_pixels)-old==16);
 // Independent depth write with test disabled, then test enabled/write disabled.
 af(v,0,1);af(v,1,1);set(c,"depthTest","Z",0);fill(d,0);fill(p,0);assert(m3g_draw_triangle(g,tri,a,0xff987654,false,false,false));assert(VMS_HEAP(g_heap)[d].elems.get(9).i==10);
 set(c,"depthTest","Z",1);set(c,"depthWrite","Z",0);fill(d,11);assert(m3g_draw_triangle(g,tri,a,0xff987654,false,false,false));assert(VMS_HEAP(g_heap)[d].elems.get(9).i==11);
 fill(d,9);fill(p,0);assert(m3g_draw_triangle(g,tri,a,0xff987654,false,false,false));assert(VMS_HEAP(g_heap)[p].elems.get(9).i==0);
 // Alpha threshold precedes both color and depth writes; masks are independent.
 fill(d,32767);set(c,"depthWrite","Z",1);set(c,"alphaThreshold","F",f_bits(0.5));assert(m3g_draw_triangle(g,tri,a,0x20102030,false,false,false));assert(VMS_HEAP(g_heap)[d].elems.get(9).i==32767);
 set(c,"alphaThreshold","F",f_bits(0));set(c,"alphaWrite","Z",0);fill(p,0x66123456);assert(m3g_draw_triangle(g,tri,a,0xee987654,false,false,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==0x66987654u);
 set(c,"alphaWrite","Z",1);set(c,"colorWrite","Z",0);assert(m3g_draw_triangle(g,tri,a,0xee123456,false,false,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==0xee987654u);set(c,"colorWrite","Z",1);
 // All compositing formulas, vertex color interpolation and viewport origin/clip.
 for(int mode=64;mode<=68;++mode){set(c,"blending","I",mode);fill(p,0x80101010);assert(m3g_draw_triangle(g,tri,a,0x80402010,false,false,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==m3g_composite(0x80101010,0x80402010,128,mode));}
 set(c,"blending","I",68);assert(m3g_draw_triangle(g,tri,a,0,true,false,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==0xff102030u);
 set(g,"viewClipL","I",2);set(g,"originX","I",1);fill(p,0);assert(m3g_draw_triangle(g,tri,a,0xff123456,false,false,false));assert(VMS_HEAP(g_heap)[p].elems.get(9).i==0);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(11).i)==0xff123456u);set(g,"originX","I",0);set(g,"viewClipL","I",0);
 // Exact descriptors: ACTIVE_TEXTURE_UNITS is B and textures static reference array.
 int tx=object(),im=object(),ip=array("[I",4),ta=array("[Ljavax/microedition/m3g/Texture2D;",4),uv=floats({0.5,0.5,0.5,0.5,0.5,0.5}),uvs=array("[[F",1);
 VMS_HEAP(g_heap)[ta].elems.set(0,Value::of_ref(tx));VMS_HEAP(g_heap)[uvs].elems.set(0,Value::of_ref(uv));fr(tri,"t","[[F",uvs);fr(g,"textures","[Ljavax/microedition/m3g/Texture2D;",ta,true);fi(g,"ACTIVE_TEXTURE_UNITS","B",1,true);assert(m3g_field(g,"ACTIVE_TEXTURE_UNITS","I")==nullptr);
 fi(tx,"levelFilter","I",208);fi(tx,"imageFilter","I",210);fi(tx,"wraps","I",241);fi(tx,"wrapt","I",240);fi(tx,"blending","I",228);fi(tx,"blendColor","I",0);fr(tx,"texImage","Ljavax/microedition/m3g/Image2D;",im);
 fi(im,"width","I",2);fi(im,"height","I",2);fi(im,"format","I",100);fr(im,"image","[I",ip);VMS_HEAP(g_heap)[ip].elems.set(0,Value::of_i(0xffff0000));VMS_HEAP(g_heap)[ip].elems.set(1,Value::of_i(0xff00ff00));VMS_HEAP(g_heap)[ip].elems.set(2,Value::of_i(0xff0000ff));VMS_HEAP(g_heap)[ip].elems.set(3,Value::of_i(0xffffffff));
 assert(m3g_draw_triangle(g,tri,a,0,true,true,true));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==0xffff0000u);
 set(tx,"imageFilter","I",209);for(int i=0;i<6;++i)af(uv,i,1);assert(m3g_draw_triangle(g,tri,a,0,true,true,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==0xff7f7f7fu);
 set(tx,"imageFilter","I",210);for(int i=0;i<6;i+=2){af(uv,i,-0.1f);af(uv,i+1,5);}assert(m3g_draw_triangle(g,tri,a,0,true,true,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(9).i)==0xffffffffu);
 // Unsupported states must decline without partially changing target/depth.
 int savedP=VMS_HEAP(g_heap)[p].elems.get(9).i,savedD=VMS_HEAP(g_heap)[d].elems.get(9).i;
 for(int hint:{2,4}){set(g,"hints","I",hint);assert(!m3g_draw_triangle(g,tri,a,0,true,true,false));}set(g,"hints","I",0);set(tx,"levelFilter","I",209);assert(!m3g_draw_triangle(g,tri,a,0,true,true,false));set(tx,"levelFilter","I",208);m3g_field(a,"fog","Ljavax/microedition/m3g/Fog;")->ref=im;assert(!m3g_draw_triangle(g,tri,a,0,true,true,false));m3g_field(a,"fog","Ljavax/microedition/m3g/Fog;")->ref=0;assert(VMS_HEAP(g_heap)[p].elems.get(9).i==savedP&&VMS_HEAP(g_heap)[d].elems.get(9).i==savedD);
 // Geometry: byte/short inputs, 2/3 components, W, large output tail, matrix alias.
 int transform=object(),matrix=floats({2,0,0,10,0,3,0,20,0,0,4,30,0,0,0,1});fr(transform,"matrix","[F",matrix);
 int vector=floats({1,2,3,1});assert(m3g_transform_vectors(transform,vector));assert(fa(vector,0)==12&&fa(vector,1)==26&&fa(vector,2)==42&&fa(vector,3)==1);
 for(int type:{1,2})for(int dims:{2,3})for(bool W:{false,true}) {
  int va=object(),input=array(type==1?"[B":"[S",dims*2),out=array("[F",11);fi(va,"numVertices","I",2);fi(va,"numComponents","I",dims);fi(va,"componentType","I",type);fr(va,type==1?"vertArrayByteSize":"vertArrayShortSize",type==1?"[B":"[S",input);
  for(int i=0;i<dims*2;++i)VMS_HEAP(g_heap)[input].elems.set(i,Value::of_i(-i-1));
  fill(out,f_bits(99));assert(m3g_transform_vertices(transform,va,out,W));assert(fa(out,0)==-2+(W?10:0));assert(fa(out,1)==-6+(W?20:0));assert(fa(out,2)==(dims==3?-12:0)+(W?30:0));for(int i=8;i<11;++i)assert(fa(out,i)==99);
 }
 int texcoords=floats({1,2,3,4});assert(m3g_transform_texcoords(transform,texcoords));assert(fa(texcoords,0)==12&&fa(texcoords,1)==26&&fa(texcoords,2)==16&&fa(texcoords,3)==32);
 int identity=floats({1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});assert(m3g_multiply(matrix,identity,matrix));assert(fa(matrix,0)==2&&fa(matrix,3)==10);assert(m3g_multiply(identity,matrix,identity));assert(fa(identity,0)==2&&fa(identity,3)==10);
 // Background crop maps across the full viewport; BORDER preserves clear color.
 int bg=object();fi(bg,"cropx","I",-1);fi(bg,"cropy","I",0);fi(bg,"cropw","I",4);fi(bg,"croph","I",2);fi(bg,"modex","I",32);fi(bg,"modey","I",32);
 fi(g,"vieww","I",4);fi(g,"viewh","I",4);set(g,"viewClipR","I",4);set(g,"viewClipB","I",4);fill(p,0x12345678);
 assert(m3g_clear_to_target(g,bg,im,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(0).i)==0x12345678u);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(1).i)==0xffff0000u);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(2).i)==0xff00ff00u);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(3).i)==0x12345678u);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(17).i)==0xff0000ffu);
 set(bg,"modex","I",33);assert(m3g_clear_to_target(g,bg,im,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(0).i)==0xff00ff00u);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(3).i)==0xffff0000u);
 // Clip and Graphics origin, and dither RGB while preserving image alpha.
 set(g,"originX","I",1);set(g,"originY","I",1);set(g,"viewClipL","I",1);set(g,"viewClipT","I",1);set(g,"viewClipR","I",3);set(g,"viewClipB","I",3);set(g,"hints","I",4);fill(p,0x12345678);
 assert(m3g_clear_to_target(g,bg,im,false));assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(18).i)==0xffff0404u);assert(uint32_t(VMS_HEAP(g_heap)[p].elems.get(17).i)==0x12345678u);
 int dstimage=object(),dp=array("[I",64);fi(dstimage,"width","I",8);fi(dstimage,"height","I",8);fr(dstimage,"image","[I",dp);fr(g,"target","Ljava/lang/Object;",dstimage);set(g,"originX","I",2);set(g,"originY","I",2);set(g,"viewx","I",1);set(g,"viewy","I",1);fill(dp,0x12345678);
 assert(m3g_clear_to_target(g,bg,im,true));assert(uint32_t(VMS_HEAP(g_heap)[dp].elems.get(18).i)==0xffff0404u);assert(uint32_t(VMS_HEAP(g_heap)[dp].elems.get(36).i)==0x12345678u);
 // Stable descending sort with equal depths, leaving unused array tail unchanged.
 int order=array("[Ljavax/microedition/m3g/Triangle;",4),refs[4];float sortdepth[4]={1,5,5,-1};for(int i=0;i<4;++i){refs[i]=object();fi(refs[i],"sortZ","F",f_bits(sortdepth[i]));VMS_HEAP(g_heap)[order].elems.set(i,Value::of_ref(refs[i]));}
 assert(m3g_sort_front_to_back(order,3));assert(VMS_HEAP(g_heap)[order].elems.get(0).ref==refs[1]&&VMS_HEAP(g_heap)[order].elems.get(1).ref==refs[2]&&VMS_HEAP(g_heap)[order].elems.get(2).ref==refs[0]&&VMS_HEAP(g_heap)[order].elems.get(3).ref==refs[3]);
 set(refs[0],"sortZ","F",f_bits(std::nanf("")));assert(!m3g_sort_front_to_back(order,3));assert(VMS_HEAP(g_heap)[order].elems.get(0).ref==refs[1]);
 set(g,"viewx","I",0);set(g,"viewy","I",0);set(g,"originX","I",0);set(g,"originY","I",0);set(g,"hints","I",0);
 // Entirely offscreen extremes and degeneracy must remain memory-safe.
 for(int i:{0,4,8})af(v,i,3e38f);
 assert(m3g_draw_triangle(g,tri,a,0,false,false,false));af(v,4,-3e38f);af(v,8,0);assert(m3g_draw_triangle(g,tri,a,0,false,false,false));
 std::printf("M3G native checks passed: triangles=%llu pixels=%llu vertices=%llu matrices=%llu\n",(unsigned long long)VMS_RS(g_m3g_native_triangles),(unsigned long long)VMS_RS(g_m3g_native_pixels),(unsigned long long)VMS_RS(g_m3g_native_vertices),(unsigned long long)VMS_RS(g_m3g_native_matrices));
}
