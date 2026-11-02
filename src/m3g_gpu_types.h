#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

// Owned transport only. No Java handles, VM pointers, deko objects or live views.
namespace m3g_gpu {
inline constexpr std::array<float,16> identity{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
enum class Code { Success, Unsupported, Unavailable, Failed, Cancelled };
enum class Cull : uint32_t { None, Back, Front };
struct Rect {int32_t x=0,y=0,w=0,h=0;};
struct GeometryRequest {
    uint64_t epoch=0;
    uint32_t width=0,height=0;
    Rect viewport,clip;
    std::array<float,16> model_view=identity,projection=identity,texture_matrix=identity;
    // VM captures Java's precise matrix combination order. GPU must not
    // reassociate P*(MV*(S*v+B)) into different binary32 operations.
    bool canonical_clip=false;
    std::array<float,16> clip_transform=identity;
    std::vector<float> positions; // raw XYZ; scale/bias already folded into model_view
    std::vector<uint32_t> indices; // ordered triangle list
    std::vector<float> uv; // XY; texture_matrix yields texel coordinates
    std::vector<uint32_t> texture; // ARGB, initially opaque RGB/REPLACE only
    uint32_t texture_width=0,texture_height=0;
    bool wrap_s=true,wrap_t=true,perspective_correct=false;
    uint32_t default_color=0xffffffff;
    std::vector<uint32_t> colors; // initial entire target, ARGB
    std::vector<int16_t> depth; // signed Java short, clear=32767
    float near_depth=0,far_depth=1;
    bool depth_test=true,depth_write=true,front_cw=false;
    Cull cull=Cull::None;
};
struct GeometryResult {
    Code code=Code::Unavailable;
    std::string reason;
    std::vector<uint32_t> colors;
    std::vector<int16_t> depth;
};
struct BoneRange {uint32_t first=0,count=0;int32_t weight=0;uint32_t bone=0;};
struct SkinRequest {
    uint64_t epoch=0;
    bool postprocess=false;
    uint8_t position_type=0,normal_type=0; // 1=byte,2=short; normal0=absent
    float original_scale=1;
    std::vector<float> positions; // dequantized XYZ; ranges rebased to this block
    std::vector<float> normals; // optional original components
    std::vector<float> matrices; // ordered row-major mat4 blocks
    std::vector<BoneRange> ranges; // preserve original contribution order
};
struct SkinResult {
    Code code=Code::Unavailable;
    std::string reason;
    std::vector<float> positions,normals; // final float output, including established normal algorithm
    bool postprocessed=false;
    std::vector<uint8_t> position_bytes{},normal_bytes{}; // owned little-endian Java payload
    float scale=1;
    std::array<float,3> bias{{0,0,0}};
};
inline bool finite(const std::vector<float>& a){for(float x:a)if(!std::isfinite(x))return false;return true;}
inline bool finite(const std::array<float,16>& a){for(float x:a)if(!std::isfinite(x))return false;return true;}
inline std::string validate(const GeometryRequest& q){
    const uint64_t area=uint64_t(q.width)*q.height;
    if(!q.width||!q.height||q.width>800||q.height>800||area>409600)return "target_extent";
    if(q.colors.size()!=area||q.depth.size()!=area)return "target_storage";
    if(q.positions.empty()||q.positions.size()%3||q.positions.size()/3>65535||!finite(q.positions))return "positions";
    if(q.indices.empty()||q.indices.size()%3||q.indices.size()>196605)return "indices";
    for(auto i:q.indices)if(i>=q.positions.size()/3)return "index_range";
    auto rect=[&](Rect r){return r.w>=0&&r.h>=0&&r.x>=0&&r.y>=0&&uint64_t(r.x)+uint32_t(r.w)<=q.width&&uint64_t(r.y)+uint32_t(r.h)<=q.height;};
    if(!rect(q.viewport)||!rect(q.clip)||!q.viewport.w||!q.viewport.h)return "viewport_clip";
    if(!finite(q.model_view)||!finite(q.projection)||!finite(q.texture_matrix)||(q.canonical_clip&&!finite(q.clip_transform)))return "matrix";
    if(!std::isfinite(q.near_depth)||!std::isfinite(q.far_depth)||q.near_depth<0||q.near_depth>1||q.far_depth<0||q.far_depth>1)return "depth_range";
    if(uint32_t(q.cull)>uint32_t(Cull::Front))return "culling";
    if(!q.texture.empty()){
        if(!q.texture_width||!q.texture_height||q.texture_width>1024||q.texture_height>1024||uint64_t(q.texture_width)*q.texture_height>262144||q.texture.size()!=uint64_t(q.texture_width)*q.texture_height)return "texture_extent";
        if(q.uv.size()!=q.positions.size()/3*2||!finite(q.uv))return "texture_coordinates";
        for(auto c:q.texture)if((c>>24)!=255)return "texture_alpha";
    }else if(q.texture_width||q.texture_height||!q.uv.empty())return "texture_storage";
    if((q.default_color>>24)!=255)return "color_alpha";
    return {};
}
inline std::string validate(const SkinRequest& q){
    const size_t n=q.positions.size()/3;
    if(!n||n>65535||q.positions.size()%3||!finite(q.positions))return "positions";
    if(!q.normals.empty()&&(q.normals.size()!=q.positions.size()||!finite(q.normals)))return "normals";
    if(q.matrices.size()%16||q.matrices.size()/16>256||!finite(q.matrices)||q.ranges.size()>256)return "bones";
    for(auto r:q.ranges)if(r.weight<=0||r.bone>=q.matrices.size()/16||r.first>n||r.count>n-r.first)return "bone_range";
    if(q.postprocess&&((q.position_type!=1&&q.position_type!=2)||q.normal_type>2||
        (q.normals.empty()!=(q.normal_type==0))||!std::isfinite(q.original_scale)))return "postprocess_format";
    return {};
}
using GeometryExecutor=GeometryResult(*)(GeometryRequest&&);
using SkinExecutor=SkinResult(*)(SkinRequest&&);
inline GeometryExecutor geometry_executor=nullptr;
inline SkinExecutor skin_executor=nullptr;
#ifndef __SWITCH__
inline bool test_allow_depth_experiment=false; // host tests only; never a Switch capability
#endif
inline bool enabled=false; // explicit experiment; main-thread configuration only
// Internal owned capture has already validated; the GPU owner validates again.
inline GeometryResult execute_geometry_validated(GeometryRequest&& q){
    if(!geometry_executor)return {Code::Unavailable,"geometry_provider_unavailable",{}, {}};
    return geometry_executor(std::move(q));
}
inline SkinResult execute_skin_validated(SkinRequest&& q){
    if(!skin_executor)return {Code::Unavailable,"skin_provider_unavailable",{}, {}};
    return skin_executor(std::move(q));
}
inline GeometryResult execute_geometry(GeometryRequest&& q){
    auto error=validate(q);if(!error.empty())return {Code::Unsupported,std::move(error),{}, {}};
    return execute_geometry_validated(std::move(q));
}
inline SkinResult execute_skin(SkinRequest&& q){
    auto error=validate(q);if(!error.empty())return {Code::Unsupported,std::move(error),{}, {}};
    return execute_skin_validated(std::move(q));
}
}
