#pragma once
#include "m3g_gpu_types.h"
#include <algorithm>

namespace m3g_gpu {
namespace order_detail {
// Preserve Java binary32 operation boundaries; AArch64 must not fuse a matrix
// product or the sort key into a multiply-add with different tie rounding.
inline float add(float a,float b){volatile float r=a+b;return r;}
inline float sub(float a,float b){volatile float r=a-b;return r;}
inline float mul(float a,float b){volatile float r=a*b;return r;}
inline float div(float a,float b){volatile float r=a/b;return r;}
inline float reciprocal(float a){volatile float r=1.0f/a;return r;}
inline void transform(const std::array<float,16>& m,const float* p,float* out){
    for(unsigned r=0;r<4;++r)out[r]=add(add(add(mul(m[4*r],p[0]),mul(m[4*r+1],p[1])),mul(m[4*r+2],p[2])),mul(m[4*r+3],p[3]));
}
}
// Graphics3D builds its viewport matrix with postScale then postTranslate.
// Precompute those exact binary32 coefficients on the owner, not the VM thread.
inline std::array<float,2> geometry_depth_mapping(float near_depth,float far_depth){
    using namespace order_detail;
    const float scale=mul(mul(.5f,sub(far_depth,near_depth)),32200.0f);
    const float base=mul(near_depth,32200.0f);
    const float bias=scale!=0?mul(scale,add(1.0f,div(base,scale))):base;
    return {{scale,bias}};
}
// Mandatory owner-side draw preparation: Triangle.fromVertAndTris computes
// (z0/w0 + z1/w1 + z2/w2)*0.33333334f before the viewport transform, then
// sortFrontToBack uses a stable DESCENDING order (despite its name).
// No transformed vertex buffers are supplied to the renderer: GLSL still
// transforms the original vertices. Reuse this scan for inside-clip validation.
// Declines never change the caller's output. Call validate(q) for full DTO checks.
inline std::string prepare_geometry_order(const GeometryRequest& q,std::vector<uint32_t>& ordered){
    using namespace order_detail;
    const size_t count=q.positions.size()/3;
    if(!count||count>65535||q.positions.size()%3||!finite(q.positions))return "positions";
    if(q.indices.empty()||q.indices.size()%3||q.indices.size()>196605)return "indices";
    if(!finite(q.model_view)||!finite(q.projection)||!finite(q.clip_transform))return "matrix";
    for(auto i:q.indices)if(i>=count)return "index_range";
    std::vector<float> ndc_z(count);
    for(size_t i=0;i<count;++i){
        float p[4]={q.positions[3*i],q.positions[3*i+1],q.positions[3*i+2],1},eye[4],clip[4];
        if(q.canonical_clip)transform(q.clip_transform,p,clip);
        else {transform(q.model_view,p,eye);transform(q.projection,eye,clip);}
        if(!std::isfinite(clip[3])||clip[3]<=0)return "homogeneous_clip";
        for(unsigned j=0;j<3;++j)if(!std::isfinite(clip[j])||std::fabs(clip[j])>clip[3])return "outside_clip";
        ndc_z[i]=mul(clip[2],reciprocal(clip[3]));
        if(!std::isfinite(ndc_z[i]))return "ndc_depth";
    }
    struct Triangle {uint32_t first;float z;};std::vector<Triangle> triangles;triangles.reserve(q.indices.size()/3);
    for(size_t i=0;i<q.indices.size();i+=3){
        float z=mul(add(add(ndc_z[q.indices[i]],ndc_z[q.indices[i+1]]),ndc_z[q.indices[i+2]]),0.33333334f);
        triangles.push_back({uint32_t(i),z});
    }
    std::stable_sort(triangles.begin(),triangles.end(),[](const Triangle& a,const Triangle& b){return a.z>b.z;});
    std::vector<uint32_t> result;result.reserve(q.indices.size());
    for(auto t:triangles)for(unsigned j=0;j<3;++j)result.push_back(q.indices[t.first+j]);
    ordered.swap(result);return {};
}
}
