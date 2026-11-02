#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// A CPU-idle certificate only. It neither advances a game clock nor changes
// branches, Java locals, waits, frame grouping, or the original loop's body.
struct CooperativePollPattern {
    int kind=0,begin=-1,end=-1,ready=-1,clock=-1;
    int receiver=-1,anchor_local=-1,deadline_local=-1,anchor_cp=-1,period_cp=-1;
    int anchor_owner=-1,anchor_field=-1,period_owner=-1,period_field=-1;
    int64_t period_ms=0;
    bool valid() const {return kind!=0;}
};
// kind4's original Java guard has already pushed its deadline before calling
// the clock. The caller validates anchor/period overflow before forming it.
inline bool cooperative_poll_stack_matches(int kind,std::size_t size,bool is_long,int64_t value,int64_t deadline){
    return (kind==4||kind==7)?size==1&&is_long&&value==deadline:size==0;
}

// A native certificate window, NOT the source Java period. The original
// deadline remains untouched; compare safely before subtracting signed values.
inline bool cooperative_poll_deadline_window(int64_t deadline,int64_t now,int64_t& anchor,int64_t& period){
    if(now<0||deadline<=now||deadline-now>120)return false;
    const int64_t a=deadline>=120?deadline-120:0,p=deadline-a;
    if(p<2)return false;
    anchor=a;period=p;return true;
}

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

    // clock() - a loaded static long anchor < a constant short period.
    // The exit may return to the outer loop; only the pure wait is certified.
    {
        int p=clock+3;
        if(op(p,178)) {
            const int anchor=cp(p);p+=3;
            if(op(p++,101)&&op(p,20)) {
                const int64_t period=constant(cp(p));p+=3;
                if(period>=2&&period<=120&&op(p++,148)&&op(p,156)) {
                    const int ready=jump(p);p+=3;
                    if(invoke(p,1)&&op(p,167)&&jump(p)==clock) {
                        CooperativePollPattern v;v.kind=6;v.begin=clock;v.end=p+3;v.ready=ready;
                        v.clock=clock;v.anchor_cp=anchor;v.period_ms=period;return finish(v);
                    }
                }
            }
        }
    }
    // A previously computed local deadline is already on the original stack.
    // Admit direct deadline<=clock or (deadline-clock)<=0, never a changed body.
    for(int begin=std::max(0,clock-2);begin<clock;++begin){
        int p=begin;const int deadline=local(p,22,30);if(deadline<0||p!=clock)continue;
        p=clock+3;if(op(p,101)){++p;if(!op(p++,9))continue;}
        if(!op(p++,148)||!op(p,158))continue;
        const int ready=jump(p);p+=3;
        if(!invoke(p,1)||!op(p,167)||jump(p)!=begin)continue;
        CooperativePollPattern v;v.kind=7;v.begin=begin;v.end=p+3;v.ready=ready;
        v.clock=clock;v.deadline_local=deadline;return finish(v);
    }

    // while (localAnchor + staticIntegralPeriod > clock()) yield();
    // Only this exact read-only guard/body/exit is admitted, including the
    // original i2l/ladd and operand ordering. No Java instruction is skipped.
    for(int begin=std::max(0,clock-8);begin<clock;++begin){
        int p=begin;const int anchor=local(p,22,30);
        if(anchor<0||!op(p,178))continue;
        const int period=cp(p);p+=3;
        if(!op(p++,133)||!op(p++,97)||p!=clock)continue;
        p=clock+3;if(!op(p++,148)||!op(p,158))continue;
        const int ready=jump(p);p+=3;
        if(!invoke(p,1)||!op(p,167)||jump(p)!=begin||ready!=p+3)continue;
        CooperativePollPattern v;v.kind=4;v.begin=begin;v.end=ready;v.ready=ready;
        v.clock=clock;v.anchor_local=anchor;v.period_cp=period;
        return finish(v);
    }

    // while (clock() - localAnchor < staticIntegralPeriod) yield();
    // Keep the exact subtraction/i2l/comparison polarity and original exit.
    // Clock is the first instruction: its original operand stack is empty.
    {
        int p=clock+3;const int anchor=local(p,22,30);
        if(anchor>=0&&op(p++,101)&&op(p,178)) {
            const int period=cp(p);p+=3;
            if(op(p++,133)&&op(p++,148)&&op(p,156)) {
                const int ready=jump(p);p+=3;
                if(invoke(p,1)&&op(p,167)&&jump(p)==clock&&ready==p+3) {
                    CooperativePollPattern v;v.kind=5;v.begin=clock;v.end=ready;v.ready=ready;
                    v.clock=clock;v.anchor_local=anchor;v.period_cp=period;
                    return finish(v);
                }
            }
        }
    }

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
