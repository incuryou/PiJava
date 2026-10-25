#include "../src/decoded_image_cache.h"
#include "../src/gate_recovery.h"
#include <cassert>
#include <random>
#include <iostream>
int main() {
    DecodedImageCache c(128,2);int keys[12]{};std::vector<uint8_t> rgb(30,5),alpha(10,255),out,oa;int w=0,h=0;
    assert(c.put(keys,rgb,alpha,5,2));assert(c.retained_bytes()==30);
    assert(c.get(keys,out,oa,w,h) && w==5 && h==2 && oa.empty() && out==rgb);
    out[0]=8;assert(c.get(keys,out,oa,w,h) && out[0]==5);
    alpha[2]=0;assert(c.put(keys+1,rgb,alpha,5,2));assert(c.entries()==2 && c.retained_bytes()==70);
    assert(c.get(keys,out,oa,w,h));assert(c.put(keys+2,rgb,alpha,5,2));
    assert(!c.get(keys+1,out,oa,w,h));assert(c.get(keys,out,oa,w,h));assert(c.retained_bytes()<=128);
    assert(!c.put(keys+3,std::vector<uint8_t>(300,0),{},10,10));
    assert(!c.put(keys+3,rgb,{},0,0));assert(!c.put(keys+3,rgb,std::vector<uint8_t>(11,5),5,2));
    std::mt19937 rng(0x234);
    for(int i=0;i<20000;i++) {
        int at=rng()%12,width=1+rng()%10;std::vector<uint8_t> r(width*3,uint8_t(at));
        if(rng()%5==0)c.clear();
        if(rng()%2)c.put(keys+at,r,{},width,1);
        else if(c.get(keys+at,out,oa,w,h)){assert(out.size()==size_t(w*h*3));for(auto v:out)assert(v==at);}
        assert(c.retained_bytes()<=128 && c.entries()<=2);
    }
    c.clear();assert(c.retained_bytes()==0 && c.entries()==0 && !c.get(keys,out,oa,w,h));
    const void* method=keys;AutoFpsLearner a;a.started=true;a.verified_gate_seen=true;a.gate_method=method;a.gate_pc=23;a.candidate=50000;a.stable=10;
    GateRecovery r;
    auto remember=[&](){a.reset();a.started=true;a.verified_gate_seen=true;a.gate_method=method;a.gate_pc=23;a.candidate=50000;a.stable=10;assert(r.remember(a,16667,2.999940,1000000));a.reset();};
    remember();assert(!r.restore(a,method,23,50000,16667,1100000)); // No fresh certified open group.
    a.started=true;assert(a.gate_period(50000,100,method,23));
    assert(r.restore(a,method,23,50000,16667,1100000));
    double target=a.drawing(500000,16667,1.0);assert(target>2.99 && target<3.0);
    const double restored=r.consume(target,16667,1100000);assert(restored>2.99 && restored<=2.999940);
    assert(r.consume(target,16667,1100000)==0); // One shot.
    for(int reason=0;reason<5;reason++) {
        remember();a.started=true;a.gate_period(50000,100,method,23);
        assert(!r.restore(a,reason==0?keys+1:method,reason==1?24:23,reason==2?33000:50000,
                        reason==3?33333:16667,reason==4?3000001:1100000));assert(!r.method);
    }
    remember();a.started=true;a.gate_period(50000,100,method,23);assert(!r.restore(a,method,23,50000,16667,999999));
    remember();r.reset();a.started=true;a.gate_period(50000,100,method,23);assert(!r.restore(a,method,23,50000,16667,1100000));
    a.stable=4;assert(!r.remember(a,16667,3.0,100));
    std::cout<<"PASS 2.34 core: 20000 bounded cache operations, identity/copy/LRU/clear, same-gate recovery and 9 rejection guards\n";
}
