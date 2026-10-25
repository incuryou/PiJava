#version 450
layout(std430,binding=0) readonly buffer InputPixels { uint words[]; };
layout(std430,binding=1) readonly buffer ScanlinesGains { uint gains[]; };
layout(location=0) out vec4 color;
#define GPU_INLINE
#define GPU_PIXEL_SOURCE
#define GPU_PIXEL_READ(x,y) words[4u+(y)*w+(x)]
#include "gpu_filter_math.inc"
void main(){
    uint x=uint(gl_FragCoord.x)-400u,y=uint(gl_FragCoord.y)-40u;
    uint preset=words[2];
    uint gain=preset==5u?gains[(y%64u)*48u+x%48u]:0u;
    uint c=gpu_filter_pixel(x,y,words[0],words[1],preset,gain);
    uint r=c&255u,g=(c>>8u)&255u,b=(c>>16u)&255u;
    color=vec4(vec3(r,g,b)/255.0,1.0);
}
