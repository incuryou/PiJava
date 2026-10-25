#pragma once
#include <cstdint>
#include <vector>

// A field-held frame period minus two clock samples. Only the exact conditional
// remainder-sleep expression qualifies; a fixed short sleep never does.
struct ResidualSleepPattern {
    int start_pc=-1,end_pc=-1,sleep_pc=-1;
    int start_field=0,end_field=0,period_field=0;
    bool valid() const {return start_pc>=0&&end_pc>start_pc&&sleep_pc>end_pc;}
};
template<class Length,class IsClock,class IsSleep,class FieldKind>
ResidualSleepPattern recognize_residual_sleep(const std::vector<uint8_t>& code,int clock_pc,
        Length length,IsClock is_clock,IsSleep is_sleep,FieldKind field_kind) {
    const int n=int(code.size());
    auto cp=[&](int p){return p>=0&&p+1<n?(int(code[p])<<8)|code[p+1]:0;};
    auto load=[&](int& p,int expected){
        if(p<0||p+4>n||code[p]!=0x2a||code[p+1]!=0xb4||cp(p+2)!=expected)return false;
        p+=4;return true;
    };
    if(clock_pc<1||clock_pc+6>n||code[clock_pc-1]!=0x2a||code[clock_pc]!=0xb8||
       !is_clock(cp(clock_pc+1))||code[clock_pc+3]!=0xb5)return {};
    const int start=cp(clock_pc+4);
    if(field_kind(start)!=1)return {};
    // Decode actual instruction boundaries, so constants containing opcode-like
    // bytes cannot manufacture a certificate.
    for(int q=clock_pc+6;q<n;) {
        const int size=length(code,q);if(size<=0||q+size>n)return {};
        if(code[q]==0xb8&&q>=15&&is_sleep(cp(q+1))) {
            for(int conversion:{0,1}) {
                const int body=q-14-conversion,condition=body-17-conversion;
                if(condition<clock_pc+6||body<0||body+4>n||code[body]!=0x2a||code[body+1]!=0xb4)continue;
                const int period=cp(body+2),kind=field_kind(period);
                if((conversion&&kind!=2)||(!conversion&&kind!=1))continue;
                int p=body;if(!load(p,period))continue;
                if(conversion&&code[p++]!=0x85)continue;
                if(p+8>n||code[p]!=0x2a||code[p+1]!=0xb4)continue;
                const int end=cp(p+2);if(field_kind(end)!=1||end==start||!load(p,end)||!load(p,start))continue;
                if(p+2!=q||code[p++]!=0x65||code[p++]!=0x65)continue;
                p=condition;
                if(!load(p,end)||!load(p,start)||code[p++]!=0x65||!load(p,period))continue;
                if(conversion&&code[p++]!=0x85)continue;
                if(code[p++]!=0x94||code[p]!=0x9c)continue;
                const int target=p+int16_t(cp(p+1));
                if(p+3!=body||target<q+3||target>q+12)continue;
                // Find the clock that writes end in this straight-line section.
                int ending=-1;
                for(int e=clock_pc+6;e<condition;) {
                    const int es=length(code,e);if(es<=0||e+es>n)break;
                    if(e>0&&e+6<=condition&&code[e-1]==0x2a&&code[e]==0xb8&&
                       is_clock(cp(e+1))&&code[e+3]==0xb5&&cp(e+4)==end)ending=e;
                    e+=es;
                }
                if(ending>=0)return {clock_pc,ending,q,start,end,period};
            }
        }
        q+=size;
    }
    return {};
}
struct ResidualSleepState {
    const void* method=nullptr;
    ResidualSleepPattern pattern;
    uint64_t draws=0;
    int64_t start_ms=0,end_ms=0,start_us=0;
    size_t depth=0;
    bool active=false,complete=false;
    void cancel(){active=complete=false;}
    void reset(){*this={};}
};
