#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// A private local error ledger: period - elapsed - previous_error, followed
// by exactly one sleep and error = after - before - requested. Neither the
// request nor the carried error may be read by game logic elsewhere.
struct CompensatedSleepPattern {
    int before_clock=-1,after_clock=-1,anchor_clock=-1,sleep=-1,subtract=-1;
    int now=-1,anchor=-1,elapsed=-1,error=-1,request=-1,period_field=0;
    bool valid() const {return before_clock>=0&&after_clock>sleep&&subtract>after_clock&&anchor_clock>subtract;}
};
template<class Length,class IsClock,class IsSleep,class IsPeriod>
CompensatedSleepPattern recognize_compensated_sleep(const std::vector<uint8_t>& c,int sleep,
        Length length,IsClock is_clock,IsSleep is_sleep,IsPeriod is_period) {
    const int n=int(c.size());
    auto op=[&](int p,int b){return p>=0&&p<n&&c[p]==b;};
    auto cp=[&](int p){return p>=0&&p+1<n?(int(c[p])<<8)|c[p+1]:-1;};
    auto jump=[&](int p){return p>=0&&p+2<n?p+int16_t(cp(p+1)):-1;};
    auto local=[&](int& p,int generic,int compact){
        if(p<0||p>=n)return -1;
        const int b=c[p++];if(b>=compact&&b<compact+4)return b-compact;
        return b==generic&&p<n?int(c[p++]):-1;
    };
    std::vector<bool> boundaries(size_t(n),false);
    for(int at=0;at<n;){const int len=length(c,at);if(len<=0||at+len>n)return {};boundaries[size_t(at)]=true;at+=len;}
    if(!op(sleep,0xb8)||!is_sleep(cp(sleep+1))||!boundaries[size_t(sleep)])return {};
    for(int before=0;before<sleep;) {
        const int next=before+length(c,before);int p=before;
        if(!op(p,0xb8)||!is_clock(cp(p+1))){before=next;continue;}
        CompensatedSleepPattern r;r.before_clock=before;r.sleep=sleep;p+=3;
        if(!op(p++,0x5c)){before=next;continue;}
        r.now=local(p,0x37,0x3f);r.anchor=local(p,0x16,0x1e);
        if(r.now<0||r.anchor<0||!op(p++,0x65)){before=next;continue;}
        r.elapsed=local(p,0x37,0x3f);
        // Static long periods are read, never altered by the pacing protocol.
        if(r.elapsed<0||!op(p,0xb2)||!is_period(cp(p+1))){before=next;continue;}
        r.period_field=cp(p+1);p+=3;
        if(local(p,0x16,0x1e)!=r.elapsed||!op(p++,0x65)){before=next;continue;}
        const int error_read=p;r.error=local(p,0x16,0x1e);
        if(r.error<0||!op(p++,0x65)||!op(p++,0x5c)){before=next;continue;}
        const int request_write=p;r.request=local(p,0x37,0x3f);
        if(r.request<0||!op(p++,0x09)||!op(p++,0x94)||!op(p,0x9e)){before=next;continue;}
        const int guard=p,no_sleep=jump(p);p+=3;const int request_read=p;
        if(local(p,0x16,0x1e)!=r.request||p!=sleep){before=next;continue;}
        p=sleep+3;const int after_jump=op(p,0xa7)?p:-1;
        if(after_jump>=0)p=jump(p);
        r.after_clock=p;
        if(!op(p,0xb8)||!is_clock(cp(p+1))){before=next;continue;}p+=3;
        if(local(p,0x16,0x1e)!=r.now||!op(p++,0x65)){before=next;continue;}
        const int compensation_read=p;
        if(local(p,0x16,0x1e)!=r.request||!op(p,0x65)){before=next;continue;}
        r.subtract=p++;const int error_write=p;
        if(local(p,0x37,0x3f)!=r.error||!op(p,0xa7)){before=next;continue;}
        const int done_jump=p,done=jump(p);p+=3;
        if(p!=no_sleep||!op(p++,0x09)){before=next;continue;}
        const int error_zero=p;
        if(local(p,0x37,0x3f)!=r.error||p!=done||!op(p,0xb8)||!is_clock(cp(p+1))){before=next;continue;}
        r.anchor_clock=p;p+=3;
        if(local(p,0x37,0x3f)!=r.anchor){before=next;continue;}
        const int end=p;
        bool safe=true;
        for(int x:{r.now,r.anchor,r.elapsed,r.error,r.request})
            for(int y:{r.now,r.anchor,r.elapsed,r.error,r.request})
                if(x!=y&&x+1==y)safe=false;
        if(r.now==r.anchor||r.now==r.elapsed||r.now==r.error||r.now==r.request||
           r.anchor==r.elapsed||r.anchor==r.error||r.anchor==r.request||r.elapsed==r.error||
           r.elapsed==r.request||r.error==r.request)safe=false;
        for(int at=0;safe&&at<n;at+=length(c,at)) {
            const int b=c[at];int slot=-1,width=1;bool read=false,write=false;
            if(b>=0x15&&b<=0x19){slot=at+1<n?c[at+1]:-1;read=true;width=b==0x16||b==0x18?2:1;}
            else if(b>=0x36&&b<=0x3a){slot=at+1<n?c[at+1]:-1;write=true;width=b==0x37||b==0x39?2:1;}
            else if(b>=0x1a&&b<=0x2d){slot=(b-0x1a)%4;read=true;width=(b>=0x1e&&b<=0x21)||(b>=0x26&&b<=0x29)?2:1;}
            else if(b>=0x3b&&b<=0x4e){slot=(b-0x3b)%4;write=true;width=(b>=0x3f&&b<=0x42)||(b>=0x47&&b<=0x4a)?2:1;}
            else if(b==0x84){slot=at+1<n?c[at+1]:-1;read=write=true;}
            else if(b==0xc4||b==0xa8||b==0xa9||b==0xc9){safe=false;break;}
            auto overlaps=[&](int target){return slot>=0&&slot<target+2&&slot+width>target;};
            if(overlaps(r.request)&&!((read&&(at==request_read||at==compensation_read))||(write&&at==request_write)))safe=false;
            if(overlaps(r.error)) {
                bool initial=false;
                if(write&&at<before&&slot==r.error&&width==2) {
                    for(int prev=0;prev<at;prev+=length(c,prev))if(prev+length(c,prev)==at)initial=op(prev,0x09);
                }
                if(!((read&&at==error_read)||(write&&(at==error_write||at==error_zero))||initial))safe=false;
            }
            // No unrelated control-flow entry can reach the compensation.
            if((b>=0x99&&b<=0xa7)||b==0xc6||b==0xc7) {
                const int target=jump(at);
                if(target>before&&target<end&&at!=guard&&at!=after_jump&&at!=done_jump)safe=false;
            } else if(b==0xaa||b==0xab||b==0xc8)safe=false;
        }
        for(int at:{r.before_clock,r.after_clock,r.anchor_clock,r.sleep,r.subtract,no_sleep,done})
            if(at<0||at>=n||!boundaries[size_t(at)])safe=false;
        if(safe)return r;
        before=next;
    }
    return {};
}
struct CompensatedSleepState {
    const void* method=nullptr;
    CompensatedSleepPattern pattern;
    int canvas=0,cap=0;
    size_t depth=0;
    uint64_t draws=0;
    int64_t anchor_ms=0,before_ms=0,after_ms=0,requested_ms=0,cut_ms=0,started_us=0;
    bool anchor_seen=false,pending=false,after_seen=false;
    void cancel(){anchor_seen=pending=after_seen=false;cut_ms=0;}
    void reset(){*this={};}
};
