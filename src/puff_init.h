#pragma once
#include <mutex>
extern "C" int puff(unsigned char*,unsigned long*,const unsigned char*,unsigned long*);
namespace j2me_image {
// puff's fixed Huffman tables have one mutable first-use initialization. Every
// caller, including metadata/ZIP/M3G and worker PNG, shares this publication fence.
inline bool initialize_puff(){
    static std::once_flag once;static bool ok=false;
    std::call_once(once,[]{const unsigned char empty_fixed[]={3,0};unsigned char out[1];unsigned long n=1,m=2;ok=puff(out,&n,empty_fixed,&m)==0;});
    return ok;
}
inline int inflate_raw(unsigned char* out,unsigned long* out_size,const unsigned char* input,unsigned long* input_size){
    if(!initialize_puff())return -1;
    return puff(out,out_size,input,input_size);
}
}
