#include "../src/time_gate_patterns.h"
#include <cassert>
#include <iostream>
static TimeGatePattern recognize(const std::vector<uint8_t>& code,int handler=-1) {
    return recognize_aliased_field_time_gate(code,0,3,[](int cp)->int64_t{return cp==1?37:0;},
        [](int cp){return cp==2;},[](int cp){return cp==3;},[](int cp){return cp>=4&&cp<=6;},
        [](int a,int b){return a==b||((a==4||a==6)&&(b==4||b==6));},
        [&](int p){const int op=code[p];return op==0xb8||op==0xb2||op==0xb3||op==0x14||op==0xa7||(op>=0x99&&op<=0xa6)?3:1;},
        [&](int pc){return pc==handler;});
}
int main() {
    const std::vector<uint8_t> code={0xb8,0,3,0x5c,0x3f,0xb2,0,4,0x65,0x5c,0x41,0x14,0,1,0x94,0x9c,0,6,0xa7,0xff,0xee,
        0x1e,0xb3,0,4,0xb8,0,3,0x5c,0x3f,0xb2,0,4,0x65,0xb3,0,5,0x1e,0xb3,0,4,0xb1};
    auto gate=recognize(code);assert(gate.static_field==4&&gate.anchor_clock_pc==25&&gate.site_pc==0&&gate.millis==37);
    for(size_t n=0;n<41;++n)assert(recognize(std::vector<uint8_t>(code.begin(),code.begin()+n)).static_field<0);
    auto bad=code;bad[10]=0x3f;assert(recognize(bad).static_field<0); // elapsed overwrites the timestamp alias
    bad=code;bad.insert(bad.begin()+38,{0x0a,0x61});assert(recognize(bad).static_field<0); // business-adjusted anchor
    bad=code;bad.insert(bad.begin()+41,{0x09,0xb3,0,4});assert(recognize(bad).static_field<0); // extra definition
    bad=code;bad.insert(bad.begin()+41,{0xa7,0xff,0xf4});assert(recognize(bad).static_field<0); // branch into alias definition
    assert(recognize(code,29).static_field<0); // exception entry bypasses clock definition
    bad=code;bad[40]=6;assert(recognize(bad).static_field==4); // duplicate CP reference to the same field
    bad=code;bad[20]=0xef;assert(recognize(bad).static_field<0); // wait backedge targets a non-boundary
    bad=code;bad[8]=0x61;assert(recognize(bad).static_field<0); // addition is not a time difference
    std::cout<<"PASS aliased field provenance, definitions and control-flow entries\n";
}
