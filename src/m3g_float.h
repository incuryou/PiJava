#pragma once
// Java float bytecodes round each operation. Preserve those boundaries on
// AArch64 and other FMA targets without changing whole-program compiler flags.
namespace m3g_float {
inline float add(float a,float b){volatile float value=a+b;return value;}
inline float mul(float a,float b){volatile float value=a*b;return value;}
inline float dot4(float a,float x,float b,float y,float c,float z,float d,float w){
    return add(add(add(mul(a,x),mul(b,y)),mul(c,z)),mul(d,w));
}
}
