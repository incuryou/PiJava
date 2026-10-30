#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// A one-shot event adds exactly one ordinary period to a drawn frame:
// start=clock(); if(work()) exit; extra=0; if(flag){extra=field;flag=false;}
// sleep_positive(max(floor, (extra+P)-(clock()-start))).
// This certificate is separate from the general 8..120ms sleep classifier.
struct EventFrameSleepPattern {
    int start=-1,end=-1,bridge=-1,loop_end=-1;
    int anchor=-1,extra=-1,request=-1,flag_field=-1,extra_field=-1,bridge_cp=-1;
    int period_ms=0,floor_ms=0;
    bool valid() const {return start>=0&&end>start&&bridge>end&&loop_end>bridge;}
};
template<class Length,class IsClock,class IsWork,class IsField,class IsMax,class IsBridge,class Constant>
EventFrameSleepPattern recognize_event_frame_sleep(const std::vector<uint8_t>& c,int start,
        Length length,IsClock is_clock,IsWork is_work,IsField is_field,IsMax is_max,IsBridge is_bridge,Constant constant) {
    const int n=int(c.size());
    auto op=[&](int p,int b){return p>=0&&p<n&&c[p]==b;};
    auto cp=[&](int p){return p>=0&&p+1<n?(int(c[p])<<8)|c[p+1]:-1;};
    auto jump=[&](int p){return p>=0&&p+2<n?p+int16_t(cp(p+1)):-1;};
    auto local=[&](int& p,int generic,int compact) {
        if(p<0||p>=n)return -1;const int b=c[p++];
        if(b>=compact&&b<compact+4)return b-compact;
        return b==generic&&p<n?int(c[p++]):-1;
    };
    std::vector<bool> boundaries(size_t(n),false);
    for(int at=0;at<n;) {
        const int len=length(c,at);if(len<=0||len>n-at)return {};
        boundaries[size_t(at)]=true;at+=len;
    }
    if(start<0||start>=n||!boundaries[size_t(start)])return {};
    EventFrameSleepPattern r;r.start=start;int p=start;
    if(!op(p,0xb8)||!is_clock(cp(p+1)))return {};p+=3;
    r.anchor=local(p,0x37,0x3f);
    if(r.anchor<=0||local(p,0x19,0x2a)!=0||!op(p,0xb6)||!is_work(cp(p+1)))return {};p+=3;
    if(!op(p,0x9a))return {};const int exit_branch=p,exit=jump(p);p+=3;
    if(!op(p++,0x03))return {};r.extra=local(p,0x36,0x3b);
    if(r.extra<=0||local(p,0x19,0x2a)!=0||!op(p,0xb4)||!is_field(cp(p+1),false))return {};
    r.flag_field=cp(p+1);p+=3;
    if(!op(p,0x99))return {};const int event_branch=p,selected=jump(p);p+=3;
    if(local(p,0x19,0x2a)!=0||!op(p,0xb4)||!is_field(cp(p+1),true))return {};
    r.extra_field=cp(p+1);p+=3;
    if(local(p,0x36,0x3b)!=r.extra||local(p,0x19,0x2a)!=0||!op(p++,0x03)||
       !op(p,0xb5)||cp(p+1)!=r.flag_field)return {};p+=3;
    if(p!=selected)return {};
    int64_t floor=0;
    if(op(p,0x0a)){floor=1;++p;}
    else if(op(p,0x14)){floor=constant(cp(p+1),true);p+=3;}
    else return {};
    if(floor<1||floor>7)return {};r.floor_ms=int(floor);
    if(local(p,0x15,0x1a)!=r.extra)return {};
    int64_t period=0;
    if(op(p,0x10)&&p+1<n){period=int8_t(c[p+1]);p+=2;}
    else if(op(p,0x11)&&p+2<n){period=int16_t(cp(p+1));p+=3;}
    else if(op(p,0x12)&&p+1<n){period=constant(c[p+1],false);p+=2;}
    else if(op(p,0x13)){period=constant(cp(p+1),false);p+=3;}
    else return {};
    if(period<8||period>120||!op(p++,0x60)||!op(p++,0x85))return {};r.period_ms=int(period);
    r.end=p;if(!op(p,0xb8)||!is_clock(cp(p+1)))return {};p+=3;
    if(local(p,0x16,0x1e)!=r.anchor||!op(p++,0x65)||!op(p++,0x65)||!op(p,0xb8)||!is_max(cp(p+1)))return {};p+=3;
    if(!op(p++,0x5c))return {};r.request=local(p,0x37,0x3f);
    r.bridge=p;r.bridge_cp=cp(p+1);
    if(r.request<=0||!op(p,0xb8)||!is_bridge(r.bridge_cp))return {};p+=3;
    if(!op(p,0xa7)||jump(p)!=start)return {};const int backedge=p;p+=3;r.loop_end=p;
    if(exit!=p||exit>=n||!boundaries[size_t(exit)]||
       (r.extra>=r.anchor&&r.extra<r.anchor+2)||(r.extra>=r.request&&r.extra<r.request+2))return {};
    for(int at=0;at<n;at+=length(c,at)) {
        const int b=c[at];
        if((b>=0x99&&b<=0xa7)||b==0xc6||b==0xc7) {
            const int target=jump(at);
            if(target<0||target>=n||!boundaries[size_t(target)])return {};
            if(target>start&&target<r.loop_end&&at!=exit_branch&&at!=event_branch&&at!=backedge)return {};
        } else if(b==0xaa||b==0xab||b==0xa8||b==0xa9||b==0xc4||b==0xc8||b==0xc9)return {};
    }
    return r;
}

struct EventFrameBridgePattern {
    int sleep=-1,positive=-1,after_sleep=-1,handler=-1;
    bool valid() const {return sleep>=0;}
};
template<class IsSleep,class IsTrace>
EventFrameBridgePattern recognize_event_frame_bridge(const std::vector<uint8_t>& c,IsSleep is_sleep,IsTrace is_trace) {
    // Deliberately closed normal path. The optional Java catch/print path is
    // retained and can only be entered after Thread.sleep throws.
    auto op=[&](int p,int b){return p>=0&&p<int(c.size())&&c[p]==b;};
    auto cp=[&](int p){return p>=0&&p+1<int(c.size())?(int(c[p])<<8)|c[p+1]:-1;};
    if(c.size()!=12&&c.size()!=18)return {};
    if(!op(0,0x1e)||!op(1,0x09)||!op(2,0x94)||!op(3,0x9d)||cp(4)!=4||!op(6,0xb1)||
       !op(7,0x1e)||!op(8,0xb8)||!is_sleep(cp(9))||!op(11,0xb1))return {};
    EventFrameBridgePattern r;r.positive=7;r.sleep=8;r.after_sleep=11;
    if(c.size()==18) {
        if(!op(12,0x59)||!op(13,0x4b)||!op(14,0xb6)||!is_trace(cp(15))||!op(17,0xb1))return {};
        r.handler=12;
    }
    return r;
}

struct EventFrameSleepState {
    const void* method=nullptr;
    EventFrameSleepPattern pattern;
    int controller=0,canvas=0,cap=0;
    uint32_t epoch=0,controller_generation=0,canvas_generation=0;
    uint64_t cycle=0,draws=0;
    size_t depth=0;
    int64_t start_ms=0,end_ms=0,start_us=0,request_ms=0;
    bool active=false,complete=false;
    void cancel(){active=complete=false;}
    void reset(){*this={};}
};
