#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
// Admit only a static read and conditional backedge, optionally expressed
// as an exit test plus goto. No write, call, allocation or monitor is skipped.
inline bool pure_static_poll236(const uint8_t* code,size_t size,int pc) {
    if(!code || pc<0)return false;
    for(int start: {pc,pc-3,pc-6}) {
        if(start<0 || size_t(start)>size || size-size_t(start)<6)continue;
        if(code[start]!=178 || code[start+3]<153 || code[start+3]>158)continue;
        const unsigned encoded=(unsigned(code[start+4])<<8)|code[start+5];
        const int delta=encoded<32768?int(encoded):int(encoded)-65536;
        if(delta==-3 && pc<=start+3)return true;
        if(size-size_t(start)>=9 && delta==6 && code[start+6]==167 &&
           code[start+7]==255 && code[start+8]==250)return true;
    }
    return false;
}
