#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include <iostream>
int main() {
    static_assert(sizeof(QuickSite)==32,"pacing cache bytes must occupy existing padding");
    Class c;c.cp.resize(7);
    c.cp[1].tag=10;c.cp[1].a=2;c.cp[1].b=3;
    c.cp[2].tag=7;c.cp[2].a=4;
    c.cp[3].tag=12;c.cp[3].a=5;c.cp[3].b=6;
    for(int i:{4,5,6})c.cp[i].tag=1;
    c.cp[4].utf="java/lang/System";c.cp[5].utf="currentTimeMillis";c.cp[6].utf="()J";
    std::string owner,name,desc;
    assert(pacing_resolve_member(c,1,owner,name,desc)&&owner=="java/lang/System"&&name=="currentTimeMillis"&&desc=="()J");
    for(int value:{-1,0,7,INT_MAX}) {
        auto bad=c;bad.cp[1].a=value;assert(!pacing_resolve_member(bad,1,owner,name,desc));
        bad=c;bad.cp[1].b=value;assert(!pacing_resolve_member(bad,1,owner,name,desc));
        bad=c;bad.cp[2].a=value;assert(!pacing_resolve_member(bad,1,owner,name,desc));
        bad=c;bad.cp[3].a=value;assert(!pacing_resolve_member(bad,1,owner,name,desc));
        bad=c;bad.cp[3].b=value;assert(!pacing_resolve_member(bad,1,owner,name,desc));
    }
    for(int i:{1,2,3,4,5,6}) {
        auto bad=c;bad.cp[i].tag=0;assert(!pacing_resolve_member(bad,1,owner,name,desc));
    }
    Method m;Frame frame;frame.method=&m;
    auto& t=current_context();t.wait_frame.method=&m;t.wait_frame.active=t.wait_frame.complete=true;
    t.residual_sleep.method=&m;t.residual_sleep.active=t.residual_sleep.complete=true;
    release_frame_monitor(frame);
    assert(!t.wait_frame.active&&!t.wait_frame.complete&&!t.residual_sleep.active&&!t.residual_sleep.complete);
    std::cout<<"PASS pacing CP links, cache ABI and invocation-proof expiry\n";
}
