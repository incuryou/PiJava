#include "../src/rechecked_deadline_gate.h"
#include <cassert>
#include <iostream>
static int length(const std::vector<uint8_t>& c,int p) {
    if(p<0||p>=int(c.size()))return -1;
    switch(c[p]) {
    case 0xb8:case 0xb4:case 0xb5:case 0x99:case 0x9d:case 0x9e:case 0xa7:case 0x14:return 3;
    case 0x10:case 0x37:case 0x36:case 0x16:case 0x15:return 2;
    default:return 1;
    }
}
static RecheckedDeadlinePattern recognize(const std::vector<uint8_t>& c,int at=0) {
    return recognize_rechecked_deadline(c,at,length,[](int cp){return cp==1;},[](int cp){return cp==2;},
       [](int cp){return cp==3;},[](int cp){return cp==4;},[](int cp)->int64_t{return cp==5?40:0;});
}
int main() {
    // Activity/obfuscator shape, including the one-shot max(1,remaining) clamp.
    std::vector<uint8_t> c={0xb8,0,1,0x37,10,0x10,40,0x85,0x16,10,0x2a,0xb4,0,4,0x65,0x65,0x88,0x36,4,
        0x15,1,0x99,0,13,0x04,0x15,4,0xb8,0,3,0x36,4,0x03,0x3c,
        0x15,4,0x9d,0,10,0x2a,0x16,10,0xb5,0,4,0xb1,
        0x15,4,0x85,0xb8,0,2,0xa7,0xff,0xcc};
    const auto p=recognize(c);assert(p.valid()&&p.period_ms==40&&p.branch_pc==36&&p.sleep_pc==49);
    for(size_t end=0;end<c.size();++end)assert(!recognize({c.begin(),c.begin()+end}).valid());
    auto bad=c;bad[40]=0x16;bad[41]=9;assert(!recognize(bad).valid());
    bad=c;bad[44]=6;assert(!recognize(bad).valid());
    bad=c;bad[51]=3;assert(!recognize(bad).valid());
    bad=c;bad[54]=0xcd;assert(!recognize(bad).valid());
    bad=c;bad[6]=7;assert(!recognize(bad).valid());
    bad=c;bad[6]=121;assert(!recognize(bad).valid());
    bad=c;bad[50]=0x84;assert(!recognize(bad).valid());
    assert(!recognize(c,1).valid());
    std::cout<<"PASS exact rechecked deadline shape, truncation, boundaries, foreign locals/fields, period and impure path\n";
}
