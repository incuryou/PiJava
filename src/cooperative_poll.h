#pragma once
#include <cstdint>
#include <vector>

// A CPU-idle certificate only. It neither advances a game clock nor changes
// branches, Java locals, waits, frame grouping, or the original loop's body.
struct CooperativePollPattern {
    int kind=0,begin=-1,end=-1,ready=-1,clock=-1;
    int receiver=-1,anchor_local=-1,anchor_cp=-1,period_cp=-1;
    int anchor_owner=-1,anchor_field=-1,period_owner=-1,period_field=-1;
    int64_t period_ms=0;
    bool valid() const {return kind!=0;}
};

template<class IsCall,class Constant,class Length,class HandlerEntry>
CooperativePollPattern recognize_cooperative_poll(const std::vector<uint8_t>& code,int clock,
        IsCall call,Constant constant,Length length,HandlerEntry handler_entry) {
    const int n=int(code.size());
    auto op=[&](int p,int value){return p>=0&&p<n&&code[p]==value;};
    auto cp=[&](int p){return p>=0&&p+2<n?(int(code[p+1])<<8)|code[p+2]:-1;};
    auto jump=[&](int p){return p>=0&&p+2<n?p+int16_t(cp(p)):-1;};
    auto local=[&](int& p,int generic,int first)->int {
        if(p<0||p>=n)return -1;
        if(code[p]>=first&&code[p]<=first+3)return code[p++]-first;
        if(code[p]==generic&&p+1<n){const int v=code[p+1];p+=2;return v;}
        return -1;
    };
    auto invoke=[&](int& p,int which){
        if(!op(p,184)||!call(cp(p),which))return false;p+=3;return true;
    };
    auto finish=[&](CooperativePollPattern v)->CooperativePollPattern {
        if(v.begin<0||v.end>n||v.begin>=v.end||v.ready<0||v.ready>=n||
           (v.ready>=v.begin&&v.ready<v.end))return {};
        bool c=false,b=false,e=v.end==n,r=false;
        for(int p=0;p<n;) {
            const int len=length(p);if(len<=0||len>n-p)return {};
            if(p==clock)c=true;if(p==v.begin)b=true;if(p==v.end)e=true;if(p==v.ready)r=true;
            if(p>=v.begin&&p<v.end&&handler_entry(p))return {};
            p+=len;
        }
        return c&&b&&e&&r?v:CooperativePollPattern{};
    };
    if(!op(clock,184)||!call(cp(clock),0))return {};

    // now=clock; anchor=min(anchor,now); while(now-anchor<period) yield.
    // The exact loaded Math.min implementation is checked separately.
    {
        int p=clock+3;const int now_local=local(p,55,63);
        const int receiver=local(p,25,42),duplicate=local(p,25,42);
        const int field=cp(p);
        if(now_local>=0&&receiver>=0&&receiver==duplicate&&op(p,180)) {
            p+=3;
            if(local(p,22,30)==now_local&&invoke(p,3)&&op(p,181)&&cp(p)==field) {
                p+=3;const int guard=jump(p),end=p+3;
                if(op(p,167)) {
                    p=guard;
                    if(local(p,22,30)==now_local&&local(p,25,42)==receiver&&op(p,180)&&cp(p)==field) {
                        p+=3;
                        if(op(p++,101)&&op(p,178)) {
                            const int period=cp(p);p+=3;
                            if(op(p++,133)&&op(p++,148)&&op(p,156)&&jump(p)==end) {
                                p+=3;
                                if(invoke(p,1)&&p==clock) {
                                    CooperativePollPattern v;v.kind=1;v.begin=guard;v.end=end;v.ready=end;
                                    v.clock=clock;v.receiver=receiver;v.anchor_cp=field;v.period_cp=period;
                                    return finish(v);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Cached local clock, rollback exit, sleep(0), and an unchanged deadline.
    {
        int p=clock+3;const int cached=local(p,55,63),guard=p;
        if(cached>=0&&local(p,22,30)==cached) {
            const int anchor=local(p,22,30);
            if(anchor>=0&&anchor!=cached&&op(p++,101)&&op(p,20)) {
                const int64_t period=constant(cp(p));p+=3;
                if(period>=2&&period<=120&&op(p++,148)&&op(p,155)) {
                    const int body=jump(p);p+=3;const int end=p+3,ready=jump(p);
                    if(op(p,167)&&body<clock&&guard>clock) {
                        p=body;
                        if(local(p,22,30)==cached&&local(p,22,30)==anchor&&op(p++,148)&&op(p,156)) {
                            const int positive=jump(p);p+=3;
                            if(op(p,167)&&jump(p)==ready&&positive==p+3) {
                                p=positive;
                                if(op(p++,9)&&invoke(p,2)&&p==clock) {
                                    CooperativePollPattern v;v.kind=2;v.begin=body;v.end=end;v.ready=ready;
                                    v.clock=clock;v.anchor_local=anchor;v.period_ms=period;
                                    return finish(v);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // elapsed=clock-anchor; repeat only while 0<elapsed<bounded period.
    {
        int p=clock+3;const int receiver=local(p,25,42),field=cp(p);
        if(receiver>=0&&op(p,180)) {
            p+=3;
            if(op(p++,101)&&op(p++,92)) {
                const int elapsed=local(p,55,63);
                if(elapsed>=0&&op(p,20)) {
                    const int64_t period=constant(cp(p));p+=3;
                    if(period>=2&&period<=120&&op(p++,148)&&op(p,156)) {
                        const int ready=jump(p);p+=3;
                        if(local(p,22,30)==elapsed&&op(p++,9)&&op(p++,148)&&op(p,157)) {
                            const int repeat=jump(p);p+=3;
                            if(op(p,167)&&jump(p)==ready) {
                                const int end=p+3;p=repeat;
                                if(invoke(p,1)&&p==clock) {
                                    CooperativePollPattern v;v.kind=3;v.begin=repeat;v.end=end;v.ready=ready;
                                    v.clock=clock;v.receiver=receiver;v.anchor_cp=field;v.period_ms=period;
                                    return finish(v);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return {};
}

struct CooperativePollObservation {
    int site=-1,canvas=0,cap=0,receiver=0;
    uint32_t generation=0;
    unsigned reads=0;
    uint64_t draws=0;
    int64_t anchor=0,period=0,epoch=0,last_ms=0;
    double scale=0;
    void reset(){*this={};}
};
