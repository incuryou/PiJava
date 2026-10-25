#pragma once
#include "jit_budget.h"
#include "jit_code_pool.h"
// Original bounded, non-allocating Java int leaf compiler. There is deliberately
// no support for loops, heap access, monitors or calls: rejected methods execute
// in the reference interpreter. Compiled code and addresses are never saved.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace jit236 {
constexpr unsigned MaxLocals=32,MaxStack=64,MaxCode=512,MaxOps=128;
struct Site { uint64_t epoch=0; uint32_t calls=0; int entry=-1; };
struct Constant { bool integer=false; int32_t value=0; };
struct Spec {
    const uint8_t* code=nullptr;size_t size=0;
    const std::string* descriptor=nullptr;uint16_t flags=0,max_locals=0,max_stack=0;
    bool handlers=false,special=false;
    const std::vector<Constant>* constants=nullptr;
};
struct Ins { int pc=0,next=0,target=-1,op=0,index=0,depth=0;int32_t value=0; };
struct Program { std::vector<Ins> ins;unsigned args=0,locals=0,stack=0,max_ops=0; };
inline bool int_type(char c){return c=='I'||c=='B'||c=='S'||c=='C'||c=='Z';}
inline bool descriptor(const std::string& s,unsigned& args){
    args=0;if(s.size()<3||s[0]!='(')return false;size_t i=1;
    while(i<s.size()&&int_type(s[i])){++args;++i;if(args>16)return false;}
    return i+2==s.size()&&s[i]==')'&&int_type(s[i+1]);
}
inline int signed16(uint8_t a,uint8_t b){unsigned v=(unsigned(a)<<8)|b;return v<32768?int(v):int(v)-65536;}
inline bool verify(const Spec& s,Program& p){
    p=Program{};
    if(!s.code||!s.descriptor||!s.size||s.size>MaxCode||s.handlers||s.special||
       !(s.flags&8)||(s.flags&(0x0020|0x0100|0x0400))||s.max_locals>MaxLocals||s.max_stack>MaxStack||
       !descriptor(*s.descriptor,p.args)||s.max_locals<p.args)return false;
    p.locals=s.max_locals;p.stack=s.max_stack;
    std::array<int,MaxCode+1> at{};at.fill(-1);
    size_t pc=0;
    while(pc<s.size){
        if(p.ins.size()>=MaxOps)return false;
        Ins n;n.pc=int(pc);n.op=s.code[pc++];
        auto byte=[&](int& v){if(pc>=s.size)return false;v=s.code[pc++];return true;};
        int x=0,y=0;
        switch(n.op){
        case 0:break;
        case 2:case 3:case 4:case 5:case 6:case 7:case 8:n.value=n.op-3;break;
        case 16:if(!byte(x))return false;n.value=x<128?x:x-256;break;
        case 17:if(!byte(x)||!byte(y))return false;n.value=signed16(uint8_t(x),uint8_t(y));break;
        case 18:case 19:{if(!byte(x))return false;if(n.op==19){if(!byte(y))return false;x=(x<<8)|y;}
            if(!s.constants||size_t(x)>=s.constants->size()||!(*s.constants)[x].integer)return false;n.value=(*s.constants)[x].value;break;}
        case 21:case 54:if(!byte(n.index))return false;break;
        case 26:case 27:case 28:case 29:n.index=n.op-26;break;
        case 59:case 60:case 61:case 62:n.index=n.op-59;break;
        case 132:if(!byte(n.index)||!byte(x))return false;n.value=x<128?x:x-256;break;
        case 87:case 89:case 95: // pop, dup, swap (category-1 int only)
        case 96:case 100:case 104:case 116:case 120:case 122:case 124:case 126:case 128:case 130:
        case 145:case 146:case 147:case 172:break;
        case 153:case 154:case 155:case 156:case 157:case 158:
        case 159:case 160:case 161:case 162:case 163:case 164:case 167:
            if(!byte(x)||!byte(y))return false;n.target=n.pc+signed16(uint8_t(x),uint8_t(y));break;
        default:return false;
        }
        n.next=int(pc);at[n.pc]=int(p.ins.size());p.ins.push_back(n);
    }
    struct State{bool live=false;int depth=0;uint32_t assigned=0;unsigned ops=0;};
    std::array<State,MaxCode+1> states{};
    states[0]={true,0,p.args?((uint32_t(1)<<p.args)-1):0,0};
    bool returned=false;
    auto send=[&](int pc,int depth,uint32_t assigned,unsigned ops){
        if(pc<0||size_t(pc)>=s.size||at[size_t(pc)]<0)return false;
        State& d=states[size_t(pc)];
        if(d.live){if(d.depth!=depth)return false;d.assigned&=assigned;d.ops=std::max(d.ops,ops);}
        else d={true,depth,assigned,ops};return true;
    };
    for(auto& n:p.ins){
        State q=states[size_t(n.pc)];
        // Reject unreachable bytes as well; no ambiguous or hidden exception path.
        if(!q.live)return false;n.depth=q.depth;
        int need=0,delta=0;bool load=(n.op==21||(n.op>=26&&n.op<=29));
        bool store=(n.op==54||(n.op>=59&&n.op<=62));
        if((load||store||n.op==132)&&(n.index<0||unsigned(n.index)>=p.locals))return false;
        if((load||n.op==132)&&!(q.assigned&(uint32_t(1)<<n.index)))return false;
        if(load||(n.op>=2&&n.op<=8)||n.op==16||n.op==17||n.op==18||n.op==19)delta=1;
        else if(store||n.op==87){need=1;delta=-1;}
        else if(n.op==89){need=1;delta=1;}
        else if(n.op==95)need=2;
        else if(n.op==96||n.op==100||n.op==104||n.op==120||n.op==122||n.op==124||n.op==126||n.op==128||n.op==130){need=2;delta=-1;}
        else if(n.op==116||(n.op>=145&&n.op<=147)){need=1;}
        else if(n.op>=153&&n.op<=158){need=1;delta=-1;}
        else if(n.op>=159&&n.op<=164){need=2;delta=-2;}
        else if(n.op==172){need=1;delta=-1;}
        if(q.depth<need||q.depth+delta<0||unsigned(q.depth+delta)>p.stack)return false;
        q.depth+=delta;if(store)q.assigned|=uint32_t(1)<<n.index;
        ++q.ops;if(q.ops>MaxOps)return false;
        if(n.op==172){if(q.depth!=0)return false;returned=true;p.max_ops=std::max(p.max_ops,q.ops);continue;}
        if((n.op>=153&&n.op<=164)||n.op==167){if(n.target<n.next||!send(n.target,q.depth,q.assigned,q.ops))return false;}
        if(n.op!=167&&!send(n.next,q.depth,q.assigned,q.ops))return false;
    }
    return returned;
}
struct Bytes{
    std::vector<uint8_t> v;
    void b(uint8_t n){v.push_back(n);}void u32(uint32_t n){for(int i=0;i<4;++i)b(uint8_t(n>>(i*8)));}
    void patch32(size_t at,uint32_t n){for(int i=0;i<4;++i)v[at+i]=uint8_t(n>>(i*8));}
};
// x86-64 SysV: args pointer in RDI; scratch stack, EAX/EDX/ECX and R8 count.
// AArch64 AAPCS64: args pointer in X0; scratch stack, W9/W10/W12 count.
// Return uint64_t: low32 Java int bits; high32 exact executed bytecode count.
// 0.2.113 install-id guard (id != 0): the entry compares args[StaleIdSlot] with the id compiled into this code.
// On mismatch it returns StaleMark<<32 | own id before touching anything (stale code at a reused address).
constexpr unsigned StaleIdSlot=16;constexpr uint32_t StaleMark=0xffffffffu;
inline bool stale_result(uint64_t r){return uint32_t(r>>32)==StaleMark;}
inline std::vector<uint8_t> emit_x64(const Program& p,uint32_t id=0){
    Bytes o;std::array<size_t,MaxCode+1> labels{};
    struct Fix{size_t at;int pc;};std::vector<Fix> fix;
    size_t guard_at=SIZE_MAX;
    if(id){o.b(0x8b);o.b(0x47);o.b(uint8_t(StaleIdSlot*4));o.b(0x3d);o.u32(id);o.b(0x0f);o.b(0x85);guard_at=o.v.size();o.u32(0);}   // mov eax,[rdi+64]; cmp eax,id; jne stale
    auto mem=[&](bool load,int reg,unsigned off){o.b(load?0x8b:0x89);o.b(uint8_t(0x84|(reg<<3)));o.b(0x24);o.u32(off);};
    auto l=[&](int r,unsigned slot){mem(true,r,slot*4);};auto st=[&](int r,unsigned slot){mem(false,r,slot*4);};
    constexpr unsigned S=MaxLocals;
    o.b(0x48);o.b(0x81);o.b(0xec);o.u32(512); // aligned scratch; no calls
    for(unsigned i=0;i<p.args;++i){o.b(0x8b);o.b(0x87);o.u32(i*4);st(0,i);}
    o.b(0x45);o.b(0x31);o.b(0xc0);
    for(const auto& n:p.ins){
        labels[size_t(n.pc)]=o.v.size();o.b(0x41);o.b(0xff);o.b(0xc0); // inc r8d (one original opcode)
        unsigned d=unsigned(n.depth);bool load=n.op==21||(n.op>=26&&n.op<=29),store=n.op==54||(n.op>=59&&n.op<=62);
        if(load){l(0,unsigned(n.index));st(0,S+d);}
        else if(store){l(0,S+d-1);st(0,unsigned(n.index));}
        else if((n.op>=2&&n.op<=8)||n.op==16||n.op==17||n.op==18||n.op==19){o.b(0xb8);o.u32(uint32_t(n.value));st(0,S+d);}
        else if(n.op==132){l(0,unsigned(n.index));o.b(0x05);o.u32(uint32_t(n.value));st(0,unsigned(n.index));}
        else if(n.op==89){l(0,S+d-1);st(0,S+d);}
        else if(n.op==95){l(0,S+d-1);l(2,S+d-2);st(2,S+d-1);st(0,S+d-2);}
        else if(n.op==116||n.op==145||n.op==146||n.op==147){
            l(0,S+d-1);
            if(n.op==116){o.b(0xf7);o.b(0xd8);} // neg eax
            else if(n.op==145){o.b(0x0f);o.b(0xbe);o.b(0xc0);}
            else if(n.op==146){o.b(0x0f);o.b(0xb7);o.b(0xc0);}
            else {o.b(0x0f);o.b(0xbf);o.b(0xc0);}st(0,S+d-1);
        }else if(n.op==96||n.op==100||n.op==104||n.op==120||n.op==122||n.op==124||n.op==126||n.op==128||n.op==130){
            l(0,S+d-2);
            if(n.op==120||n.op==122||n.op==124){l(1,S+d-1);o.b(0xd3);o.b(n.op==120?0xe0:n.op==122?0xf8:0xe8);}
            else {l(2,S+d-1);if(n.op==104){o.b(0x0f);o.b(0xaf);o.b(0xc2);}
              else {o.b(n.op==96?0x01:n.op==100?0x29:n.op==126?0x21:n.op==128?0x09:0x31);o.b(0xd0);}}
            st(0,S+d-2);
        }else if(n.op>=153&&n.op<=164){
            if(n.op<=158){l(0,S+d-1);o.b(0x85);o.b(0xc0);}
            else {l(0,S+d-2);l(2,S+d-1);o.b(0x39);o.b(0xd0);}
            const uint8_t cc[]={0x84,0x85,0x8c,0x8d,0x8f,0x8e};
            o.b(0x0f);o.b(cc[(n.op-(n.op<=158?153:159))]);size_t at=o.v.size();o.u32(0);fix.push_back({at,n.target});
        }else if(n.op==167){o.b(0xe9);size_t at=o.v.size();o.u32(0);fix.push_back({at,n.target});}
        else if(n.op==172){l(0,S+d-1);o.b(0x4c);o.b(0x89);o.b(0xc2);o.b(0x48);o.b(0xc1);o.b(0xe2);o.b(32);o.b(0x48);o.b(0x09);o.b(0xd0);
            o.b(0x48);o.b(0x81);o.b(0xc4);o.u32(512);o.b(0xc3);}
        // nop and pop only update the verified virtual depth.
    }
    for(const auto& f:fix){int64_t rel=int64_t(labels[size_t(f.pc)])-int64_t(f.at+4);o.patch32(f.at,uint32_t(int32_t(rel)));}
    if(id){o.patch32(guard_at,uint32_t(o.v.size()-(guard_at+4)));
        o.b(0xb8);o.u32(id);o.b(0x48);o.b(0xba);for(int i=0;i<8;++i)o.b(uint8_t((uint64_t(StaleMark)<<32)>>(8*i)));o.b(0x48);o.b(0x09);o.b(0xd0);o.b(0xc3);}   // eax=id; rdx=mark<<32; or rax,rdx; ret
    return std::move(o.v);
}
inline std::vector<uint8_t> emit_a64(const Program& p,uint32_t id=0){
    Bytes o;std::array<size_t,MaxCode+1> labels{};
    struct Fix{size_t at;int pc;uint32_t base;bool conditional;};std::vector<Fix>fix;
    auto l=[&](unsigned r,unsigned slot){o.u32(0xb9400000u|((slot&4095u)<<10)|(31u<<5)|r);};
    auto st=[&](unsigned r,unsigned slot){o.u32(0xb9000000u|((slot&4095u)<<10)|(31u<<5)|r);};
    auto imm=[&](unsigned r,uint32_t n){o.u32(0x52800000u|((n&65535u)<<5)|r);if(n>>16)o.u32(0x72a00000u|((n>>16)<<5)|r);};
    size_t guard_at=SIZE_MAX;
    if(id){o.u32(0xb9400009u|(StaleIdSlot<<10));imm(10,id);o.u32(0x6b0a013fu);guard_at=o.v.size();o.u32(0x54000001u);}   // ldr w9,[x0,#64]; cmp w9,w10; b.ne stale
    constexpr unsigned S=MaxLocals;
    o.u32(0xd10803ffu); // sub sp,sp,#512
    for(unsigned i=0;i<p.args;++i){o.u32(0xb9400009u|(i<<10));st(9,i);}
    imm(12,0);
    for(const auto& n:p.ins){
        labels[size_t(n.pc)]=o.v.size();o.u32(0x1100058cu); // add w12,w12,#1
        unsigned d=unsigned(n.depth);bool load=n.op==21||(n.op>=26&&n.op<=29),store=n.op==54||(n.op>=59&&n.op<=62);
        if(load){l(9,unsigned(n.index));st(9,S+d);}
        else if(store){l(9,S+d-1);st(9,unsigned(n.index));}
        else if((n.op>=2&&n.op<=8)||n.op==16||n.op==17||n.op==18||n.op==19){imm(9,uint32_t(n.value));st(9,S+d);}
        else if(n.op==132){l(9,unsigned(n.index));imm(10,uint32_t(n.value));o.u32(0x0b0a0129u);st(9,unsigned(n.index));}
        else if(n.op==89){l(9,S+d-1);st(9,S+d);}
        else if(n.op==95){l(9,S+d-1);l(10,S+d-2);st(10,S+d-1);st(9,S+d-2);}
        else if(n.op==116||n.op==145||n.op==146||n.op==147){l(9,S+d-1);
            if(n.op==116)o.u32(0x4b0903e9u);
            else if(n.op==145)o.u32(0x13001d29u);
            else if(n.op==146)o.u32(0x53003d29u);
            else o.u32(0x13003d29u);st(9,S+d-1);
        }else if(n.op==96||n.op==100||n.op==104||n.op==120||n.op==122||n.op==124||n.op==126||n.op==128||n.op==130){
            l(9,S+d-2);l(10,S+d-1);uint32_t ins=0;
            switch(n.op){case 96:ins=0x0b0a0129u;break;case 100:ins=0x4b0a0129u;break;case 104:ins=0x1b0a7d29u;break;
            case 120:ins=0x1aca2129u;break;case 122:ins=0x1aca2929u;break;case 124:ins=0x1aca2529u;break;
            case 126:ins=0x0a0a0129u;break;case 128:ins=0x2a0a0129u;break;case 130:ins=0x4a0a0129u;break;}
            o.u32(ins);st(9,S+d-2);
        }else if(n.op>=153&&n.op<=164){
            if(n.op<=158){l(9,S+d-1);o.u32(0x7100013fu);} // cmp w9,#0
            else {l(9,S+d-2);l(10,S+d-1);o.u32(0x6b0a013fu);}
            const unsigned cc[]={0,1,11,10,12,13};uint32_t base=0x54000000u|cc[n.op-(n.op<=158?153:159)];
            fix.push_back({o.v.size(),n.target,base,true});o.u32(base);
        }else if(n.op==167){fix.push_back({o.v.size(),n.target,0x14000000u,false});o.u32(0x14000000u);}
        else if(n.op==172){l(9,S+d-1);o.u32(0xaa0c8120u); // orr x0,x9,x12,lsl#32
            o.u32(0x910803ffu);o.u32(0xd65f03c0u);}
    }
    for(const auto& f:fix){int64_t rel=(int64_t(labels[size_t(f.pc)])-int64_t(f.at))/4;
        o.patch32(f.at,f.base|(f.conditional?((uint32_t(rel)&0x7ffffu)<<5):(uint32_t(rel)&0x3ffffffu)));}
    if(id){const int64_t rel=(int64_t(o.v.size())-int64_t(guard_at))/4;o.patch32(guard_at,0x54000001u|((uint32_t(rel)&0x7ffffu)<<5));
        imm(0,id);o.u32(0xd2dfffe1u);o.u32(0xf2ffffe1u);o.u32(0xaa010000u);o.u32(0xd65f03c0u);}   // w0=id; x1=mark<<32; orr x0,x0,x1; ret
    return std::move(o.v);
}
class Executable {
public:
    using Fn=uint64_t(*)(const int32_t*);
    Fn fn=nullptr;size_t size=0;uint32_t id=0;   // id: install id compiled into the code (0.2.113; 0 = unguarded)
private:
    CodeSlice memory_;
public:
    Executable()=default;Executable(const Executable&)=delete;Executable&operator=(const Executable&)=delete;
    Executable(Executable&& other)noexcept{swap(other);}Executable&operator=(Executable&& other)noexcept{if(this!=&other){close();swap(other);}return *this;}
    ~Executable(){close();}
    void swap(Executable& o)noexcept{std::swap(fn,o.fn);std::swap(size,o.size);std::swap(id,o.id);memory_.swap(o.memory_);}
    void close(){memory_.close();fn=nullptr;size=0;id=0;}
    bool executable()const{return memory_.executable();}
    static const char*& last_error(){static const char* e="none";return e;}
    static uint32_t& last_result(){static uint32_t r=0;return r;}
    static int& fault(){static int f=0;return f;}
    bool install(const std::vector<uint8_t>& bytes,uint32_t code_id=0,InstallMode mode=InstallMode::Normal){
        if(active_code_calls()){last_error()="install_during_compiled_call";return false;}
        close();if(fault()==1){last_error()="fault_injected_install";return false;}
        if(!memory_.install(bytes,last_error(),last_result(),mode))return false;
        size=(bytes.size()+4095)&~size_t(4095);fn=reinterpret_cast<Fn>(memory_.address());id=code_id;return true;
    }
};
// 0.2.113: process-unique install ids (never 0, never StaleMark); every compiled body gets its own.
inline uint32_t next_install_id(){static uint32_t n=0x4a320000u;do{++n;}while(n==0||n==StaleMark);return n;}
inline const char* wx_mode(){
#ifdef __SWITCH__
    return "switch_jit_rw_then_rx";
#elif defined(__unix__) || defined(__APPLE__)
    return "mmap_rw_then_mprotect_rx";
#else
    return "none";
#endif
}
inline const char* architecture(){
#if defined(__aarch64__)
    return "aarch64";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return "unsupported";
#endif
}
inline std::vector<uint8_t> emit_host(const Program& p,uint32_t id=0){
#if defined(__aarch64__)
    return emit_a64(p,id);
#elif defined(__x86_64__)
    return emit_x64(p,id);
#else
    return {};
#endif
}
struct Stats{uint64_t compiled=0,rejected=0,executed=0,ops=0,budget_fallbacks=0,permission_failures=0,stale=0,stale_reinstalls=0;};
// 0.2.113: what the last caught stale leaf call looked like (read once by the VM glue for its JITSTALE line).
struct StaleLeaf{bool hit=false;uint32_t expected=0,got=0;bool reinstalled=false;};
class Engine {
    friend struct EngineTestAccess;   // 0.2.114: unit-test access only (declaration, no runtime effect)
    struct Entry{Executable code;unsigned args=0,max_ops=0;Program program;};
    std::vector<Entry> entries_;uint64_t epoch_=1;size_t bytes_=0;
public:
    static constexpr size_t BudgetBytes=jit_budget::LeafBytes,MaxEntries=jit_budget::LeafEntries;
    static constexpr unsigned HotThreshold=32;
    bool enabled=false,available=false,self_test=false,admission_closed=false;Stats stats{};std::string reason="not_requested";int result_fault=0;int guard_fault=0;StaleLeaf last_stale{};   // guard_fault: 0.2.114 desktop-only fault (J2ME_JIT_FAULT=leafguard), never set on Switch
    void clear(){entries_.clear();bytes_=0;++epoch_;if(!epoch_)++epoch_;stats={};admission_closed=false;}
    size_t bytes()const{return bytes_;}size_t entries()const{return entries_.size();}
    uint64_t epoch()const{return epoch_;}
    bool configure(bool requested){
        clear();enabled=requested;available=false;self_test=false;reason=requested?"pending":"not_requested";if(!requested)return false;
        const uint8_t b[]={26,16,17,130,172};std::string d="(I)I";Spec s{b,sizeof(b),&d,8,1,2,false,false,nullptr};Program p;
        if(!verify(s,p)){enabled=false;reason="jit236_verify_failed";return false;}Executable test;
        // 0.2.113: the self-test runs guarded code from a dedicated block whose pages are never reused for game code.
        const uint32_t tid=next_install_id();
        if(!test.install(emit_host(p,tid),tid,InstallMode::Dedicated)){++stats.permission_failures;enabled=false;reason=std::string("jit236_install_failed:")+Executable::last_error();return false;}
        for(uint32_t n:{0u,1u,0xffffffffu,0x80000000u,0x12345678u}){
            int32_t input[StaleIdSlot+1]{};input[0]=int32_t(n);input[StaleIdSlot]=int32_t(tid);uint64_t r=test.fn(input);if(result_fault)r^=1u;
            if(uint32_t(r)!=(n^17u)||uint32_t(r>>32)!=4){enabled=false;reason="jit236_result_mismatch";return false;}
        }
        {   // 0.2.114: leaf install-id guard. Another id must bail with the stale mark and the code's own id, running nothing.
            int32_t input[StaleIdSlot+1]{};input[0]=5;input[StaleIdSlot]=int32_t(tid^0x5a5a5a5au);uint64_t r=test.fn(input);
#ifndef __SWITCH__
            if(guard_fault)r=0;
#endif
            if(!stale_result(r)||uint32_t(r)!=tid){enabled=false;reason="jit236_guard_mismatch";return false;}
        }
        available=self_test=true;reason="ok";return true;
    }
    bool prepare(Site& site,const Spec& s){
        if(!enabled||!available)return false;
        if(site.epoch!=epoch_){site={};site.epoch=epoch_;}
        if(site.entry==-2)return false;if(site.entry>=0)return true;
        if(admission_closed){site.entry=-2;++stats.budget_fallbacks;return false;}
        if(site.calls<HotThreshold){++site.calls;return false;}
        Program p;if(!verify(s,p)){site.entry=-2;++stats.rejected;return false;}
        const uint32_t id=next_install_id();auto bytes=emit_host(p,id);size_t pages=(bytes.size()+4095)&~size_t(4095);
        if(entries_.size()>=MaxEntries||pages>BudgetBytes-bytes_){site.entry=-2;++stats.budget_fallbacks;return false;}
        Entry e;e.args=p.args;e.max_ops=p.max_ops;
        if(!e.code.install(bytes,id)){site.entry=-2;admission_closed=true;++stats.permission_failures;return false;}
        if(e.code.size>BudgetBytes-bytes_){site.entry=-2;++stats.budget_fallbacks;return false;}
        e.program=p;bytes_+=e.code.size;site.entry=int(entries_.size());entries_.push_back(std::move(e));++stats.compiled;return true;
    }
    bool call(const Site& site,const int32_t* args,unsigned count,unsigned budget,int32_t& result,unsigned& ops){
        if(!enabled||!available||site.epoch!=epoch_||site.entry<0||size_t(site.entry)>=entries_.size())return false;
        auto& e=entries_[size_t(site.entry)];if(!e.code.executable())return false;if(count!=e.args||budget<e.max_ops){++stats.budget_fallbacks;return false;}
        int32_t guarded[StaleIdSlot+1]{};for(unsigned i=0;i<count&&i<StaleIdSlot;++i)guarded[i]=args[i];guarded[StaleIdSlot]=int32_t(e.code.id);
        uint64_t value;{CodeCall active;value=e.code.fn(guarded);}
        if(stale_result(value)){
            // 0.2.113: the code at this address is not the code installed for this site. Nothing ran (the guard is the
            // first instruction); the interpreter executes this call and the site gets a fresh copy on a never-used page.
            ++stats.stale;last_stale={true,e.code.id,uint32_t(value),false};
            const uint32_t nid=next_install_id();const size_t old=e.code.size;
            if(e.code.install(emit_host(e.program,nid),nid,InstallMode::Fresh)){bytes_=bytes_-old+e.code.size;++stats.stale_reinstalls;last_stale.reinstalled=true;}
            else if(!e.code.executable())bytes_-=old;   // 0.2.114: only closed code leaves the budget (install refused during a compiled call keeps the old code open and counted; it is checked again on the next call)
            return false;
        }
        result=int32_t(uint32_t(value));ops=unsigned(value>>32);
        ++stats.executed;stats.ops+=ops;return true;
    }
};
} // namespace jit236
// P6.1 measurement hooks (no-op unless -DJ2ME_P61_PROF)
#include "p61_prof.h"
