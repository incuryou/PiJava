#pragma once
#include "poll_gate.h"
#include <utility>

// A clock may be retained in a long local while its difference is also cached.
// The ready path writes that clock to the anchor, then a later fresh clock
// refreshes it before drawing. Certify both definitions, never an arbitrary
// value stored in the same field. Runtime ownership remains FieldGateAnchor's.
// SameField returns 1 for one resolved storage identity, 0 for distinct storage,
// and -1 for an unresolved potential alias; uncertainty must reject the proof.
template<class LongConstant,class IsYield,class IsClock,class IsLongStatic,
         class SameField,class OpLength,class HandlerEntry>
TimeGatePattern recognize_aliased_field_time_gate(const std::vector<uint8_t>& code,
        int clock_pc,int next_pc,LongConstant constant,IsYield is_yield,IsClock is_clock,
        IsLongStatic is_long_static,SameField same_field,OpLength length,HandlerEntry handler_entry) {
    const int n=int(code.size());
    auto op=[&](int p,int v){return p>=0&&p<n&&code[p]==v;};
    auto cp=[&](int p){return p>=0&&p+2<n?(int(code[p+1])<<8)|code[p+2]:-1;};
    auto jump=[&](int p){return p+int16_t(cp(p));};
    auto local=[&](int& p,bool store){
        if(p<0||p>=n)return -1;
        const int base=store?0x3f:0x1e,generic=store?0x37:0x16;
        if(code[p]>=base&&code[p]<=base+3)return int(code[p++])-base;
        if(code[p]==generic&&p+1<n){const int value=code[p+1];p+=2;return value;}
        return -1;
    };
    if(!op(clock_pc,0xb8)||!is_clock(cp(clock_pc))||next_pc!=clock_pc+3||!op(next_pc,0x5c))return {};
    std::vector<int> boundaries;
    std::vector<std::pair<int,int>> edges;
    auto s32=[&](int p)->int64_t {
        if(p<0||p+4>n)return INT64_MAX;
        return int32_t((uint32_t(code[p])<<24)|(uint32_t(code[p+1])<<16)|(uint32_t(code[p+2])<<8)|code[p+3]);
    };
    for(int at=0;at<n;) {
        const int len=length(at);if(len<=0||at+len>n)return {};
        boundaries.push_back(at);
        const int byte=code[at];
        if((byte>=0x99&&byte<=0xa8)||byte==0xc6||byte==0xc7)edges.push_back({at,jump(at)});
        else if(byte==0xc8||byte==0xc9) {
            const int64_t target=at+s32(at+1);if(target<0||target>=n)return {};
            edges.push_back({at,int(target)});
        } else if(byte==0xaa||byte==0xab) {
            const int base=(at+4)&~3;
            const int64_t count=byte==0xaa?s32(base+8)-s32(base+4)+1:s32(base+4);
            const int width=byte==0xaa?4:8,start=base+(byte==0xaa?12:8);
            if(count<0||count>(n-start)/width)return {};
            const int64_t def=at+s32(base);if(def<0||def>=n)return {};
            edges.push_back({at,int(def)});
            for(int64_t i=0;i<count;++i) {
                const int64_t target=at+s32(start+int(i)*width+(byte==0xaa?0:4));
                if(target<0||target>=n)return {};
                edges.push_back({at,int(target)});
            }
        }
        at+=len;
    }
    auto boundary=[&](int p){return std::binary_search(boundaries.begin(),boundaries.end(),p);};
    for(const auto& edge:edges)if(!boundary(edge.second))return {};
    if(!boundary(clock_pc)||!op(clock_pc,0xb8)||!is_clock(cp(clock_pc))||next_pc!=clock_pc+3)return {};
    int p=next_pc;
    if(!op(p++,0x5c))return {};
    const int clock_local=local(p,true);if(clock_local<0)return {};
    if(!op(p,0xb2)||!is_long_static(cp(p)))return {};
    const int anchor=cp(p);p+=3;
    if(!op(p++,0x65))return {};
    if(op(p,0x5c)) {++p;const int elapsed=local(p,true);if(elapsed<0||elapsed==clock_local||elapsed+1==clock_local||clock_local+1==elapsed)return {};}
    if(!op(p,0x14))return {};
    int64_t period=constant(cp(p));p+=3;
    if(period<8||period>120||!op(p++,0x94))return {};
    const int branch=p;
    int ready=-1,wait=-1;
    if(op(p,0x9b)||op(p,0x9e)){wait=jump(p);ready=p+3;}
    else if(op(p,0x9c)||op(p,0x9d)){wait=p+3;ready=jump(p);}
    else return {};
    if(op(p,0x9e)||op(p,0x9d))++period;
    if(period>120||ready<=branch||!boundary(ready))return {};
    bool closes=false;
    for(int k=0;k<8&&boundary(wait);++k) {
        if(wait==clock_pc){closes=true;break;}
        if(op(wait,0)){++wait;continue;}
        if(op(wait,0xb8)&&is_yield(cp(wait))){wait+=3;continue;}
        if(op(wait,0xa7)&&jump(wait)==clock_pc){closes=true;break;}
        return {};
    }
    if(!closes)return {};
    p=ready;
    if(local(p,false)!=clock_local||!op(p,0xb3)||same_field(cp(p),anchor)!=1)return {};
    const int ready_store=p;
    // Reject alternative entries which could bypass the value's definition.
    auto entry_in=[&](int first,int last,int allowed_source=-1) {
        for(const auto& edge:edges)if(edge.second>=first&&edge.second<=last&&edge.first!=allowed_source)return true;
        for(int at:boundaries)if(at>=first&&at<=last&&handler_entry(at))return true;
        return false;
    };
    if(entry_in(clock_pc+1,branch)||entry_in(ready,ready_store,branch))return {};
    int anchor_clock=-1,anchor_store=-1;
    // Tiny symbolic long stack: true means this unmodified clock value.
    // No call, branch, array, object, conversion or integer operation can hide
    // an alternate anchor definition in an accepted straight-line segment.
    for(int at:boundaries) {
        if(at==clock_pc||!op(at,0xb8)||!is_clock(cp(at)))continue;
        std::vector<bool> stack{true};std::vector<int> aliases;
        int q=at+3,found=-1;
        for(int k=0;k<24&&boundary(q)&&q-at<=64;++k) {
            int next=q;const int load=local(next,false);
            int store=-1;if(load<0){next=q;store=local(next,true);}
            if(load>=0){stack.push_back(std::find(aliases.begin(),aliases.end(),load)!=aliases.end());q=next;continue;}
            if(store>=0) {
                if(stack.empty())break;
                aliases.erase(std::remove_if(aliases.begin(),aliases.end(),[&](int x){return x==store||x+1==store||store+1==x;}),aliases.end());
                if(stack.back())aliases.push_back(store);stack.pop_back();q=next;continue;
            }
            if(op(q,0x5c)){if(stack.empty())break;stack.push_back(stack.back());++q;continue;}
            if(op(q,0xb2)&&is_long_static(cp(q))){stack.push_back(false);q+=3;continue;}
            if(op(q,0x09)||op(q,0x0a)||op(q,0x14)){stack.push_back(false);q+=op(q,0x14)?3:1;continue;}
            if(op(q,0x61)||op(q,0x65)){if(stack.size()<2)break;stack.pop_back();stack.back()=false;++q;continue;}
            if(op(q,0xb3)&&is_long_static(cp(q))) {
                if(stack.empty())break;
                const bool clock=stack.back();stack.pop_back();
                const int relation=same_field(cp(q),anchor);
                if(relation<0)return {};
                if(relation==1){if(clock)found=q;break;}
                q+=3;continue;
            }
            break;
        }
        if(found>=0) {
            if(anchor_clock>=0||at<=ready_store||entry_in(at+1,found))return {};
            anchor_clock=at;anchor_store=found;
        }
    }
    if(anchor_clock<0)return {};
    for(int at:boundaries)if(op(at,0xb3)) {
        const int relation=same_field(cp(at),anchor);
        if(relation<0||(relation==1&&at!=ready_store&&at!=anchor_store))return {};
    }
    TimeGatePattern result;result.static_field=anchor;result.anchor_clock_pc=anchor_clock;
    result.site_pc=clock_pc;result.millis=period;return result;
}
