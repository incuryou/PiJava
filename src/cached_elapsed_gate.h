#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// A cached now-anchor field with a positive bounded guard and a constant
// short sleep. Paint-completion flag waits are outside this certificate.
struct CachedElapsedPattern {
    int anchor_clock=-1,clock=-1,repeat_clock=-1,branch=-1,sleep=-1;
    int anchor_field=-1,elapsed_field=-1;
    int64_t period_ms=0,sleep_ms=0;
    bool valid() const {return anchor_clock>=0&&branch>=0&&sleep>=0;}
};
template<class Length,class IsClock,class IsSleep,class Constant,class IsLongStatic>
CachedElapsedPattern recognize_cached_elapsed(const std::vector<uint8_t>& code,int clock,
        Length length,IsClock is_clock,IsSleep is_sleep,Constant constant,IsLongStatic is_static) {
    const int n=int(code.size());
    auto op=[&](int p,int b){return p>=0&&p<n&&code[p]==b;};
    auto cp=[&](int p){return p>=0&&p+2<n?(int(code[p+1])<<8)|code[p+2]:-1;};
    auto jump=[&](int p){return p>=0&&p+2<n?p+int16_t(cp(p)):-1;};
    if(!op(clock,184)||!is_clock(cp(clock))||!op(clock+3,178)||!is_static(cp(clock+3))||
       !op(clock+6,101)||!op(clock+7,179)||!is_static(cp(clock+7)))return {};
    const int anchor=cp(clock+3),elapsed=cp(clock+7);
    if(anchor==elapsed)return {};
    int guard=clock+10;if(op(guard,167))guard=jump(guard);
    if(!op(guard,178)||cp(guard)!=elapsed||!op(guard+3,9)||!op(guard+4,148)||!op(guard+5,158))return {};
    const int exit=jump(guard+5),bound=guard+8;
    if(!op(bound,178)||cp(bound)!=elapsed||!op(bound+3,20)||!op(bound+6,148)||
       !op(bound+7,156)||jump(bound+7)!=exit)return {};
    const int64_t period=constant(cp(bound+3));
    if(period<8||period>120)return {};
    int p=bound+10;int64_t delay=0;
    if(op(p,20)){delay=constant(cp(p));p+=3;}
    else if(op(p,10)){delay=1;++p;}
    else return {};
    const int sleep=p;
    if(delay<1||delay>20||!op(p,184)||!is_sleep(cp(p)))return {};
    p+=3;
    // Normal flow may jump over one catch-local store/pop. A real exception
    // cancels runtime evidence before that handler can run.
    if(op(p,167)) {
        const int target=jump(p),handler=p+3;int after=handler;
        if(op(after,87)||(after<n&&code[after]>=75&&code[after]<=78))++after;
        else if(op(after,58)&&after+1<n)after+=2;
        else return {};
        if(target!=after)return {};
        p=target;
    }
    const int repeat=p;
    if(!op(p,184)||!is_clock(cp(p))||!op(p+3,178)||cp(p+3)!=anchor||!op(p+6,101)||
       !op(p+7,179)||cp(p+7)!=elapsed||!op(p+10,167)||jump(p+10)!=guard)return {};
    if(clock!=repeat&&clock+10!=guard)return {};
    if(exit<0||exit>=n||(exit>=guard&&exit<p+13))return {};
    std::vector<bool> boundaries(std::size_t(n),false);
    int anchor_clock=-1,writes=0;
    for(int at=0;at<n;) {
        boundaries[std::size_t(at)]=true;const int len=length(code,at);
        if(len<=0||at+len>n)return {};
        if(op(at,179)&&cp(at)==anchor)++writes;
        if(at<clock&&op(at,184)&&is_clock(cp(at))&&op(at+3,179)&&cp(at+3)==anchor)anchor_clock=at;
        at+=len;
    }
    if(anchor_clock<0||writes!=1)return {};
    for(int at:{guard,exit,repeat,sleep,bound+7,anchor_clock})if(!boundaries[std::size_t(at)])return {};
    return {anchor_clock,clock,repeat,bound+7,sleep,anchor,elapsed,period,delay};
}
struct CachedElapsedState {
    const void* method=nullptr;
    CachedElapsedPattern pattern;
    int canvas=0,cap=0;
    std::size_t depth=0;
    uint64_t draws=0;
    int64_t anchor_ms=0,anchor_us=0,latest_ms=0,due_us=0,rejected_anchor_us=0;
    bool active=false;
    void reset(){*this={};}
    void cancel(){active=false;rejected_anchor_us=anchor_us;}
};
