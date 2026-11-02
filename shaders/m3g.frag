#version 450
layout(std430,binding=2) readonly buffer TexturePixels { uint texels[]; };
layout(std140,row_major,binding=0) uniform Geometry {
    mat4 modelView; mat4 projection; uvec4 config; vec4 depthRange; mat4 textureMatrix; mat4 canonicalClip;
};
layout(location=0) in vec2 texcoord;
layout(location=1) noperspective in vec2 affineTexcoord;
layout(location=2) noperspective in float windowDepth;
layout(location=0) out vec4 color;
uint coordinate(float t,uint size,bool repeat) {
    float f=floor(t);
    return uint(repeat?mod(f,float(size)):clamp(f,0.0,float(size-1u)));
}
void main() {
    uint argb=config.x;
    if((config.w&1u)!=0u) {
        vec2 uv=(config.w&16u)!=0u?texcoord:affineTexcoord;
        uint x=coordinate(uv.x,config.y,(config.w&2u)!=0u);
        uint y=coordinate(uv.y,config.z,(config.w&4u)!=0u);
        uint sampleColor=texels[y*config.y+x];
        if((config.w&8u)!=0u) {
            uvec3 a=uvec3(argb>>16u,argb>>8u,argb)&255u;
            uvec3 b=uvec3(sampleColor>>16u,sampleColor>>8u,sampleColor)&255u;
            uvec3 c=(a*b)>>8u; // bundled RGB MODULATE integer semantics
            argb=0xff000000u|(c.r<<16u)|(c.g<<8u)|c.b;
        } else argb=sampleColor|0xff000000u;
    }
    color=vec4(vec3((argb>>16u)&255u,(argb>>8u)&255u,argb&255u)/255.0,1.0);
    // Explicit signed-short encoding, including negative initial depths.
    // Never reinterpret a Java short as an IEEE float or a ZF32 image.
    // A power-of-two scale is exactly representable for every Java short;
    // CPU division and GPU reciprocal folding cannot disagree at LEQUAL ties.
    gl_FragDepth=(float(int(windowDepth))+32768.0)/65536.0;
}
