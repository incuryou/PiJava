#version 450
layout(std430,binding=0) readonly buffer InputPixels { uint words[]; };
layout(std430,binding=1) readonly buffer ScanlinesGains { uint gains[]; };
layout(location=0) out vec4 color;
#define GPU_INLINE
#define GPU_PIXEL_SOURCE
#define GPU_PIXEL_READ(x,y) words[4u+(y)*w+(x)]
#include "gpu_filter_math.inc"
void main(){
    GpuFilterViewport rect=gpu_filter_viewport(words[0],words[1],words[3]);
    uint x=uint(gl_FragCoord.x)-rect.x,y=uint(gl_FragCoord.y)-rect.y;
    uint preset=words[2];
    uint gain=preset==5u?gains[(y%64u)*48u+x%48u]:0u;
    uint c=words[3]==0u?gpu_filter_pixel(x,y,words[0],words[1],preset,gain):
        gpu_filter_pixel_extent(x,y,words[0],words[1],preset,gain,rect.w,rect.h);
    uint r=c&255u,g=(c>>8u)&255u,b=(c>>16u)&255u;
    color=vec4(vec3(r,g,b)/255.0,1.0);
}
