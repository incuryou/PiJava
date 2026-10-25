#define __SWITCH__ 1
#include "../src/cpu_clock.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
static int mode=0,calls=0;
static u64 ticks=19200000;
Result svcGetInfo(u64* out,u32 type,Handle handle,u64 subtype){
    ++calls;assert(handle==CUR_THREAD_HANDLE && subtype==TickCountInfo_Total);
    if(mode==2 || (mode==1&&type==InfoType_ThreadTickCount))return 1;
    *out=ticks;return 0;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;mode=std::atoi(argv[1]);const int64_t a=j2me_cpu_clock::now_ns();
    if(mode==2){assert(a==-1 && calls==2);assert(j2me_cpu_clock::now_ns()==-1 && calls==2);}
    else {
        assert(a==1000000000 && calls==(mode==1?2:1));
        ticks+=19200;assert(j2me_cpu_clock::now_ns()==1001000000 && calls==(mode==1?3:2));
        ticks=UINT64_MAX;assert(j2me_cpu_clock::now_ns()==-1);
    }
    std::cout<<"PASS CPU Switch mock mode="<<mode<<" modern/legacy cache, nanoseconds and failure/overflow\n";
}
