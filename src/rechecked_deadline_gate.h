#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// A closed, side-effect-free remainder-sleep backedge. The ready path stores
// the unmodified clock sample into the same instance-long anchor.
struct RecheckedDeadlinePattern {
    int clock_pc=-1,branch_pc=-1,sleep_pc=-1,anchor_field=0,now_local=-1,remainder_local=-1;
    int64_t period_ms=0;
    bool valid() const{return clock_pc>=0&&branch_pc>clock_pc&&sleep_pc>branch_pc;}
};
template<class Length,class IsClock,class IsSleep,class IsMax,class IsLongField,class Constant>
RecheckedDeadlinePattern recognize_rechecked_deadline(const std::vector<uint8_t>& code,int clock_pc,
        Length length,IsClock is_clock,IsSleep is_sleep,IsMax is_max,IsLongField is_long_field,Constant constant) {
    const int n=int(code.size());
    auto op=[&](int p,int v){return p>=0&&p<n&&code[p]==v;};
    auto cp=[&](int p){return p>=0&&p+1<n?(int(code[p])<<8)|code[p+1]:-1;};
    auto jump=[&](int p){return p>=0&&p+2<n?p+int16_t(cp(p+1)):-1;};
    auto local=[&](int& p,int generic,int compact){
        if(p<0||p>=n)return -1;
        const int v=code[p++];
        if(v>=compact&&v<compact+4)return v-compact;
        if(v==generic&&p<n)return int(code[p++]);
        return -1;
    };
    // A target inside an operand cannot manufacture a gate certificate.
    std::vector<bool> boundary(size_t(n),false);
    for(int at=0;at<n;){const int size=length(code,at);if(size<=0||at+size>n)return {};boundary[size_t(at)]=true;at+=size;}
    if(clock_pc<0||clock_pc>=n||!boundary[size_t(clock_pc)]||!op(clock_pc,0xb8)||!is_clock(cp(clock_pc+1)))return {};
    int p=clock_pc+3;const int now=local(p,0x37,0x3f);if(now<0)return {};
    int64_t period=0;
    if(op(p,0x14)){period=constant(cp(p+1));p+=3;}
    else if(op(p,0x10)||op(p,0x11)) {
        const bool wide=op(p,0x11);period=wide?int16_t(cp(p+1)):(p+1<n?int8_t(code[p+1]):-1);p+=wide?3:2;
        if(!op(p++,0x85))return {};
    } else return {};
    if(period<8||period>120||local(p,0x16,0x1e)!=now||!op(p++,0x2a)||!op(p,0xb4))return {};
    const int anchor=cp(p+1);p+=3;
    if(!is_long_field(anchor)||!op(p++,0x65)||!op(p++,0x65)||!op(p++,0x88))return {};
    const int remainder=local(p,0x36,0x3b);if(remainder<0)return {};
    const int guard_start=p;
    // Optional one-shot clamp: if(flag!=0){remainder=max(1,remainder);flag=0;}
    int test=p;const int flag=local(test,0x15,0x1a);
    if(flag>=0&&op(test,0x99)) {
        if(flag==remainder||flag==now||flag==now+1)return {};
        const int exit=jump(test);p=test+3;
        if(!op(p++,0x04)||local(p,0x15,0x1a)!=remainder||!op(p,0xb8)||!is_max(cp(p+1)))return {};
        p+=3;if(local(p,0x36,0x3b)!=remainder||!op(p++,0x03)||local(p,0x36,0x3b)!=flag||p!=exit)return {};
    } else p=guard_start;
    if(local(p,0x15,0x1a)!=remainder)return {};
    const int branch=p;if(!op(branch,0x9d)&&!op(branch,0x9e))return {};
    int wait=op(branch,0x9d)?jump(branch):branch+3;
    const int ready=op(branch,0x9d)?branch+3:jump(branch);
    if(wait<0||wait>=n||ready<0||ready>=n||!boundary[size_t(wait)]||!boundary[size_t(ready)])return {};
    int r=ready;
    if(!op(r++,0x2a)||local(r,0x16,0x1e)!=now||!op(r,0xb5)||cp(r+1)!=anchor)return {};
    p=wait;if(local(p,0x15,0x1a)!=remainder||!op(p++,0x85)||!op(p,0xb8)||!is_sleep(cp(p+1)))return {};
    const int sleep=p;p+=3;
    // Follow only exact normal-path unconditional backedges. A side-by-side
    // exception handler cannot grant permission to skip its Java side effects.
    bool closed=false;for(int i=0;i<3;++i){if(p==clock_pc){closed=true;break;}if(!op(p,0xa7))return {};p=jump(p);}
    if(!closed)return {};
    return {clock_pc,branch,sleep,anchor,now,remainder,period};
}
struct RecheckedDeadlineState {
    const void* method=nullptr;
    RecheckedDeadlinePattern pattern;
    int canvas=0;
    size_t depth=0;
    uint64_t ready_draws=0,cycle_draws=0;
    int64_t ready_now=0,sample_now=0,anchor=0,due_us=0,started_us=0;
    bool have_ready=false,active=false,sampled=false;
    void cancel(){active=false;have_ready=false;sampled=false;due_us=0;}
    void reset(){*this={};}
};
