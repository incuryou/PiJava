#version 450
// Shared verbatim by offline UAM and the desktop GPU conformance harness.
layout(std430,binding=0) readonly buffer Vertices { float vertices[]; };
layout(std430,binding=1) readonly buffer Indices { uint indices[]; };
layout(std140,row_major,binding=0) uniform Geometry {
    mat4 modelView; mat4 projection;
    uvec4 config; // ARGB, texture width, texture height, flags
    vec4 depthRange; // near, far, exact Java viewport scale, exact viewport bias
    mat4 textureMatrix;
    mat4 canonicalClip;
};
layout(location=0) out vec2 texcoord;
layout(location=1) noperspective out vec2 affineTexcoord;
layout(location=2) noperspective out float windowDepth;
vec4 transformJava(mat4 m,vec4 p) {
    precise vec4 result;
    for(int r=0;r<4;++r)result[r]=((m[0][r]*p.x+m[1][r]*p.y)+m[2][r]*p.z)+m[3][r]*p.w;
    return result;
}
void main() {
    uint i=indices[gl_VertexID]*5u;
    vec4 position=vec4(vertices[i],vertices[i+1u],vertices[i+2u],1.0);
    precise vec4 clip;
    if((config.w&32u)!=0u)clip=transformJava(canonicalClip,position);
    else {
        precise vec4 eye=transformJava(modelView,position);
        clip=transformJava(projection,eye);
    }
    // Triangle emission divides clip coordinates before applying the viewport
    // matrix. Preserve each binary32 operation and do not contract an FMA.
    precise float invW=1.0/clip.w;
    precise float ndcZ=clip.z*invW;
    precise float scaledDepth=depthRange.z*ndcZ;
    precise float mappedDepth=scaledDepth+depthRange.w;
    windowDepth=mappedDepth;
    // Java projection uses [-w,w]; the shared device uses [0,w].
    clip.z=(clip.z+clip.w)*0.5;
    gl_Position=clip;
    texcoord=(textureMatrix*vec4(vertices[i+3u],vertices[i+4u],0.0,1.0)).xy;
    affineTexcoord=texcoord;
}
