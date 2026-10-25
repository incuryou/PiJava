#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

// A deliberately narrow source certificate: start=clock; work;
// elapsed=clock-start; if(elapsed<P) synchronized(this){wait(P-elapsed);}.
// Conditional waits, arbitrary timeouts and repeated waits do not qualify.
struct FrameWaitValue {
    int local=-1, field=0;
    bool same(const FrameWaitValue& x) const {return local==x.local&&field==x.field;}
};
struct FrameWaitPattern {
    int start_pc=-1,end_pc=-1,wait_pc=-1,repaint_pc=-1;
    int64_t period_ms=0;
    int period_local=-1,yield_pc=-1;
    bool branched=false;
    FrameWaitValue start,elapsed,period;
    bool valid() const {return wait_pc>=0;}
};
struct FrameWaitState {
    const void* method=nullptr;
    FrameWaitPattern pattern;
    size_t depth=0;
    uint64_t draws=0;
    int64_t started_ms=0,started_us=0,elapsed_ms=0;
    bool active=false,complete=false,paced=false,timed_out=false;
    uint64_t request=0;
    int canvas=0,cap_period=0;
    int64_t observed_period=0,stable_period=0;
    unsigned stable_samples=0;
    bool requested=false,awaiting_paint=false;
    void cancel() {active=complete=paced=timed_out=requested=awaiting_paint=false;}
    // A reentrant paint can span a mode/Canvas reset. Never reuse its request
    // identity if that owner begins another candidate before it completes.
    void reset() {const auto serial=request;*this={};request=serial;}
};

template<class OpLength,class IsClock,class IsWait,class IsYield,class IsLongField,class LongConstant>
FrameWaitPattern recognize_frame_wait(const std::vector<uint8_t>& code,int clock_pc,
    OpLength length,IsClock is_clock,IsWait is_wait,IsYield is_yield,
    IsLongField is_long_field,LongConstant constant) {
    const int size=int(code.size());
    auto byte=[&](int p,int op){return p>=0&&p<size&&code[size_t(p)]==op;};
    auto cp=[&](int p){return p>=0&&p+1<size?(int(code[size_t(p)])<<8)|code[size_t(p+1)]:0;};
    auto local=[&](int& p,int generic,int short_base) {
        if(p<0||p>=size)return -1;
        if(code[size_t(p)]==generic&&p+1<size){p+=2;return int(code[size_t(p-1)]);}
        if(code[size_t(p)]>=short_base&&code[size_t(p)]<short_base+4)return int(code[size_t(p++)])-short_base;
        return -1;
    };
    auto store=[&](int& p) {
        FrameWaitValue v;v.local=local(p,0x37,0x3f);
        if(v.local>=0)return v;
        if(byte(p,0xb5)&&p+2<size&&is_long_field(cp(p+1))){v.field=cp(p+1);p+=3;}
        return v;
    };
    auto load=[&](int& p) {
        FrameWaitValue v;v.local=local(p,0x16,0x1e);
        if(v.local>=0)return v;
        if(byte(p,0x2a)&&byte(p+1,0xb4)&&p+3<size&&is_long_field(cp(p+2))){v.field=cp(p+2);p+=4;}
        return v;
    };
    FrameWaitPattern result;
    if(!byte(clock_pc,0xb8)||clock_pc+2>=size||!is_clock(cp(clock_pc+1)))return {};
    int p=clock_pc+3;result.start=store(p);
    if(result.start.local<0&&!result.start.field)return {};
    if(result.start.field&&!byte(clock_pc-1,0x2a))return {};
    result.start_pc=clock_pc;
    // Decode instruction boundaries; operands must never become clock sites.
    for(int n=0;n<256&&p<size;++n) {
        const int end=p,len=length(code,size_t(p));
        if(len<=0||p+len>size)return {};
        const int op=code[size_t(p)];
        if((op>=0x99&&op<=0xb1)||op==0xbf||op==0xc6||op==0xc7||op==0xc8||op==0xc9)return {};
        p+=len;
        if(!byte(end,0xb8)||len!=3||!is_clock(cp(end+1)))continue;
        int q=p;
        if(!load(q).same(result.start)||!byte(q++,0x65))return {};
        const bool elapsed_receiver=byte(end-1,0x2a);
        result.elapsed=store(q);
        if(result.elapsed.local<0&&!result.elapsed.field)return {};
        if(result.elapsed.field&&!elapsed_receiver)return {};
        if(!load(q).same(result.elapsed)||!byte(q,0x14)||q+2>=size)return {};
        result.period_ms=constant(cp(q+1));q+=3;
        if(result.period_ms<8||result.period_ms>120||!byte(q++,0x94)||!byte(q,0x9c)||q+2>=size)return {};
        const int ready=q+int16_t(cp(q+1));q+=3;
        // Exact entry lock, no producer flag/queue/IO check before this wait.
        if(!byte(q++,0x2a)||!byte(q++,0x59))return {};
        const int lock=local(q,0x3a,0x4b);
        if(lock<0||!byte(q++,0xc2)||!byte(q++,0x2a)||!byte(q,0x14)||q+2>=size)return {};
        if(constant(cp(q+1))!=result.period_ms)return {};
        q+=3;
        if(!load(q).same(result.elapsed)||!byte(q++,0x65)||!byte(q,0xb6)||q+2>=size||!is_wait(cp(q+1)))return {};
        result.wait_pc=q;q+=3;
        if(local(q,0x19,0x2a)!=lock||!byte(q++,0xc3))return {};
        // On the normal path only monitor exit and gotos may follow. A
        // condition recheck, another wait or IO would change the meaning.
        bool loops=false;
        for(int jumps=0;jumps<5;++jumps) {
            if(!byte(q,0xa7)||q+2>=size)break;
            const int target=q+int16_t(cp(q+1));
            if(target>=0&&target<=clock_pc){loops=true;break;}
            q=target;
        }
        if(!loops||!byte(ready,0xb8)||ready+2>=size||!is_yield(cp(ready+1)))return {};
        result.end_pc=end;return result;
    }
    return {};
}

// Asynchronous form has a separate source certificate. Only forward work
// branches are allowed, every target must be a decoded work boundary, and the
// final operation before elapsed is this.repaint(). The runtime still needs
// this exact request to finish painting while the original timed wait exists.
template<class OpLength,class IsClock,class IsWait,class IsYield,class IsIntField,class IsRepaint>
FrameWaitPattern recognize_async_frame_wait(const std::vector<uint8_t>& code,int clock_pc,
    OpLength length,IsClock is_clock,IsWait is_wait,IsYield is_yield,
    IsIntField is_int_field,IsRepaint is_repaint) {
    const int size=int(code.size());
    auto byte=[&](int p,int op){return p>=0&&p<size&&code[size_t(p)]==op;};
    auto cp=[&](int p){return p>=0&&p+1<size?(int(code[size_t(p)])<<8)|code[size_t(p+1)]:0;};
    auto i32=[&](int p){return int32_t((uint32_t(cp(p))<<16)|uint32_t(cp(p+2)));};
    auto local=[&](int& p,int generic,int base) {
        if(p<0||p>=size)return -1;
        if(byte(p,generic)&&p+1<size){p+=2;return int(code[size_t(p-1)]);}
        if(code[size_t(p)]>=base&&code[size_t(p)]<base+4)return int(code[size_t(p++)])-base;
        return -1;
    };
    auto period=[&](int& p) {
        FrameWaitValue v;
        if(byte(p,0x2a)&&byte(p+1,0xb4)&&p+4<size&&is_int_field(cp(p+2))&&byte(p+4,0x85)) {
            v.field=cp(p+2);p+=5;
        }
        return v;
    };
    if(!byte(clock_pc,0xb8)||clock_pc+2>=size||!is_clock(cp(clock_pc+1)))return {};
    FrameWaitPattern result;result.start_pc=clock_pc;
    int p=clock_pc+3;result.start.local=local(p,0x37,0x3f);
    if(result.start.local<0)return {};
    std::vector<int> boundaries,targets;
    int previous=-1,before_previous=-1;
    for(int n=0;n<256&&p<size;++n) {
        const int at=p,len=length(code,size_t(p)),op=code[size_t(p)];
        if(len<=0||len>size-p)return {};
        boundaries.push_back(at);p+=len;
        if(byte(at,0xb8)&&len==3&&is_clock(cp(at+1))) {
            if(before_previous<0||!byte(before_previous,0x2a)||previous!=at-3||!byte(previous,0xb6)||
               !is_repaint(cp(previous+1)))return {};
            for(int target:targets)if(target<=clock_pc||target>at||
                std::find(boundaries.begin(),boundaries.end(),target)==boundaries.end())return {};
            int q=p;
            if(local(q,0x16,0x1e)!=result.start.local||!byte(q++,0x65))return {};
            const bool duplicate=byte(q,0x5c);if(duplicate)++q;
            result.elapsed.local=local(q,0x37,0x3f);
            if(result.elapsed.local<0)return {};
            if(!duplicate&&local(q,0x16,0x1e)!=result.elapsed.local)return {};
            result.period=period(q);
            if(!result.period.field||!byte(q++,0x94)||!byte(q,0x9c)||q+2>=size)return {};
            const int ready=q+int16_t(cp(q+1));q+=3;
            if(!byte(q++,0x2a)||!byte(q++,0x59))return {};
            const int lock=local(q,0x3a,0x4b);
            if(lock<0||!byte(q++,0xc2)||!byte(q++,0x2a)||!period(q).same(result.period)||
               local(q,0x16,0x1e)!=result.elapsed.local||!byte(q++,0x65)||!byte(q,0xb6)||q+2>=size||!is_wait(cp(q+1)))return {};
            result.wait_pc=q;q+=3;
            if(local(q,0x19,0x2a)!=lock||!byte(q++,0xc3))return {};
            bool loops=false;
            for(int jumps=0;jumps<5;++jumps) {
                if(!byte(q,0xa7)||q+2>=size)break;
                const int target=q+int16_t(cp(q+1));
                if(target>=0&&target<=clock_pc){loops=true;break;}
                q=target;
            }
            if(!loops||!byte(ready,0xb8)||ready+2>=size||!is_yield(cp(ready+1)))return {};
            result.end_pc=at;result.repaint_pc=previous;return result;
        }
        if((op>=0x99&&op<=0xa7)||op==0xc6||op==0xc7) {
            const int target=at+int16_t(cp(at+1));if(target<=at)return {};targets.push_back(target);
        } else if(op==0xaa||op==0xab) {
            int q=(at+4)&~3;
            auto target=[&](int offset) {
                const int64_t to=int64_t(at)+i32(offset);
                if(to<=at||to>=size)return false;
                targets.push_back(int(to));return true;
            };
            if(!target(q))return {};
            const int64_t count=op==0xaa?int64_t(i32(q+8))-i32(q+4)+1:i32(q+4);
            if(count<0||count>size)return {};
            q+=op==0xaa?12:8;
            for(int i=0;i<count;++i)if(!target(q+i*(op==0xaa?4:8)+(op==0xaa?0:4)))return {};
        } else if((op>=0xa8&&op<=0xb1)||op==0xbf||op==0xc8||op==0xc9)return {};
        before_previous=previous;previous=at;
    }
    return {};
}

// Synchronous conditional-paint work with a local constant frame period.
// Work branches may only join decoded work instructions; they cannot bypass
// the elapsed/remainder wait. The timestamp local is immutable throughout work.
template<class OpLength,class IsClock,class IsWait,class IsYield>
FrameWaitPattern recognize_branched_frame_wait(const std::vector<uint8_t>& code,int clock_pc,
    OpLength length,IsClock is_clock,IsWait is_wait,IsYield is_yield) {
    const int size=int(code.size());
    auto byte=[&](int p,int op){return p>=0&&p<size&&code[size_t(p)]==op;};
    auto cp=[&](int p){return p>=0&&p+1<size?(int(code[size_t(p)])<<8)|code[size_t(p+1)]:0;};
    auto i32=[&](int p){return int32_t((uint32_t(cp(p))<<16)|uint32_t(cp(p+2)));};
    auto local=[&](int& p,int generic,int base) {
        if(p<0||p>=size)return -1;
        if(byte(p,generic)&&p+1<size){p+=2;return int(code[size_t(p-1)]);}
        if(code[size_t(p)]>=base&&code[size_t(p)]<base+4)return int(code[size_t(p++)])-base;
        return -1;
    };
    auto jump=[&](int p){return p+int16_t(cp(p+1));};
    if(!byte(clock_pc,0xb8)||clock_pc+2>=size||!is_clock(cp(clock_pc+1)))return {};
    FrameWaitPattern base;base.start_pc=clock_pc;base.branched=true;
    int begin=clock_pc+3;base.start.local=local(begin,0x37,0x3f);
    if(base.start.local<0)return {};
    std::vector<int> boundaries,targets;
    for(int p=begin,n=0;p<size&&n<256;++n) {
        const int at=p,len=length(code,size_t(p)),op=code[size_t(p)];
        if(len<=0||len>size-p)return {};
        boundaries.push_back(at);p+=len;
        if(byte(at,0xb8)&&len==3&&is_clock(cp(at+1))) {
            // Statistics clocks are not elapsed clocks unless their immediate
            // value flow subtracts this exact immutable start timestamp.
            int q=p;
            if(local(q,0x16,0x1e)==base.start.local&&byte(q++,0x65)) {
                auto result=base;result.end_pc=at;
                result.elapsed.local=local(q,0x37,0x3f);
                if(result.elapsed.local<0||result.elapsed.local==base.start.local||
                   result.elapsed.local==base.start.local+1)return {};
                int period=0;
                if(byte(q,0x10)&&q+1<size){period=int8_t(code[size_t(q+1)]);q+=2;}
                else if(byte(q,0x11)&&q+2<size){period=int16_t(cp(q+1));q+=3;}
                else return {};
                result.period_ms=period;result.period_local=local(q,0x36,0x3b);
                if(period<8||period>120||result.period_local<0||
                   result.period_local==base.start.local||result.period_local==base.start.local+1||
                   result.period_local==result.elapsed.local||result.period_local==result.elapsed.local+1)return {};
                if(local(q,0x16,0x1e)!=result.elapsed.local||local(q,0x15,0x1a)!=result.period_local||
                   !byte(q++,0x85)||!byte(q++,0x94)||!byte(q,0x9c)||q+2>=size)return {};
                const int ready=jump(q);q+=3;
                if(!byte(q++,0x2a)||!byte(q++,0x59))return {};
                const int lock=local(q,0x3a,0x4b);
                if(lock<0||!byte(q++,0xc2)||!byte(q++,0x2a)||local(q,0x15,0x1a)!=result.period_local||
                   !byte(q++,0x85)||local(q,0x16,0x1e)!=result.elapsed.local||!byte(q++,0x65)||
                   !byte(q,0xb6)||q+2>=size||!is_wait(cp(q+1)))return {};
                result.wait_pc=q;q+=3;
                if(local(q,0x19,0x2a)!=lock||!byte(q++,0xc3))return {};
                for(int n=0;n<5&&q!=ready;++n) {
                    if(!byte(q,0xa7)||q+2>=size)return {};
                    const int next=jump(q);if(next<=q||next>ready)return {};q=next;
                }
                if(q!=ready)return {};
                // Some compilers retain a dead Thread receiver before yield.
                int dead=q;
                if(local(dead,0x19,0x2a)>=0&&byte(dead,0x57))q=dead+1;
                if(!byte(q,0xb8)||q+2>=size||!is_yield(cp(q+1)))return {};
                result.yield_pc=q;q+=3;
                if(!byte(q,0xa7)||q+2>=size||jump(q)<0||jump(q)>clock_pc)return {};
                for(int target:targets)if(target>at||
                    std::find(boundaries.begin(),boundaries.end(),target)==boundaries.end())return {};
                return result;
            }
        }
        // All stores to either slot of the long start local invalidate the
        // source proof, including narrower stores, iinc and wide operations.
        int stored=-1,width=1;
        for(int kind=0;kind<5;++kind) {
            int q=at;const int slot=local(q,0x36+kind,0x3b+kind*4);
            if(slot>=0){stored=slot;width=(kind==1||kind==3)?2:1;break;}
        }
        if(op==0x84&&at+1<size)stored=code[size_t(at+1)];
        if(op==0xc4)return {};
        if(stored>=0&&stored<=base.start.local+1&&stored+width>base.start.local)return {};
        if((op>=0x99&&op<=0xa7)||op==0xc6||op==0xc7) {
            if(len!=3)return {};
            const int target=jump(at);
            if(op==0xa7&&target>=0&&target<=clock_pc)continue;
            if(target<=at||target>=size)return {};
            targets.push_back(target);
        } else if(op==0xaa||op==0xab) {
            int q=(at+4)&~3;
            auto target=[&](int offset) {
                if(offset<0||offset+3>=size)return false;
                const int64_t to=int64_t(at)+i32(offset);
                if(to<=at||to>=size)return false;targets.push_back(int(to));return true;
            };
            if(!target(q)||q+11>=size)return {};
            const int64_t count=op==0xaa?int64_t(i32(q+8))-i32(q+4)+1:i32(q+4);
            if(count<0||count>size)return {};
            q+=op==0xaa?12:8;
            for(int n=0;n<count;++n)if(!target(q+n*(op==0xaa?4:8)+(op==0xaa?0:4)))return {};
        } else if((op>=0xa8&&op<=0xb1)||op==0xbf||op==0xc8||op==0xc9||op==0xc2||op==0xc3)return {};
        if((op==0xb6||op==0xb8)&&len==3&&(is_wait(cp(at+1))||is_yield(cp(at+1))))return {};
    }
    return {};
}
