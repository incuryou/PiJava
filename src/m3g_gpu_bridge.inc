// Main-thread-only VM boundary. No owned request contains a Java handle.
#include "m3g_gpu_bridge.h"
#include "m3g_gpu_types.h"
#include <memory>
namespace m3g_gpu_bridge {
enum class GeometryDecline:size_t {Dependency,Arguments,Types,Target,CameraLights,MaterialNormals,Polygon,Compositing,ExtentClip,Vertices,Indices,Matrices,Texture,TargetArrays,TargetUnbound,TargetImage2D,TargetMapMissing,TargetOffscreen,CameraScope,CameraDirty,Hints,Lights,DepthCompat,TargetBacking,Count};
static constexpr const char* geometry_decline_names[]={"dependency","arguments_alpha","types","target","camera_lights_scope_hints","material_normals_colors_fog","polygon","compositing","extent_clip_depth","vertices","indices","matrices","texture","target_arrays","target_unbound","target_image2d","target_map_missing","target_graphics_offscreen","scope_culled","camera_dirty","hints","lights_nonempty","depth_compat","target_backing_image_state"};
static bool diagnostic_combinations=false;
static bool diagnostic_declines=false; // opt-in counters only; no extra state scans
struct Stats {uint64_t capability_samples=0,capability_scan_ns=0,public_render_entries=0,geometry_calls=0,skin_block_calls=0,skinning_entries=0,geometry_requests=0,geometry_resolved=0,skin_calls=0,skin_meshes=0,skin_blocks=0,skin_resolved=0,skin_vertices=0,stale_results=0,captured_bytes=0,main_capture_ns=0,main_commit_ns=0,wait_wall_ns=0,api_validation_ns=0,api_validations=0,api_cache_hits=0,api_seal_ns=0,api_seal_bytes=0,api_seal_builds=0;std::array<uint64_t,size_t(GeometryDecline::Count)> geometry_declines{};};
static Stats stats;
static uint64_t pinned_bind_generation=0;
static void clear_sealed_api_source();
static uint64_t time_ns(){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}

static void bind_class(int cid,const std::vector<uint8_t>& bytes,const std::string& requested=std::string()) {
    auto& c=VMS_CL(g_classes)[cid];c.trusted_m3g_gpu=false;
    for(const auto& r:reviewed_classes)if(c.name==r.name||requested==r.name) {
        ++pinned_bind_generation;c.trusted_m3g_gpu=c.name==r.name&&rms_jar_identity(bytes.data(),bytes.size())==r.sha256;if(!c.trusted_m3g_gpu||(!requested.empty()&&requested!=c.name))clear_sealed_api_source();return;
    }
}
static bool trusted_class(int cid,const char* name) {
    return cid>=0&&size_t(cid)<VMS_CL(g_classes).size()&&VMS_CL(g_classes)[cid].trusted_m3g_gpu&&VMS_CL(g_classes)[cid].name==name;
}
struct DependencyCache {uint32_t epoch=0;uint64_t generation=0;std::vector<std::string> classpath;bool verified=false;};
static DependencyCache dependency_cache;
struct SealedApiSource {
    bool active=false;uint32_t epoch=0;std::vector<std::string> classpath;
    std::unordered_map<std::string,std::shared_ptr<const std::vector<uint8_t>>> classes;
};
static SealedApiSource sealed_api_source;
static void clear_sealed_api_source(){sealed_api_source=SealedApiSource{};dependency_cache.verified=false;}
static bool sealed_source_current(){
    if(sealed_api_source.active&&(sealed_api_source.epoch!=resver::g_epoch||sealed_api_source.classpath!=VMS_CL(g_classpath)))clear_sealed_api_source();
    return sealed_api_source.active;
}
static std::shared_ptr<const std::vector<uint8_t>> sealed_class_bytes(const std::string& name){
    if(!sealed_source_current())return {};auto found=sealed_api_source.classes.find(name);return found==sealed_api_source.classes.end()?std::shared_ptr<const std::vector<uint8_t>>{}:found->second;
}
static bool seal_api_source(){
    if(sealed_source_current())return true;
    const uint64_t began=time_ns(),generation=pinned_bind_generation;uint64_t owned_bytes=0;
    auto fail=[&](){stats.api_seal_ns+=time_ns()-began;return false;};
    SealedApiSource candidate;candidate.epoch=resver::g_epoch;candidate.classpath=VMS_CL(g_classpath);
    for(const auto& reviewed:reviewed_classes){
        auto loaded=VMS_CL(g_class_index).find(reviewed.name);if(loaded!=VMS_CL(g_class_index).end()&&!trusted_class(loaded->second,reviewed.name))return fail();
        bool found=false;std::vector<uint8_t> bytes;
        for(const auto& dir:VMS_CL(g_classpath)){std::ifstream in(dir+"/"+reviewed.name+".class",std::ios::binary);if(!in)continue;found=true;in.seekg(0,std::ios::end);auto length=in.tellg();if(length<0||length>1024*1024)return fail();in.seekg(0);bytes.assign(std::istreambuf_iterator<char>(in),{});break;}
        if(!found){auto jar=VMS_CL(g_jar_files).find(std::string(reviewed.name)+".class");if(jar!=VMS_CL(g_jar_files).end()){if(jar->second.size()>1024*1024)return fail();bytes=jar->second;found=true;}}
        if(!found||rms_jar_identity(bytes.data(),bytes.size())!=reviewed.sha256)return fail();owned_bytes+=bytes.size();if(owned_bytes>64*1024*1024)return fail();
        candidate.classes.emplace(reviewed.name,std::make_shared<const std::vector<uint8_t>>(std::move(bytes)));
    }
    if(candidate.epoch!=resver::g_epoch||candidate.classpath!=VMS_CL(g_classpath)||generation!=pinned_bind_generation)return fail();
    candidate.active=true;sealed_api_source=std::move(candidate);dependency_cache.verified=false;stats.api_seal_ns+=time_ns()-began;stats.api_seal_bytes=owned_bytes;++stats.api_seal_builds;return true;
}
// The caller must prove the whole source set immutable before enabling cache.
// Tests exercise this same cache logic against a declared-immutable fixture.
static bool geometry_dependencies_impl(bool immutable) {
    const uint64_t began=time_ns();
    auto& cache=dependency_cache;
    if(immutable&&cache.verified&&cache.epoch==resver::g_epoch&&cache.generation==pinned_bind_generation&&cache.classpath==VMS_CL(g_classpath)){
        ++stats.api_cache_hits;stats.api_validation_ns+=time_ns()-began;return true;
    }
    ++stats.api_validations;bool verified=true,all_romfs=immutable;
    for(const auto& reviewed:reviewed_classes){
        auto loaded=VMS_CL(g_class_index).find(reviewed.name);
        if(loaded!=VMS_CL(g_class_index).end()){if(!trusted_class(loaded->second,reviewed.name)){verified=false;break;}continue;}
        bool found=false,match=false;auto sealed=sealed_class_bytes(reviewed.name);
        if(sealed){found=true;match=true;}
        for(const auto& dir:VMS_CL(g_classpath)){
            if(found)break;
            std::ifstream in(dir+"/"+reviewed.name+".class",std::ios::binary);if(!in)continue;
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});found=true;match=rms_jar_identity(bytes.data(),bytes.size())==reviewed.sha256;break;
        }
        if(!found){all_romfs=false;auto jar=VMS_CL(g_jar_files).find(std::string(reviewed.name)+".class");if(jar!=VMS_CL(g_jar_files).end()){found=true;match=rms_jar_identity(jar->second.data(),jar->second.size())==reviewed.sha256;}}
        if(!found||!match){verified=false;break;}
    }
    cache.verified=verified&&all_romfs;cache.epoch=resver::g_epoch;cache.generation=pinned_bind_generation;cache.classpath=VMS_CL(g_classpath);
    stats.api_validation_ns+=time_ns()-began;return verified;
}
static bool geometry_dependencies() {
    bool immutable=sealed_source_current();
#ifdef __SWITCH__
    if(!immutable){immutable=!VMS_CL(g_classpath).empty();for(const auto& dir:VMS_CL(g_classpath))if(dir.rfind("romfs:/",0)!=0)immutable=false;}
#endif
    return geometry_dependencies_impl(immutable);
}
static void post_translate(float* m,const float bias[3]) {
    using namespace m3g_mesh242;for(int r=0;r<16;r+=4)m[r+3]=add(m[r+3],add(add(mul(m[r],bias[0]),mul(m[r+1],bias[1])),mul(m[r+2],bias[2])));
}
static void post_scale(float* m,float sx,float sy,float sz) {
    using m3g_mesh242::mul;for(int r=0;r<16;r+=4){m[r]=mul(m[r],sx);m[r+1]=mul(m[r+1],sy);m[r+2]=mul(m[r+2],sz);}
}
static bool exact(int ref,const char* name) {
    return ref>0&&size_t(ref)<VMS_HEAP(g_heap).size()&&!VMS_HEAP(g_heap)[ref].is_array&&trusted_class(VMS_HEAP(g_heap)[ref].cls,name);
}
// Validate the declaring class too: an exact Camera alone does not pin Node.scope.
static Value* field(int ref,const char* name,const char* desc) {
    if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return nullptr;
    int cid=VMS_HEAP(g_heap)[ref].cls;if(cid<0||size_t(cid)>=VMS_CL(g_classes).size()||!VMS_CL(g_classes)[cid].trusted_m3g_gpu)return nullptr;
    auto* f=find_field(cid,name,desc);
    if(!f||f->owner<0||!VMS_CL(g_classes)[f->owner].trusted_m3g_gpu||f->is_static||f->slot<0||size_t(f->slot)>=VMS_HEAP(g_heap)[ref].fields.size())return nullptr;
    return &VMS_HEAP(g_heap)[ref].fields[f->slot];
}
static bool integer(int ref,const char* n,const char* d,int& out){auto* f=field(ref,n,d);if(!f)return false;out=f->i;return true;}
static bool real(int ref,const char* n,float& out){int bits;if(!integer(ref,n,"F",bits))return false;out=bits_f(bits);return std::isfinite(out);}
static int reference(int ref,const char* n,const char* d){auto* f=field(ref,n,d);return f?f->ref:0;}
struct Stamp {
    uint32_t epoch=0,generation=0;int slot=0,cls=-1;std::string desc;size_t size=0;uint64_t layout=0,content=0;bool image=false;int width=0,height=0;size_t pixels=0,alpha=0;
};
static Stamp stamp(int ref) {
    Stamp s;if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return s;
    const auto& o=VMS_HEAP(g_heap)[ref];s.epoch=resver::g_epoch;s.slot=ref;s.generation=o.generation;s.cls=o.cls;
    if(o.is_array){s.desc=o.array_desc;s.size=o.elems.size();s.layout=o.elems.layout_version();s.content=o.elems.content_version();}
    else if(o.width||o.height||!o.pixels.empty()||!o.alpha.empty()){s.image=true;s.width=o.width;s.height=o.height;s.pixels=o.pixels.size();s.alpha=o.alpha.size();s.layout=o.image_layout_version;s.content=o.image_content_version;}return s;
}
static bool current(const Stamp& s) {
    if(!s.slot||s.epoch!=resver::g_epoch||size_t(s.slot)>=VMS_HEAP(g_heap).size())return false;
    const auto& o=VMS_HEAP(g_heap)[s.slot];if(o.generation!=s.generation||o.cls!=s.cls)return false;
    if(s.desc.empty())return !o.is_array&&o.cls>=0&&(!s.image||(o.width==s.width&&o.height==s.height&&o.pixels.size()==s.pixels&&o.alpha.size()==s.alpha&&o.image_layout_version==s.layout&&o.image_content_version==s.content));
    return o.is_array&&o.array_desc==s.desc&&o.elems.size()==s.size&&o.elems.layout_version()==s.layout&&o.elems.content_version()==s.content;
}
static bool floats(int ref,size_t count,float* out) {
    auto* a=m3g_array(ref,"[F",count);if(!a)return false;
    for(size_t i=0;i<count;++i){out[i]=bits_f(a->get(i).i);if(!std::isfinite(out[i]))return false;}return true;
}
static bool matrix(int ref,float* out) {
    return exact(ref,"javax/microedition/m3g/Transform")&&floats(reference(ref,"matrix","[F"),16,out);
}
static bool vertex(int ref,ArrayStorage*& data,int& n,int& c,int& type) {
    if(!exact(ref,"javax/microedition/m3g/VertexArray")||!integer(ref,"numVertices","I",n)||!integer(ref,"numComponents","I",c)||!integer(ref,"componentType","I",type)||n<1||n>65535||(c!=2&&c!=3)||(type!=1&&type!=2))return false;
    data=m3g_array(reference(ref,type==1?"vertArrayByteSize":"vertArrayShortSize",type==1?"[B":"[S"),type==1?"[B":"[S",size_t(n)*c);return data;
}
// Only empty light collections are supported. Java otherwise executes getters
// even without a material; skipping those calls would change exception order.
static bool no_lights(int g) {
    int lights=reference(g,"currLights","Ljavax/microedition/m3g/M3GVector;");if(!lights)return true;
    if(!exact(lights,"javax/microedition/m3g/M3GVector"))return false;
    int v=reference(lights,"elements","Ljava/util/Vector;"),count=-1;
    return exact(v,"java/util/Vector")&&integer(v,"elementCount","I",count)&&count==0;
}
static bool static_integer(int cid,const char* name,int& out) {
    if(cid<0||!VMS_CL(g_classes)[cid].trusted_m3g_gpu)return false;
    auto* f=find_field(cid,name,"I");if(!f||!f->is_static||!VMS_CL(g_classes)[f->owner].trusted_m3g_gpu||f->slot<0||size_t(f->slot)>=VMS_CL(g_classes)[f->owner].statics.size())return false;
    out=VMS_CL(g_classes)[f->owner].statics[f->slot].i;return true;
}
// Textures remain deliberately narrow: one RGB REPLACE, nearest/base-level.
// Texture transforms are main capture work in this increment, not GPU work.
static bool capture_texture(int g,int vb,int texture,int count,int perspective,int cam,m3g_gpu::GeometryRequest& p) {
    if(!exact(texture,"javax/microedition/m3g/Texture2D"))return false;
    int mode=0,level=0,filter=0,ws=0,wt=0;
    if(!integer(texture,"blending","I",mode)||mode!=228||!integer(texture,"levelFilter","I",level)||level!=208||!integer(texture,"imageFilter","I",filter)||filter!=210||!integer(texture,"wraps","I",ws)||(ws!=240&&ws!=241)||!integer(texture,"wrapt","I",wt)||(wt!=240&&wt!=241))return false;
    int image=reference(texture,"texImage","Ljavax/microedition/m3g/Image2D;"),format=0,w=0,h=0;
    if(!exact(image,"javax/microedition/m3g/Image2D")||!integer(image,"format","I",format)||format!=99||!integer(image,"width","I",w)||!integer(image,"height","I",h)||w<1||h<1||w>1024||h>1024||uint64_t(w)*h>262144)return false;
    int pixels=reference(image,"image","[I");if(pixels==reference(g,"rasterData","[I"))return false;
    auto* raw=m3g_array(pixels,"[I",size_t(w)*h);if(!raw||raw->size()!=size_t(w)*h)return false;
    auto* coords=m3g_array(reference(vb,"texCoords","[Ljavax/microedition/m3g/VertexArray;"),"[Ljavax/microedition/m3g/VertexArray;",4);
    auto* scales=m3g_array(reference(vb,"texCoordScale","[F"),"[F",4);auto* biases=m3g_array(reference(vb,"texCoordBias","[[F"),"[[F",4);
    if(!coords||!scales||!biases)return false;
    int n,c,type;ArrayStorage* data=nullptr;if(!vertex(coords->get(0).ref,data,n,c,type)||c!=2||n!=count)return false;
    float bias[3]={},previous[4],sc=bits_f(scales->get(0).i);
    if(!std::isfinite(sc)||!floats(biases->get(0).ref,2,bias)||!floats(reference(g,"texScaleBias","[F"),4,previous))return false;
    // getTexCoords(c=2) deliberately leaves scaleBias[3] unchanged. Preserve
    // that previous Java scratch value when a texture transform mixes Z to XY.
    bias[2]=previous[3];
    float tex[16],component[16],composite[16];m3g_mesh242::identity_core(tex);post_scale(tex,float(w),float(h),1);m3g_mesh242::identity_core(composite);
    for(const char* name:{"translate","rotate","scale","matrix"}){int transform=reference(texture,name,"Ljavax/microedition/m3g/Transform;");if(!matrix(transform,component))return false;m3g_mesh242::multiply_core(composite,component,composite);}
    m3g_mesh242::multiply_core(tex,composite,tex);post_translate(tex,bias);post_scale(tex,sc,sc,sc);
    int projection=0;if(!integer(cam,"projMode","I",projection))return false;
    p.perspective_correct=perspective&&projection!=49;
    // The GPU shader interpolates smoothly. Affine W is the only supported
    // non-perspective case; projective affine-UV Java paths retain bytecode.
    if(!p.perspective_correct)for(int i=12;i<16;++i)if(p.model_view[i]!=(i==15?1.0f:0.0f)||p.projection[i]!=(i==15?1.0f:0.0f))return false;
    p.uv.resize(size_t(n)*2);
    for(int v=0;v<n;++v){float x=float(data->get(size_t(v)*2).i),y=float(data->get(size_t(v)*2+1).i);
        for(int j=0;j<2;++j){float value=m3g_float::dot4(tex[j*4],x,tex[j*4+1],y,tex[j*4+2],0.0f,tex[j*4+3],1.0f);if(!std::isfinite(value)||value<-32767||value>32767)return false;p.uv[size_t(v)*2+j]=value;}}
    p.texture_width=uint32_t(w);p.texture_height=uint32_t(h);p.wrap_s=ws==241;p.wrap_t=wt==241;p.texture.resize(size_t(w)*h);
    for(size_t i=0;i<p.texture.size();++i){uint32_t color=uint32_t(raw->get(i).i);if((color>>24)!=255)return false;p.texture[i]=color;}
    return true;
}
// Explicit, read-only exploration. These keys contain no Java references.
static constexpr const char* capability_names[]={"target_kind","native_width","native_height","camera_mode","camera_dirty","light_count","light_modes_mask","null_lights","unknown_lights","material","normals","colors","fog","depth_enabled","depth_test","depth_write","cull","winding","shading","perspective","hints","texture_mask","coords_mask","texture0_blend","texture0_format","texture0_filter","texture0_level","alpha_factor","scope_intersects","lighting_effective","light_transform_count","foreign_vector_monitor","ambient_lights","directional_lights","omni_lights","spot_lights","unknown_light_transforms","vertex_count","triangle_count","position_components","position_type"};
using CapabilityKey=std::array<int,sizeof(capability_names)/sizeof(capability_names[0])>;
static std::map<CapabilityKey,uint64_t> capability_combinations;
static uint64_t capability_dropped=0;static uint32_t capability_epoch=0;
static void collect_capability(const Value* a,int n){
    const uint64_t began=time_ns();if(capability_epoch!=resver::g_epoch){capability_combinations.clear();capability_dropped=0;capability_epoch=resver::g_epoch;}
    CapabilityKey k;k.fill(-1);++stats.capability_samples;
    auto read=[](int obj,const char* name,const char* desc,int fallback=-1){int value;return integer(obj,name,desc,value)?value:fallback;};
    auto vector_data=[&](int wrapper,int& count,bool& foreign)->ArrayStorage*{if(!exact(wrapper,"javax/microedition/m3g/M3GVector"))return nullptr;int v=reference(wrapper,"elements","Ljava/util/Vector;");if(!exact(v,"java/util/Vector"))return nullptr;count=read(v,"elementCount","I");auto m=VMS_EXEC(g_monitors).find(v);foreign=m!=VMS_EXEC(g_monitors).end()&&m->second.owner&&m->second.owner!=&current_context();if(count<0||count>64)return nullptr;return m3g_array(reference(v,"elementData","[Ljava/lang/Object;"),"[Ljava/lang/Object;",size_t(count));};
    if(n==7&&exact(a[0].ref,"javax/microedition/m3g/Graphics3D")){
        int g=a[0].ref,vb=a[1].ref,app=a[3].ref,target=reference(g,"target","Ljava/lang/Object;"),cam=reference(g,"currCam","Ljavax/microedition/m3g/Camera;");k[0]=target?4:0;
        auto mapping=VMS_RS(g_gfx_target).find(target);if(exact(target,"javax/microedition/lcdui/Graphics")&&mapping!=VMS_RS(g_gfx_target).end()){k[0]=mapping->second?2:1;if(!mapping->second){k[1]=VMS_RS(g_screen_w);k[2]=VMS_RS(g_screen_h);}else if(mapping->second>0&&size_t(mapping->second)<VMS_HEAP(g_heap).size()){const auto& o=VMS_HEAP(g_heap)[mapping->second];k[1]=o.width;k[2]=o.height;}}
        else if(exact(target,"javax/microedition/m3g/Image2D")){k[0]=3;k[1]=read(target,"width","I");k[2]=read(target,"height","I");}
        k[3]=read(cam,"projMode","I");k[4]=read(cam,"dirtyMatrix","Z");int material=reference(app,"material","Ljavax/microedition/m3g/Material;"),normals=reference(vb,"normals","Ljavax/microedition/m3g/VertexArray;");k[9]=material!=0;k[10]=normals!=0;k[11]=reference(vb,"colors","Ljavax/microedition/m3g/VertexArray;")!=0;k[12]=reference(app,"fog","Ljavax/microedition/m3g/Fog;")!=0;k[13]=read(g,"depthEnabled","Z");
        int comp=reference(app,"compositingMode","Ljavax/microedition/m3g/CompositingMode;");if(!comp)comp=reference(g,"defaultCompositing","Ljavax/microedition/m3g/CompositingMode;");k[14]=read(comp,"depthTest","Z");k[15]=read(comp,"depthWrite","Z");
        int pm=reference(app,"polygonMode","Ljavax/microedition/m3g/PolygonMode;");k[16]=pm?read(pm,"culling","I"):160;k[17]=pm?read(pm,"winding","I"):168;k[18]=pm?read(pm,"shading","I"):165;k[19]=pm?read(pm,"perspectiveCorrection","Z"):0;k[20]=read(g,"hints","I");k[27]=a[6].i;int scope=read(cam,"scope","I",0);k[28]=(scope&a[5].i)!=0;
        bool foreign=false;int lights=-1;auto* ld=vector_data(reference(g,"currLights","Ljavax/microedition/m3g/M3GVector;"),lights,foreign);k[5]=lights;k[31]=foreign?1:0;k[6]=k[7]=k[8]=0;for(size_t j=32;j<=35;++j)k[j]=0;
        if(ld)for(int i=0;i<lights;++i){int light=ld->get(size_t(i)).ref;if(!light){++k[7];continue;}int mode=read(light,"mode","I");if(mode>=128&&mode<=131){k[6]|=1<<(mode-128);++k[32+mode-128];}else ++k[8];}
        int transforms=-1;bool tf=false;auto* td=vector_data(reference(g,"currLightTrans","Ljavax/microedition/m3g/M3GVector;"),transforms,tf);k[30]=transforms;k[31]|=tf?1:0;k[36]=0;if(td)for(int i=0;i<transforms;++i){int t=td->get(size_t(i)).ref;if(t&&!exact(t,"javax/microedition/m3g/Transform"))++k[36];}
        k[29]=material&&normals&&lights>0;k[21]=k[22]=0;auto* textures=m3g_array(reference(app,"textures","[Ljavax/microedition/m3g/Texture2D;"),"[Ljavax/microedition/m3g/Texture2D;",4);auto* coords=m3g_array(reference(vb,"texCoords","[Ljavax/microedition/m3g/VertexArray;"),"[Ljavax/microedition/m3g/VertexArray;",4);
        for(int unit=0;unit<4;++unit){if(textures&&textures->get(size_t(unit)).ref)k[21]|=1<<unit;if(coords&&coords->get(size_t(unit)).ref)k[22]|=1<<unit;}
        if(textures&&textures->get(0).ref){int t=textures->get(0).ref;k[23]=read(t,"blending","I");k[25]=read(t,"imageFilter","I");k[26]=read(t,"levelFilter","I");k[24]=read(reference(t,"texImage","Ljavax/microedition/m3g/Image2D;"),"format","I");}
        int positions=reference(vb,"positions","Ljavax/microedition/m3g/VertexArray;");k[37]=read(positions,"numVertices","I");int indices=read(a[2].ref,"indexCount","I");k[38]=indices>=0?indices/3:-1;k[39]=read(positions,"numComponents","I");k[40]=read(positions,"componentType","I");
    }
    auto found=capability_combinations.find(k);if(found!=capability_combinations.end())++found->second;else if(capability_combinations.size()<128)capability_combinations.emplace(k,1);else ++capability_dropped;
    stats.capability_scan_ns+=time_ns()-began;
}
struct CameraCommit {
    Stamp object,params,cache,scratch_transform,scratch;int mode=0,dirty=0;
    std::array<float,16> projection{};
};
static bool camera_current(const CameraCommit& c,int g){
    int mode=0,dirty=0;
    return current(c.object)&&current(c.params)&&current(c.cache)&&current(c.scratch_transform)&&current(c.scratch)&&reference(g,"currCam","Ljavax/microedition/m3g/Camera;")==c.object.slot&&reference(c.object.slot,"params","[F")==c.params.slot&&reference(c.object.slot,"projMatrix","[F")==c.cache.slot&&reference(g,"projectionMatrix","Ljavax/microedition/m3g/Transform;")==c.scratch_transform.slot&&reference(c.scratch_transform.slot,"matrix","[F")==c.scratch.slot&&integer(c.object.slot,"projMode","I",mode)&&mode==c.mode&&integer(c.object.slot,"dirtyMatrix","Z",dirty)&&dirty==c.dirty;
}
static bool camera_projection(int g,int cam,int vb,int app,int transform,CameraCommit& out){
    out.object=stamp(cam);if(!exact(cam,"javax/microedition/m3g/Camera")||!integer(cam,"projMode","I",out.mode)||!integer(cam,"dirtyMatrix","Z",out.dirty)||(out.mode!=48&&out.mode!=49&&out.mode!=50))return false;
    int params=reference(cam,"params","[F"),cache=reference(cam,"projMatrix","[F"),scratch_transform=reference(g,"projectionMatrix","Ljavax/microedition/m3g/Transform;"),scratch=reference(scratch_transform,"matrix","[F");
    auto* pa=m3g_array(params,"[F",4);auto* ca=m3g_array(cache,"[F",16);auto* sa=m3g_array(scratch,"[F",16);
    if(!pa||pa->size()!=4||!ca||ca->size()!=16||!sa||sa->size()!=16||!exact(scratch_transform,"javax/microedition/m3g/Transform")||params==cache||params==scratch||cache==scratch)return false;
    auto aliases=[&](int a){return a&&(a==params||a==cache||a==scratch);};
    // A projection write precedes the original vertex/texture matrix reads.
    // Reject private-array aliases rather than changing that evaluation order.
    for(const auto& f:VMS_CL(g_classes)[VMS_HEAP(g_heap)[g].cls].fields){if(f.is_static)continue;
        if(f.desc=="[F"&&aliases(reference(g,f.name.c_str(),"[F")))return false;
        if(f.desc=="Ljavax/microedition/m3g/Transform;"&&f.name!="projectionMatrix"){int t=reference(g,f.name.c_str(),f.desc.c_str());if(aliases(reference(t,"matrix","[F")))return false;}}
    if(aliases(reference(vb,"positionBias","[F"))||aliases(reference(vb,"texCoordScale","[F"))||aliases(reference(transform,"matrix","[F")))return false;
    auto* biases=m3g_array(reference(vb,"texCoordBias","[[F"),"[[F",4);if(!biases)return false;for(size_t i=0;i<4;++i)if(aliases(biases->get(i).ref))return false;
    auto* textures=m3g_array(reference(app,"textures","[Ljavax/microedition/m3g/Texture2D;"),"[Ljavax/microedition/m3g/Texture2D;",4);if(!textures)return false;
    for(size_t i=0;i<4;++i){int texture=textures->get(i).ref;if(!texture)continue;for(const char* name:{"translate","rotate","scale","matrix"}){int t=reference(texture,name,"Ljavax/microedition/m3g/Transform;");if(aliases(reference(t,"matrix","[F")))return false;}}
    out.params=stamp(params);out.cache=stamp(cache);out.scratch_transform=stamp(scratch_transform);out.scratch=stamp(scratch);
    if(!out.dirty||out.mode==48){if(!floats(cache,16,out.projection.data()))return false;for(float f:out.projection)if(f!=0&&std::fpclassify(f)==FP_SUBNORMAL)return false;return true;}
    float p[4];if(!floats(params,4,p)||p[2]==p[3])return false;for(float f:p)if(f!=0&&std::fpclassify(f)==FP_SUBNORMAL)return false;
    using namespace m3g_mesh242;float h=p[0];
    if(out.mode==50){float angle=divf(mul(p[0],0.017453292f),2.0f);float cosine=float(std::cos(double(angle)));if(cosine==0)return false;h=divf(float(std::sin(double(angle))),cosine);}
    float w=mul(p[1],h),d=sub(p[3],p[2]),b=add(p[2],p[3]);
    if(!std::isfinite(h)||!std::isfinite(w)||!std::isfinite(d)||!std::isfinite(b)||h==0||w==0||d==0)return false;
    out.projection.fill(0);auto& m=out.projection;
    if(out.mode==49){m[0]=divf(2,w);m[5]=divf(2,h);m[10]=divf(-2,d);m[11]=divf(-b,d);m[15]=1;}
    else {m[0]=divf(1,w);m[5]=divf(1,h);m[10]=divf(-b,d);m[11]=divf(mul(mul(-2,p[2]),p[3]),d);m[14]=-1;}
    for(float f:m)if(!std::isfinite(f)||(f!=0&&std::fpclassify(f)==FP_SUBNORMAL))return false;return true;
}
static void publish_camera(const CameraCommit& c){
    if(c.dirty){if(c.mode!=48)for(size_t i=0;i<16;++i)VMS_HEAP(g_heap)[c.cache.slot].elems.set(i,Value::of_i(f_bits(c.projection[i])));*field(c.object.slot,"dirtyMatrix","Z")=Value::of_i(0);}
    for(size_t i=0;i<16;++i)VMS_HEAP(g_heap)[c.scratch.slot].elems.set(i,Value::of_i(f_bits(c.projection[i])));
}
struct GeometryCommit {Stamp object,target,backing,color,depth;CameraCommit camera;int width=0,height=0;};
static bool geometry_current(const GeometryCommit& c) {
    auto mapping=VMS_RS(g_gfx_target).find(c.target.slot);int w=0,h=0;
    if(!current(c.object)||!current(c.target)||!current(c.color)||!current(c.depth)||!camera_current(c.camera,c.object.slot)||mapping==VMS_RS(g_gfx_target).end()||mapping->second!=c.backing.slot||reference(c.object.slot,"target","Ljava/lang/Object;")!=c.target.slot||reference(c.object.slot,"rasterData","[I")!=c.color.slot||reference(c.object.slot,"depthBuffer","[S")!=c.depth.slot||!static_integer(c.object.cls,"canvasWidth",w)||!static_integer(c.object.cls,"canvasHeight",h)||w!=c.width||h!=c.height)return false;
    if(!c.backing.slot)return VMS_RS(g_screen_w)==c.width&&VMS_RS(g_screen_h)==c.height;
    int mutable_image=0;return current(c.backing)&&exact(c.backing.slot,"javax/microedition/lcdui/Image")&&integer(c.backing.slot,"mutable","Z",mutable_image)&&mutable_image&&VMS_HEAP(g_heap)[c.backing.slot].alpha.empty();
}
// Capture is synchronous; the owner receives only the resulting native values.
static bool capture_geometry(const Value* a,int n,m3g_gpu::GeometryRequest& p,GeometryCommit& commit,GeometryDecline* declined=nullptr) {
    auto stage=[&](GeometryDecline reason){if(declined)*declined=reason;};stage(GeometryDecline::Arguments);
    if(n!=7||a[6].i!=256)return false;
    stage(GeometryDecline::Types);
    const int g=a[0].ref,vb=a[1].ref,ib=a[2].ref,app=a[3].ref,tr=a[4].ref;
    if(!exact(g,"javax/microedition/m3g/Graphics3D")||!exact(vb,"javax/microedition/m3g/VertexBuffer")||!exact(ib,"javax/microedition/m3g/TriangleStripArray")||!exact(app,"javax/microedition/m3g/Appearance"))return false;
    // An absent map entry must never accidentally become the screen target.
    stage(GeometryDecline::Target);
    int target=reference(g,"target","Ljava/lang/Object;");auto mapping=VMS_RS(g_gfx_target).find(target);
    if(!exact(target,"javax/microedition/lcdui/Graphics")){if(declined){if(!target)stage(GeometryDecline::TargetUnbound);else if(exact(target,"javax/microedition/m3g/Image2D"))stage(GeometryDecline::TargetImage2D);}return false;}
    if(mapping==VMS_RS(g_gfx_target).end()){stage(GeometryDecline::TargetMapMissing);return false;}
    int backing=mapping->second,target_width=VMS_RS(g_screen_w),target_height=VMS_RS(g_screen_h);
    if(backing){stage(GeometryDecline::TargetBacking);int mutable_image=0;
        if(!exact(backing,"javax/microedition/lcdui/Image")||!integer(backing,"mutable","Z",mutable_image)||!mutable_image)return false;
        const auto& image=VMS_HEAP(g_heap)[backing];target_width=image.width;target_height=image.height;
        if(target_width<1||target_height<1||target_width>800||target_height>800||uint64_t(target_width)*target_height>409600||image.pixels.size()!=size_t(target_width)*target_height*3||!image.alpha.empty())return false;
    }
    stage(GeometryDecline::CameraLights);
    int cam=reference(g,"currCam","Ljavax/microedition/m3g/Camera;"),scope=0,dirty=1,hints=0;
    if(!exact(cam,"javax/microedition/m3g/Camera")||!integer(cam,"scope","I",scope))return false;
    if(!(scope&a[5].i)){stage(GeometryDecline::CameraScope);return false;}
    if(!integer(cam,"dirtyMatrix","Z",dirty)){stage(GeometryDecline::CameraDirty);return false;}
    if(!integer(g,"hints","I",hints)||hints!=0){stage(GeometryDecline::Hints);return false;}
    if(!no_lights(g)){stage(GeometryDecline::Lights);return false;}
    int projection_mode=0;if(!integer(cam,"projMode","I",projection_mode)||(projection_mode!=48&&projection_mode!=49&&projection_mode!=50))return false;
    if(projection_mode!=48){float params[4];if(!floats(reference(cam,"params","[F"),4,params)||params[2]==params[3])return false;}
    stage(GeometryDecline::MaterialNormals);
    // No observable getter calls, exceptions or writes happen during probing.
    if(reference(app,"material","Ljavax/microedition/m3g/Material;")||reference(app,"fog","Ljavax/microedition/m3g/Fog;")||reference(vb,"normals","Ljavax/microedition/m3g/VertexArray;")||reference(vb,"colors","Ljavax/microedition/m3g/VertexArray;"))return false;
    stage(GeometryDecline::Polygon);
    int pm=reference(app,"polygonMode","Ljavax/microedition/m3g/PolygonMode;"),cull=160,winding=168,perspective=0;
    if(!exact(pm,"javax/microedition/m3g/PolygonMode")||!integer(pm,"culling","I",cull)||cull!=162||!integer(pm,"winding","I",winding)||winding!=168||!integer(pm,"perspectiveCorrection","Z",perspective))return false;
    stage(GeometryDecline::Compositing);
    int comp=reference(app,"compositingMode","Ljavax/microedition/m3g/CompositingMode;");if(!comp)comp=reference(g,"defaultCompositing","Ljavax/microedition/m3g/CompositingMode;");
    int blend=0,cw=0,aw=0,dt=0,dw=0,de=0;float threshold=0,offset=0,factor=0;
    if(!exact(comp,"javax/microedition/m3g/CompositingMode")||!integer(comp,"blending","I",blend)||blend!=68||!integer(comp,"colorWrite","Z",cw)||!cw||!integer(comp,"alphaWrite","Z",aw)||!aw||!integer(comp,"depthTest","Z",dt)||!integer(comp,"depthWrite","Z",dw)||!integer(g,"depthEnabled","Z",de)||!real(comp,"alphaThreshold",threshold)||threshold!=0||!real(comp,"depthOffsetUnits",offset)||offset!=0||!real(comp,"depthOffsetFactor",factor)||factor!=0)return false;
    // The historical constant-depth native mismatch is corrected separately.
    // General interpolation/coverage parity remains unproven: strict excludes depth.
    bool allow_depth=false;
#ifndef __SWITCH__
    allow_depth=m3g_gpu::test_allow_depth_experiment;
#endif
    if(de&&!allow_depth){stage(GeometryDecline::DepthCompat);return false;}
    stage(GeometryDecline::ExtentClip);
    int w=0,h=0,cid=VMS_HEAP(g_heap)[g].cls;
    if(!static_integer(cid,"canvasWidth",w)||!static_integer(cid,"canvasHeight",h)||w<=0||h<=0||w>800||h>800||uint64_t(w)*h>409600||w!=target_width||h!=target_height)return false;
    p.width=uint32_t(w);p.height=uint32_t(h);
    int ox,oy,vx,vy,vw,vh,l,t,r,b;
    if(!integer(g,"originX","I",ox)||!integer(g,"originY","I",oy)||!integer(g,"viewx","I",vx)||!integer(g,"viewy","I",vy)||!integer(g,"vieww","I",vw)||!integer(g,"viewh","I",vh)||!integer(g,"viewClipL","I",l)||!integer(g,"viewClipT","I",t)||!integer(g,"viewClipR","I",r)||!integer(g,"viewClipB","I",b)||vw<=0||vh<=0||vw>1024||vh>1024||l<0||t<0||r<l||b<t||r>vw||b>vh)return false;
    int64_t x=int64_t(ox)+vx,y=int64_t(oy)+vy;
    if(x<0||y<0||x+vw>w||y+vh>h)return false;
    p.viewport={int(x),int(y),vw,vh};p.clip={int(x)+l,int(y)+t,r-l,b-t};
    if(!real(g,"near",p.near_depth)||!real(g,"far",p.far_depth)||p.near_depth<0||p.near_depth>1||p.far_depth<0||p.far_depth>1)return false;
    // Existing native and Java raster paths disagree on depth writes when
    // testing is disabled. Keep that unproved state entirely in the old path.
    if(de&&!dt)return false;
    p.depth_test=de&&dt;p.depth_write=de&&dt&&dw;
    stage(GeometryDecline::Vertices);
    int color=0;if(!integer(vb,"defaultColor","I",color)||(uint32_t(color)>>24)!=255)return false;p.default_color=uint32_t(color);
    int posref=reference(vb,"positions","Ljavax/microedition/m3g/VertexArray;"),nv,nc,type;ArrayStorage* positions=nullptr;
    if(!vertex(posref,positions,nv,nc,type))return false;
    p.positions.resize(size_t(nv)*3);for(int v=0;v<nv;++v){p.positions[3*v]=float(positions->get(size_t(v)*nc).i);p.positions[3*v+1]=float(positions->get(size_t(v)*nc+1).i);p.positions[3*v+2]=nc==3?float(positions->get(size_t(v)*nc+2).i):0;}
    stage(GeometryDecline::Indices);
    auto* indices=m3g_array(reference(ib,"indices","[I"),"[I",0);int ni=0;
    if(!indices||!integer(ib,"indexCount","I",ni)||ni<3||ni%3||ni!=int(indices->size())||ni>196605)return false;
    p.indices.resize(size_t(ni));for(int i=0;i<ni;++i){int idx=indices->get(i).i;if(idx<0||idx>=nv)return false;p.indices[i]=uint32_t(idx);}
    stage(GeometryDecline::Matrices);
    m3g_mesh242::identity_core(p.model_view.data());
    float tmp[16],scale=0,bias[3];int inv=reference(g,"currCamTransInv","Ljavax/microedition/m3g/Transform;");
    if(inv){if(!matrix(inv,tmp))return false;m3g_mesh242::multiply_core(p.model_view.data(),tmp,p.model_view.data());}
    if(tr){if(!matrix(tr,tmp))return false;m3g_mesh242::multiply_core(p.model_view.data(),tmp,p.model_view.data());}
    if(!real(vb,"positionScale",scale)||!floats(reference(vb,"positionBias","[F"),3,bias))return false;
    if(!camera_projection(g,cam,vb,app,tr,commit.camera)){stage(GeometryDecline::CameraDirty);return false;}p.projection=commit.camera.projection;
    // Exactly Java tr: identity*projection*modelView, then the specialized
    // postTranslate and postScale bodies. Never reassociate scale/bias into MV.
    m3g_mesh242::identity_core(p.clip_transform.data());
    m3g_mesh242::multiply_core(p.clip_transform.data(),p.projection.data(),p.clip_transform.data());
    m3g_mesh242::multiply_core(p.clip_transform.data(),p.model_view.data(),p.clip_transform.data());
    post_translate(p.clip_transform.data(),bias);post_scale(p.clip_transform.data(),scale,scale,scale);p.canonical_clip=true;
    for(float f:p.clip_transform)if(!std::isfinite(f)||std::fabs(f)>1000000.0f)return false;
    for(float f:p.model_view)if(!std::isfinite(f)||std::fabs(f)>1000000.0f)return false;
    for(float f:p.projection)if(std::fabs(f)>1000000.0f)return false;
    stage(GeometryDecline::Texture);
    auto* textures=m3g_array(reference(app,"textures","[Ljavax/microedition/m3g/Texture2D;"),"[Ljavax/microedition/m3g/Texture2D;",4);
    if(!textures||textures->size()!=4)return false;
    for(size_t i=1;i<4;++i)if(textures->get(i).ref)return false;
    if(textures->get(0).ref&&!capture_texture(g,vb,textures->get(0).ref,nv,perspective,cam,p))return false;
    stage(GeometryDecline::TargetArrays);
    int colors=reference(g,"rasterData","[I"),depth=reference(g,"depthBuffer","[S");auto* ca=m3g_array(colors,"[I",size_t(w)*h);auto* da=m3g_array(depth,"[S",size_t(w)*h);
    if(!ca||!da||colors==depth||ca->size()!=size_t(w)*h)return false;
    p.colors.resize(size_t(w)*h);p.depth.resize(size_t(w)*h);
    for(size_t i=0;i<size_t(w)*h;++i){p.colors[i]=uint32_t(ca->get(i).i);p.depth[i]=int16_t(da->get(i).i);}
    commit.object=stamp(g);commit.color=stamp(colors);commit.depth=stamp(depth);commit.target=stamp(target);commit.backing=stamp(backing);commit.width=w;commit.height=h;return m3g_gpu::validate(p).empty();
}
struct SkinCommit {Stamp output,normal,bounds;int first=0,count=0;uint64_t weights=0;std::array<float,6> box{};};
static bool capture_skin(const Value* a,int n,m3g_gpu::SkinRequest& p,SkinCommit& c) {
    if(n!=10||!VMS_RS(g_m3g_mesh242_enabled))return false;
    int nv,nc,type,nn=0,ncomp=0,nt=0,first=a[8].i,count=a[9].i;ArrayStorage* pos=nullptr;ArrayStorage* normals=nullptr;
    if(count<1||count>128||first<0||!vertex(a[0].ref,pos,nv,nc,type)||nc!=3||count>nv||first>nv-count)return false;
    if(a[1].ref&&(!vertex(a[1].ref,normals,nn,ncomp,nt)||nn!=nv||ncomp!=3))return false;
    if(bool(a[1].ref)!=bool(a[6].ref))return false;
    auto* matrices=m3g_array(a[2].ref,"[[F",0);auto* ranges=m3g_array(a[3].ref,"[I",0);auto* scale=m3g_array(a[4].ref,"[F",4);
    auto* output=m3g_array(a[5].ref,"[F",size_t(nv)*3);auto* normal=a[6].ref?m3g_array(a[6].ref,"[F",size_t(nv)*3):nullptr;auto* bounds=m3g_array(a[7].ref,"[F",6);
    if(!matrices||matrices->size()>256||!ranges||ranges->size()!=matrices->size()*3||!scale||!output||!bounds||(a[6].ref&&!normal))return false;
    if(output==normal||output==bounds||output==scale||normal==bounds||normal==scale||bounds==scale)return false;
    float sb[4];if(!floats(a[4].ref,4,sb)||!floats(a[7].ref,6,c.box.data()))return false;
    p.epoch=resver::g_epoch;p.positions.resize(size_t(count)*3);if(normals)p.normals.resize(size_t(count)*3);
    for(int v=0;v<count;++v)for(int j=0;j<3;++j){size_t i=size_t(first+v)*3+j;p.positions[size_t(v)*3+j]=m3g_mesh242::add(m3g_mesh242::mul(float(pos->get(i).i),sb[0]),sb[j+1]);if(normals)p.normals[size_t(v)*3+j]=float(normals->get(i).i);}
    p.matrices.resize(matrices->size()*16);
    for(size_t b=0;b<matrices->size();++b){
        int ref=matrices->get(b).ref;auto* matrix=m3g_array(ref,"[F",16);int s=ranges->get(3*b).i,k=ranges->get(3*b+1).i,w=ranges->get(3*b+2).i;
        if(!matrix||matrix==output||matrix==normal||matrix==bounds||s<0||k<1||k>nv||s>nv-k||w<=0||w>65535||!floats(ref,16,p.matrices.data()+16*b))return false;
        // Conservative preflight bounds keep weighted sums and normal squared
        // lengths finite; numerical overflow stays in the original Java path.
        for(size_t j=16*b;j<16*b+16;++j)if(std::fabs(p.matrices[j])>10000.0f)return false;
        int begin=std::max(first,s),end=std::min(first+count,s+k);
        if(end>begin){p.ranges.push_back({uint32_t(begin-first),uint32_t(end-begin),w,uint32_t(b)});c.weights+=uint64_t(end-begin);}
    }
    for(float f:p.positions)if(std::fabs(f)>100000000.0f)return false;
    if(!m3g_gpu::validate(p).empty())return false;
    c.output=stamp(a[5].ref);c.normal=stamp(a[6].ref);c.bounds=stamp(a[7].ref);c.first=first;c.count=count;return true;
}
// Return true only after the call was consumed. Failure after submission must
// never pretend it is safe to execute the same Java draw a second time.
static bool try_geometry(const Value* a,int n) {
    if(!m3g_gpu::geometry_executor)return false;
    if(diagnostic_combinations)collect_capability(a,n);
    m3g_gpu::GeometryRequest request;GeometryCommit c;
    GeometryDecline decline=GeometryDecline::Dependency;uint64_t captured=time_ns();bool accepted=geometry_dependencies()&&capture_geometry(a,n,request,c,diagnostic_declines?&decline:nullptr);stats.main_capture_ns+=time_ns()-captured;
    if(!accepted){if(diagnostic_declines)++stats.geometry_declines[size_t(decline)];return false;}
    request.epoch=resver::g_epoch; // successful capture already validated the owned packet
    const size_t area=size_t(request.width)*request.height;
    ++stats.geometry_requests;stats.captured_bytes+=request.positions.size()*sizeof(float)+request.indices.size()*sizeof(uint32_t)+request.uv.size()*sizeof(float)+request.texture.size()*sizeof(uint32_t)+request.colors.size()*sizeof(uint32_t)+request.depth.size()*sizeof(int16_t);
    uint64_t waited=time_ns();
    auto out=m3g_gpu::execute_geometry_validated(std::move(request));stats.wait_wall_ns+=time_ns()-waited;
    if(out.code==m3g_gpu::Code::Unsupported||out.code==m3g_gpu::Code::Unavailable)return false;
    if(out.code==m3g_gpu::Code::Cancelled)return !geometry_current(c);
    if(out.code!=m3g_gpu::Code::Success)die("M3G_GPU geometry_failed: "+out.reason);
    if(out.colors.size()!=area||out.depth.size()!=area)die("M3G_GPU geometry_result_shape");
    if(!geometry_current(c)){++stats.stale_results;return true;}
    uint64_t committed=time_ns();
    auto& colors=VMS_HEAP(g_heap)[c.color.slot].elems;auto& depth=VMS_HEAP(g_heap)[c.depth.slot].elems;
    for(size_t i=0;i<area;++i){colors.set(i,Value::of_i(int_bits(out.colors[i])));depth.set(i,Value::of_i(out.depth[i]));}
    publish_camera(c.camera);stats.main_commit_ns+=time_ns()-committed;++stats.geometry_resolved;
    // This private scalar is the one non-scratch Java state write at entry.
    if(auto* alpha=field(c.object.slot,"currAlphaFactor","I"))*alpha=Value::of_i(256);
    return true;
}
static bool try_skin(const Value* a,int n,Value& result) {
    if(!m3g_gpu::skin_executor)return false;
    m3g_gpu::SkinRequest request;SkinCommit c;uint64_t captured=time_ns();bool accepted=capture_skin(a,n,request,c);stats.main_capture_ns+=time_ns()-captured;if(!accepted)return false;
    ++stats.skin_blocks;stats.skin_vertices+=c.count;stats.captured_bytes+=(request.positions.size()+request.normals.size()+request.matrices.size())*sizeof(float)+request.ranges.size()*sizeof(m3g_gpu::BoneRange);
    uint64_t waited=time_ns();
    auto out=m3g_gpu::execute_skin_validated(std::move(request));stats.wait_wall_ns+=time_ns()-waited;
    if(out.code==m3g_gpu::Code::Unsupported||out.code==m3g_gpu::Code::Unavailable)return false;
    if(out.code==m3g_gpu::Code::Cancelled){if(current(c.output)&&current(c.bounds)&&(!c.normal.slot||current(c.normal)))return false;result=Value::of_i(0);return true;}
    if(out.code!=m3g_gpu::Code::Success)die("M3G_GPU skin_failed: "+out.reason);
    const size_t count=size_t(c.count)*3;
    if(out.positions.size()!=count||(c.normal.slot?out.normals.size()!=count:!out.normals.empty())||!m3g_gpu::finite(out.positions)||!m3g_gpu::finite(out.normals))die("M3G_GPU skin_result_shape");
    if(!current(c.output)||!current(c.bounds)||(c.normal.slot&&!current(c.normal))){++stats.stale_results;result=Value::of_i(0);return true;}
    uint64_t committed=time_ns();
    for(int v=0;v<c.count;++v)for(int j=0;j<3;++j){float f=out.positions[size_t(v)*3+j];c.box[2*j]=c.box[2*j]<f?c.box[2*j]:f;c.box[2*j+1]=c.box[2*j+1]>f?c.box[2*j+1]:f;}
    auto& output=VMS_HEAP(g_heap)[c.output.slot].elems;auto& bounds=VMS_HEAP(g_heap)[c.bounds.slot].elems;
    for(size_t i=0;i<count;++i){output.set(size_t(c.first)*3+i,Value::of_i(f_bits(out.positions[i])));if(c.normal.slot)VMS_HEAP(g_heap)[c.normal.slot].elems.set(size_t(c.first)*3+i,Value::of_i(f_bits(out.normals[i])));}
    for(size_t i=0;i<6;++i)bounds.set(i,Value::of_i(f_bits(c.box[i])));
    stats.main_commit_ns+=time_ns()-committed;++stats.skin_resolved;
    ++m3g_mesh242::stats.skin_calls;m3g_mesh242::stats.skin_vertices+=c.count;m3g_mesh242::stats.skin_weights+=c.weights;result=Value::of_i(1);return true;
}
// The whole coordinator shortcut is restricted to an existing posed buffer.
// Its setter can then remove/reappend dst in an already allocated references
// Vector and copy bias into its existing array without allocating Java objects.
// These publication tickets stay on main and never enter the executor packet.
struct ObjectState {Stamp key;std::vector<Value> fields;};
struct WholeSkinCommit {
    std::vector<Stamp> arrays;std::vector<ObjectState> objects;
    int posed=0,dst=0,normal=0,raw=0,raw_normal=0,bias=0,refs=0,refs_vector=0,ref_count=0,ref_index=0,type=0,normal_type=0,count=0;
    uint64_t weights=0;
};
static bool monitor_available(int ref) {
    auto m=VMS_EXEC(g_monitors).find(ref);
    return m==VMS_EXEC(g_monitors).end()||((!m->second.owner||m->second.owner==&current_context())&&m->second.waiters.empty());
}
static bool whole_current(const WholeSkinCommit& c) {
    for(const auto& a:c.arrays)if(!current(a))return false;
    for(const auto& o:c.objects){if(!current(o.key))return false;const auto& now=VMS_HEAP(g_heap)[o.key.slot].fields;if(now.size()!=o.fields.size())return false;
        for(size_t i=0;i<now.size();++i){const auto& x=now[i];const auto& y=o.fields[i];if(x.i!=y.i||x.ref!=y.ref||x.l!=y.l||x.is_long!=y.is_long)return false;}}
    return monitor_available(c.refs_vector);
}
static bool capture_whole_skin(const Value* a,int argc,m3g_gpu::SkinRequest& p,WholeSkinCommit& c) {
    if(argc!=8||!VMS_RS(g_m3g_mesh242_enabled)||bool(a[1].ref)!=bool(a[3].ref))return false;
    for(const char* name:{"java/lang/Object","java/lang/System","javax/microedition/m3g/Object3D","javax/microedition/m3g/M3GMath"}){
        auto id=VMS_CL(g_class_index).find(name);if(id==VMS_CL(g_class_index).end()||!trusted_class(id->second,name)||VMS_CL(g_classes)[id->second].init_state!=2)return false;}
    ArrayStorage *pos=nullptr,*norm=nullptr,*dst=nullptr,*dn=nullptr;int n,components,type,nn=0,nc=0,nt=0,dn_count=0,dnc=0,dnt=0,dst_count=0,dstc=0,dstt=0;
    if(!vertex(a[0].ref,pos,n,components,type)||n<=128||components!=3||!vertex(a[2].ref,dst,dst_count,dstc,dstt)||dst_count!=n||dstc!=3||dstt!=type)return false;
    if(a[1].ref&&(!vertex(a[1].ref,norm,nn,nc,nt)||nn!=n||nc!=3||!vertex(a[3].ref,dn,dn_count,dnc,dnt)||dn_count!=n||dnc!=3))return false;
    const int posed=a[4].ref;int fixed=0,length=0;
    if(!exact(posed,"javax/microedition/m3g/VertexBuffer")||reference(posed,"positions","Ljavax/microedition/m3g/VertexArray;")!=a[2].ref||!integer(posed,"fixed","Z",fixed)||!fixed||!integer(posed,"length","I",length)||length!=n)return false;
    c.posed=posed;c.dst=a[2].ref;c.normal=a[3].ref;c.type=type;c.normal_type=dnt;c.count=n;
    c.raw=reference(a[2].ref,type==1?"vertArrayByteSize":"vertArrayShortSize",type==1?"[B":"[S");
    if(dn)c.raw_normal=reference(a[3].ref,dnt==1?"vertArrayByteSize":"vertArrayShortSize",dnt==1?"[B":"[S");
    c.bias=reference(posed,"positionBias","[F");auto* bias=m3g_array(c.bias,"[F",3);if(!bias||bias->size()!=3)return false;
    int refs=reference(posed,"curReferences","Ljavax/microedition/m3g/M3GVector;");if(!exact(refs,"javax/microedition/m3g/M3GVector"))return false;
    c.refs_vector=reference(refs,"elements","Ljava/util/Vector;");
    if(!exact(c.refs_vector,"java/util/Vector")||!monitor_available(c.refs_vector)||!integer(c.refs_vector,"elementCount","I",c.ref_count)||c.ref_count<1||c.ref_count>64)return false;
    c.refs=reference(c.refs_vector,"elementData","[Ljava/lang/Object;");auto* ref_array=m3g_array(c.refs,"[Ljava/lang/Object;",size_t(c.ref_count));if(!ref_array||ref_array->size()>256)return false;
    int occurrences=0;for(int i=0;i<c.ref_count;++i)if(ref_array->get(i).ref==c.dst){++occurrences;c.ref_index=i;}if(occurrences!=1)return false;
    auto track=[&](int ref){if(ref<=0)return false;for(const auto& key:c.arrays)if(key.slot==ref)return false;c.arrays.push_back(stamp(ref));return true;};
    int source_raw=reference(a[0].ref,type==1?"vertArrayByteSize":"vertArrayShortSize",type==1?"[B":"[S");
    if(pos->size()!=size_t(n)*3||dst->size()!=size_t(n)*3||!track(source_raw)||!track(c.raw)||!track(c.bias)||!track(c.refs)||!track(a[5].ref)||!track(a[6].ref)||!track(a[7].ref))return false;
    if(norm){int source_normal=reference(a[1].ref,nt==1?"vertArrayByteSize":"vertArrayShortSize",nt==1?"[B":"[S");if(norm->size()!=size_t(n)*3||dn->size()!=size_t(n)*3||!track(source_normal)||!track(c.raw_normal))return false;}
    for(int object:std::array<int,7>{{a[0].ref,a[1].ref,a[2].ref,a[3].ref,posed,refs,c.refs_vector}})if(object){for(const auto& state:c.objects)if(state.key.slot==object)return false;c.objects.push_back({stamp(object),VMS_HEAP(g_heap)[object].fields});}
    auto* matrices=m3g_array(a[5].ref,"[[F",0);auto* ranges=m3g_array(a[6].ref,"[I",0);float sb[4];
    if(!matrices||matrices->size()>256||!ranges||ranges->size()!=matrices->size()*3||!floats(a[7].ref,4,sb))return false;
    p.postprocess=true;p.position_type=uint8_t(type);p.normal_type=norm?uint8_t(dnt):0;p.original_scale=sb[0];p.epoch=resver::g_epoch;p.positions.resize(size_t(n)*3);if(norm)p.normals.resize(size_t(n)*3);
    for(size_t i=0;i<p.positions.size();++i){p.positions[i]=m3g_mesh242::add(m3g_mesh242::mul(float(pos->get(i).i),sb[0]),sb[1+i%3]);if(std::fabs(p.positions[i])>100000000.0f)return false;if(norm)p.normals[i]=float(norm->get(i).i);}
    p.matrices.resize(matrices->size()*16);
    for(size_t b=0;b<matrices->size();++b){int matrix=matrices->get(b).ref,first=ranges->get(3*b).i,count=ranges->get(3*b+1).i,weight=ranges->get(3*b+2).i;
        if(!track(matrix)||first<0||count<1||count>n||first>n-count||weight<1||weight>65535||!floats(matrix,16,p.matrices.data()+16*b))return false;
        for(size_t j=16*b;j<16*b+16;++j)if(std::fabs(p.matrices[j])>10000.0f)return false;
        p.ranges.push_back({uint32_t(first),uint32_t(count),weight,uint32_t(b)});c.weights+=uint64_t(count);}
    return m3g_gpu::validate(p).empty();
}
static bool try_whole_skin(const Value* a,int n,Value& result) {
    if(!m3g_gpu::skin_executor)return false;
    m3g_gpu::SkinRequest request;WholeSkinCommit c;uint64_t captured=time_ns();bool accepted=capture_whole_skin(a,n,request,c);stats.main_capture_ns+=time_ns()-captured;if(!accepted)return false;
    ++stats.skin_meshes;stats.skin_vertices+=c.count;stats.captured_bytes+=(request.positions.size()+request.normals.size()+request.matrices.size())*sizeof(float)+request.ranges.size()*sizeof(m3g_gpu::BoneRange);
    uint64_t waited=time_ns();auto out=m3g_gpu::execute_skin_validated(std::move(request));stats.wait_wall_ns+=time_ns()-waited;
    if(out.code==m3g_gpu::Code::Unsupported||out.code==m3g_gpu::Code::Unavailable)return false;
    if(out.code==m3g_gpu::Code::Cancelled){if(whole_current(c))return false;result=Value::of_i(0);return true;}
    if(out.code!=m3g_gpu::Code::Success)die("M3G_GPU whole_skin_failed: "+out.reason);
    const size_t count=size_t(c.count)*3;
    if(!out.postprocessed||out.position_bytes.size()!=count*size_t(c.type)||out.normal_bytes.size()!=(c.normal?count*size_t(c.normal_type):0)||!std::isfinite(out.scale)||out.scale==0||!std::isfinite(out.bias[0])||!std::isfinite(out.bias[1])||!std::isfinite(out.bias[2]))die("M3G_GPU whole_skin_postprocess_protocol");
    uint64_t committed=time_ns();
    // All bounds/scale/bias/quantization ran against owned data on the provider.
    // Main validates publication state, then borrows bounded primitive views.
    if(!whole_current(c)){++stats.stale_results;result=Value::of_i(0);return true;}
    auto& destination=VMS_HEAP(g_heap)[c.raw].elems;
    if(destination.payload_bytes()!=out.position_bytes.size()||(c.normal&&VMS_HEAP(g_heap)[c.raw_normal].elems.payload_bytes()!=out.normal_bytes.size())||!field(c.posed,"positionScale","F"))die("M3G_GPU whole_skin_publication_storage");
    {uint8_t* view=c.type==1?destination.byte_data():destination.short_bytes_for_write();if(!view)die("M3G_GPU whole_skin_publication_storage");std::memcpy(view,out.position_bytes.data(),out.position_bytes.size());}
    auto& references=VMS_HEAP(g_heap)[c.refs].elems;for(int i=c.ref_index;i+1<c.ref_count;++i)references.set(size_t(i),references.get(size_t(i+1)));references.set(size_t(c.ref_count-1),Value::of_ref(0));references.set(size_t(c.ref_count-1),Value::of_ref(c.dst));
    *field(c.posed,"positionScale","F")=Value::of_i(f_bits(out.scale));auto& position_bias=VMS_HEAP(g_heap)[c.bias].elems;for(int j=0;j<3;++j)position_bias.set(size_t(j),Value::of_i(f_bits(out.bias[j])));
    if(c.normal){auto& target=VMS_HEAP(g_heap)[c.raw_normal].elems;uint8_t* view=c.normal_type==1?target.byte_data():target.short_bytes_for_write();if(!view)die("M3G_GPU whole_skin_publication_storage");std::memcpy(view,out.normal_bytes.data(),out.normal_bytes.size());}
    stats.main_commit_ns+=time_ns()-committed;++stats.skin_resolved;++m3g_mesh242::stats.skin_calls;m3g_mesh242::stats.skin_vertices+=c.count;m3g_mesh242::stats.skin_weights+=c.weights;
    m3g_mesh242::stats.quantize_calls+=uint64_t((count+1023)/1024)*(c.normal?2:1);m3g_mesh242::stats.quantize_values+=count*(c.normal?2:1);result=Value::of_i(1);return true;
}
static bool try_invoke(Method* method,const Value* args,int n,Value& result) {
    if(!m3g_gpu::enabled||r4::state.active||!method||method->class_id<0||size_t(method->class_id)>=VMS_CL(g_classes).size()||(method->flags&0x0020))return false;
    const auto& owner=VMS_CL(g_classes)[method->class_id];if(!owner.trusted_m3g_gpu||owner.init_state!=2)return false;
    if(diagnostic_declines&&owner.name=="javax/microedition/m3g/Graphics3D"&&method->name=="render")++stats.public_render_entries;
    if(diagnostic_declines&&owner.name=="javax/microedition/m3g/SkinnedMesh"&&method->name=="applySkinning")++stats.skinning_entries;
    if(!method->native&&!(method->flags&0x0008)&&owner.name=="javax/microedition/m3g/Graphics3D"&&method->name=="renderSubmesh"&&method->desc==render_descriptor){++stats.geometry_calls;return try_geometry(args,n);}
    if(!method->native&&(method->flags&0x0008)&&owner.name=="javax/microedition/m3g/M3GMesh242"&&method->name=="skin"&&method->desc==whole_skin_descriptor){++stats.skin_calls;return try_whole_skin(args,n,result);}
    if(method->native&&(method->flags&0x0008)&&owner.name=="javax/microedition/m3g/M3GMesh242"&&method->name=="skinBlock"&&method->desc==skin_descriptor){++stats.skin_block_calls;return try_skin(args,n,result);}
    return false;
}
} // namespace m3g_gpu_bridge
