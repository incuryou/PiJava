#include "../src/poll_gate.h"
#include <cassert>
#include <iostream>
static TimeGatePattern recognize(const std::vector<uint8_t>& code,int clock,int64_t period) {
    auto length=[&](int p){const int op=code[p];return op==0xb8||op==0xb2||op==0xb3||op==0x14||op==0xa7||(op>=0x99&&op<=0xa6)?3:1;};
    return recognize_field_time_gate(code,clock,clock+3,[&](int cp){return cp==1?period:0;},
        [](int cp){return cp==2;},[](int cp){return cp==3;},[](int cp){return cp>=4&&cp<=6;},length);
}
int main() {
    const std::vector<uint8_t> direct={0xb8,0,3,0x5c,0xb3,0,4,0xb3,0,5,0xb8,0,2,0xb8,0,3,0xb2,0,4,0x65,0x14,0,1,0x94,0x9b,0xff,0xf2,0xb1};
    auto g=recognize(direct,13,52);assert(g.static_field==4&&g.anchor_clock_pc==0&&g.site_pc==13&&g.millis==52);
    const std::vector<uint8_t> cached={0xb8,0,3,0xb3,0,4,0xb8,0,3,0xb2,0,4,0x65,0xb3,0,6,0xb2,0,6,0x09,0x94,0x9e,0,29,0xb2,0,6,0x14,0,1,0x94,0x9c,0,19,0xb8,0,2,0xb8,0,3,0xb2,0,4,0x65,0xb3,0,6,0xa7,0xff,0xe1,0xb1};
    for(int pc:{6,37}){g=recognize(cached,pc,66);assert(g.static_field==4&&g.anchor_clock_pc==0&&g.site_pc==37&&g.millis==66);}
    for(const auto& source:{direct,cached})for(size_t size=0;size<source.size();++size) {
        std::vector<uint8_t> short_code(source.begin(),source.begin()+size);
        assert(recognize(short_code,source.size()==direct.size()?13:37,66).static_field<0);
    }
    auto bad=direct;bad[10]=0xb3;assert(recognize(bad,13,52).static_field<0); // wait path writes
    bad=direct;bad[12]=3;assert(recognize(bad,13,52).static_field<0); // wait path calls a clock instead of yield
    bad=direct;bad[16]=0xb4;assert(recognize(bad,13,52).static_field<0); // instance fields excluded
    bad=direct;bad[4]=0;assert(recognize(bad,13,52).static_field<0); // no same-method anchor clock store
    bad=cached;bad[21]=0x9b;assert(recognize(bad,37,66).static_field<0); // alternate cached guard excluded
    bad=cached;bad[46]=4;assert(recognize(bad,37,66).static_field<0); // wait path overwrites anchor
    assert(recognize(direct,13,7).static_field<0&&recognize(direct,13,121).static_field<0);
    bad=direct;bad[23]=0x9e;
    assert(recognize(bad,13,INT64_MAX).static_field<0);
    assert(recognize(bad,13,INT64_MIN).static_field<0);
    assert(recognize(bad,13,120).static_field<0);
    FieldGateAnchor anchor;anchor.record(&direct,0,4,1000,2000,4);g=recognize(direct,13,52);
    assert(anchor.owns(&direct,g,1000,3000,5));
    assert(!anchor.owns(&direct,g,1000,3000,4));
    assert(!anchor.owns(&cached,g,1000,3000,5));
    for(int64_t value:{999,1001,1500})assert(!anchor.owns(&direct,g,value,3000,5));
    assert(!anchor.owns(&direct,g,1000,122001,5));
    PollGateState poll;
    for(int i=0;i<4;++i)assert(!poll.observe(&direct,13,1000,52,1001+i,true));
    assert(poll.observe(&direct,13,1000,52,1052,true)&&poll.fresh_ready);
    assert(poll.observe(&direct,13,1100,52,1152,true)&&!poll.fresh_ready); // cached proof is not fresh waiting
    poll.reset();assert(!poll.observe(&direct,13,1200,52,1252,true)&&!poll.fresh_ready);
    std::cout<<"PASS narrow static/cached gates, truncation, wait effects and anchor ownership\n";
}
