#pragma once
#include "m3g_gpu_types.h"
#include <cstring>
#include <limits>

namespace m3g_gpu {
namespace skin_post_detail {
// Materialize every Java float operation; never contract or reassociate them.
inline float add(float a,float b){volatile float r=a+b;return r;}
inline float sub(float a,float b){volatile float r=a-b;return r;}
inline float mul(float a,float b){volatile float r=a*b;return r;}
inline float divide(float a,float b){volatile float r=a/b;return r;}
inline int32_t signed_bits(uint32_t bits){int32_t r;std::memcpy(&r,&bits,sizeof(r));return r;}
inline int32_t java_int(float f){
    if(std::isnan(f))return 0;
    if(double(f)>=double(INT32_MAX))return INT32_MAX;
    if(double(f)<=double(INT32_MIN))return INT32_MIN;
    return static_cast<int32_t>(f);
}
// M3GMath's min/max intentionally include Java signed-int wraparound.
inline int32_t minimum(int32_t a,int32_t b){const uint32_t d=uint32_t(a)-uint32_t(b);return signed_bits(uint32_t(b)+(d&(0u-(d>>31))));}
inline int32_t maximum(int32_t a,int32_t b){const uint32_t d=uint32_t(a)-uint32_t(b);return signed_bits(uint32_t(a)-(d&(0u-(d>>31))));}
inline bool quantize(const std::vector<float>& values,int type,float inv,const std::array<float,3>& bias,bool positions,std::vector<uint8_t>& bytes){
    bytes.resize(values.size()*size_t(type));
    for(size_t i=0;i<values.size();++i){
        float x=values[i];if(!std::isfinite(x))return false;
        if(positions){x=mul(sub(x,bias[i%3]),inv);if(!std::isfinite(x))return false;}
        const int32_t rounded=java_int(x>0?add(x,0.5f):sub(x,0.5f));
        const uint32_t raw=uint32_t(maximum(type==1?-128:-32768,minimum(type==1?127:32767,rounded)));
        const size_t offset=i*size_t(type);bytes[offset]=uint8_t(raw);
        if(type==2)bytes[offset+1]=uint8_t(raw>>8);
    }
    return true;
}
} // namespace skin_post_detail

// Owner-only postprocessing of owned float output. No VM access, Java allocation
// or borrowed views. All validation and scratch allocation precede publication;
// a decline leaves every field of result unchanged.
inline bool postprocess_skin(const SkinRequest& request,SkinResult& result){
    using namespace skin_post_detail;
    const size_t count=result.positions.size();
    if(!request.postprocess||result.code!=Code::Success||
       (request.position_type!=1&&request.position_type!=2)||(request.normal_type!=0&&request.normal_type!=1&&request.normal_type!=2)||
       !std::isfinite(request.original_scale)||!count||count%3||count/3>65535||
       request.positions.size()!=count||
       (request.normal_type?(request.normals.size()!=count||result.normals.size()!=count):(!request.normals.empty()||!result.normals.empty())))return false;
    const float limit=std::numeric_limits<float>::max();
    float box[6]={limit,-limit,limit,-limit,limit,-limit};
    for(size_t i=0;i<count;++i){
        const size_t j=i%3;const float x=result.positions[i];if(!std::isfinite(x))return false;
        box[2*j]=box[2*j]<x?box[2*j]:x;box[2*j+1]=box[2*j+1]>x?box[2*j+1]:x;
    }
    float half[3];std::array<float,3> bias;
    for(size_t j=0;j<3;++j){
        half[j]=mul(sub(box[2*j+1],box[2*j]),0.5f);
        bias[j]=mul(add(box[2*j],box[2*j+1]),0.5f);
        if(!std::isfinite(half[j])||!std::isfinite(bias[j]))return false;
    }
    const float yz=half[1]>half[2]?half[1]:half[2],extent=half[0]>yz?half[0]:yz;
    float scale=extent>0?divide(extent,request.position_type==1?127.0f:32767.0f):request.original_scale;
    if(scale==0)scale=1;
    const float inv=divide(1,scale);if(!std::isfinite(scale)||!std::isfinite(inv))return false;
    std::vector<uint8_t> position_bytes,normal_bytes;
    if(!quantize(result.positions,request.position_type,inv,bias,true,position_bytes)||
       (request.normal_type&&!quantize(result.normals,request.normal_type,1,bias,false,normal_bytes)))return false;
    result.position_bytes.swap(position_bytes);result.normal_bytes.swap(normal_bytes);
    result.scale=scale;result.bias=bias;result.postprocessed=true;
    return true;
}
} // namespace m3g_gpu
