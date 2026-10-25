#pragma once
#include "jit_budget.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

namespace jit236 {
// Java execution and code installation share the VM thread. Installation is
// forbidden while a compiled call is active; existing slots are immutable.
inline unsigned& active_code_calls(){static unsigned n=0;return n;}
struct CodeCall {CodeCall(){++active_code_calls();} ~CodeCall(){--active_code_calls();}};
struct CodePoolStats {size_t blocks=0,peak_blocks=0,slices=0,used_bytes=0;
    // 0.2.113: page-reuse hygiene counters (reported on the JITREGION line)
    size_t quarantined_pages=0,quarantine_releases=0,fresh_installs=0,reused_installs=0,dedicated_blocks=0,stale_fault_skips=0;};
inline CodePoolStats& code_pool_stats(){static CodePoolStats s;return s;}
constexpr size_t CodePage=4096,CodeBlockBytes=65536;
constexpr size_t CodePoolMaxBlocks=(jit_budget::RegionBytes+jit_budget::LeafBytes)/CodeBlockBytes;
// 0.2.113: freed slices wait in a FIFO quarantine before their pages may hold other code again, so an address
// is almost never re-used for different code soon after it ran (the install-id guard catches the rest).
constexpr size_t CodeQuarantinePages=512;   // 2 MiB of recently freed code pages
// 0.2.113: which libnx JIT implementation backs the pool (recorded at the first jitCreate; logged on JITSELFTEST).
inline const char*& jit_type_name(){static const char* n=
#ifdef __SWITCH__
    "unknown";
#else
    "desktop_mmap";
#endif
    return n;}
// 0.2.113: DESKTOP/TEST-ONLY fault switch J2ME_JIT_FAULT_STALE=1 (never compiled into the Switch NRO, off by default):
// freed pages are reused at once (no quarantine, lowest page first, as 0.2.112) and a reused page keeps its previous
// bytes, i.e. the CPU/emulator "keeps running the old code". Fresh-page installs are written normally.
#ifdef __SWITCH__
inline int fault_stale(){return 0;}   // the Switch build has no fault switch
#else
inline int& fault_stale(){static int f=0;return f;}
#endif
struct CodeBlock {
#ifdef __SWITCH__
    Jit memory{};
#else
    void* memory=nullptr;
#endif
    bool allocated=false,executable=false,dedicated=false;
    uint32_t used=0,written=0;   // written: pages that ever held code (0.2.113)
    ~CodeBlock(){
        if(!allocated)return;
#ifdef __SWITCH__
        jitClose(&memory);
#elif defined(__unix__) || defined(__APPLE__)
        munmap(memory,CodeBlockBytes);
#endif
        --code_pool_stats().blocks;
    }
    bool create(const char*& error,uint32_t& result){
#ifdef __SWITCH__
        Result rc=jitCreate(&memory,CodeBlockBytes);
        if(R_FAILED(rc)){error="jitCreate_failed";result=rc;return false;}
        jit_type_name()=memory.type==JitType_CodeMemory?"code_memory":memory.type==JitType_SetProcessMemoryPermission?"set_process_memory_permission":"other";
#elif defined(__unix__) || defined(__APPLE__)
        memory=mmap(nullptr,CodeBlockBytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(memory==MAP_FAILED){memory=nullptr;error="mmap_failed";return false;}
#else
        error="unsupported_platform";return false;
#endif
        allocated=true;auto& s=code_pool_stats();++s.blocks;if(s.blocks>s.peak_blocks)s.peak_blocks=s.blocks;
        return true;
    }
    int find(unsigned pages,bool fresh_only=false)const{
        const uint32_t busy=used|(fresh_only?written:0u);
        for(unsigned start=0;start+pages<=16;++start){const uint32_t mask=((1u<<pages)-1)<<start;if(!(busy&mask))return int(start);}
        return -1;
    }
    void* rx(size_t offset){
#ifdef __SWITCH__
        return static_cast<uint8_t*>(jitGetRxAddr(&memory))+offset;
#else
        return static_cast<uint8_t*>(memory)+offset;
#endif
    }
    bool write(size_t offset,const std::vector<uint8_t>& bytes,const char*& error,uint32_t& result){
#ifdef __SWITCH__
        executable=false;
        Result rc=jitTransitionToWritable(&memory);
        if(R_FAILED(rc)){error="jitTransitionToWritable_failed";result=rc;return false;}
        auto* rw=static_cast<uint8_t*>(jitGetRwAddr(&memory))+offset;
        std::memcpy(rw,bytes.data(),bytes.size());
        armDCacheFlush(rw,bytes.size());   // 0.2.113: explicit clean of the written range while the RW view is mapped
        rc=jitTransitionToExecutable(&memory);
        if(R_FAILED(rc)){error="jitTransitionToExecutable_failed";result=rc;return false;}
        armICacheInvalidate(static_cast<uint8_t*>(jitGetRxAddr(&memory))+offset,bytes.size());   // 0.2.113: and of the RX range
#elif defined(__unix__) || defined(__APPLE__)
        executable=false;
        if(mprotect(memory,CodeBlockBytes,PROT_READ|PROT_WRITE)){error="mprotect_rw_failed";return false;}
        auto* dst=static_cast<char*>(memory)+offset;
        const unsigned first=unsigned(offset/CodePage);const bool reused=(written>>first)&1u;
        if(fault_stale()&&reused)++code_pool_stats().stale_fault_skips;   // test-only: keep the previous code
        else std::memcpy(dst,bytes.data(),bytes.size());
        __builtin___clear_cache(dst,dst+bytes.size());
        if(mprotect(memory,CodeBlockBytes,PROT_READ|PROT_EXEC)){error="mprotect_rx_failed";return false;}
#else
        return false;
#endif
        {const unsigned first=unsigned(offset/CodePage),n=unsigned((bytes.size()+CodePage-1)/CodePage);written|=((1u<<n)-1)<<first;}
        executable=true;error="none";result=0;return true;
    }
};
inline std::vector<std::weak_ptr<CodeBlock>>& code_blocks(){static std::vector<std::weak_ptr<CodeBlock>> blocks;return blocks;}
struct QuarantinedPages {std::shared_ptr<CodeBlock> block;unsigned page=0,pages=0;};
inline std::deque<QuarantinedPages>& code_quarantine(){static std::deque<QuarantinedPages> q;return q;}
inline void release_oldest_quarantine(){
    auto& q=code_quarantine();if(q.empty())return;auto& e=q.front();e.block->used&=~(((1u<<e.pages)-1)<<e.page);
    auto& s=code_pool_stats();s.quarantined_pages-=e.pages;++s.quarantine_releases;q.pop_front();
}
// Self-test code lives in blocks of its own whose pages are never handed to game code (0.2.113).
inline std::vector<std::shared_ptr<CodeBlock>>& dedicated_blocks(){static std::vector<std::shared_ptr<CodeBlock>> b;return b;}
enum class InstallMode {Normal,Fresh,Dedicated};
class CodeSlice {
    std::shared_ptr<CodeBlock> block;unsigned page=0,pages=0;
public:
    CodeSlice()=default;CodeSlice(const CodeSlice&)=delete;CodeSlice& operator=(const CodeSlice&)=delete;
    ~CodeSlice(){close();}
    void swap(CodeSlice& other){block.swap(other.block);std::swap(page,other.page);std::swap(pages,other.pages);}
    bool executable()const{return block && block->executable;}
    void* address()const{return block?block->rx(page*CodePage):nullptr;}
    void close(){
        if(!block)return;auto& s=code_pool_stats();--s.slices;s.used_bytes-=pages*CodePage;
        if(block->dedicated){}   // self-test pages stay reserved for the whole process
        else if(fault_stale()){block->used&=~(((1u<<pages)-1)<<page);}   // test-only: immediate reuse (0.2.112 behaviour)
        else {code_quarantine().push_back({block,page,pages});s.quarantined_pages+=pages;
            while(s.quarantined_pages>CodeQuarantinePages)release_oldest_quarantine();}
        block.reset();page=pages=0;
    }
    bool install(const std::vector<uint8_t>& bytes,const char*& error,uint32_t& result,InstallMode mode=InstallMode::Normal){
        if(active_code_calls()){error="install_during_compiled_call";return false;}
        close();
        if(bytes.empty()||bytes.size()>CodeBlockBytes){error="empty_or_too_large";return false;}
        const unsigned need=unsigned((bytes.size()+CodePage-1)/CodePage);
        auto& blocks=code_blocks();int slot=-1;
        auto pick=[&](bool fresh_only)->bool{
            if(mode==InstallMode::Dedicated){for(auto& b:dedicated_blocks())if(b->executable&&(slot=b->find(need,true))>=0){block=b;return true;}return false;}
            for(auto it=blocks.begin();it!=blocks.end();){auto b=it->lock();if(!b){it=blocks.erase(it);continue;}
                if(b->executable&&(slot=b->find(need,fresh_only))>=0){block=std::move(b);return true;}++it;}
            return false;};
        bool create_failed=false;
        auto fresh_block=[&]()->bool{
            if(code_pool_stats().blocks>=CodePoolMaxBlocks)return false;
            auto b=std::make_shared<CodeBlock>();if(!b->create(error,result)){create_failed=true;return false;}
            if(mode==InstallMode::Dedicated){b->dedicated=true;dedicated_blocks().push_back(b);++code_pool_stats().dedicated_blocks;}
            else blocks.push_back(b);
            block=std::move(b);slot=0;return true;};
        bool ok=false;
        if(mode==InstallMode::Dedicated)ok=pick(true)||fresh_block();
        else if(fault_stale()&&mode==InstallMode::Normal)ok=pick(false)||fresh_block();   // test-only: 0.2.112 first-fit
        else {
            ok=pick(true);                                   // 1. never-used pages of an existing block
            if(!ok&&mode==InstallMode::Normal)ok=pick(false);    // 2. pages that already aged out of the quarantine
            if(!ok)ok=fresh_block();                         // 3. a new block
            if(!ok&&create_failed)return false;
            while(!ok&&!code_quarantine().empty()){release_oldest_quarantine();ok=pick(false);}   // 4. pool full: age out early
        }
        if(!ok){if(!create_failed)error="code_pool_budget_full";return false;}
        page=unsigned(slot);pages=need;
        const bool reused=(block->written>>page)&(((1u<<pages)-1));
        block->used|=((1u<<pages)-1)<<page;
        auto& s=code_pool_stats();++s.slices;s.used_bytes+=pages*CodePage;if(reused)++s.reused_installs;else ++s.fresh_installs;
        if(!block->write(page*CodePage,bytes,error,result)){close();return false;}
        return true;
    }
};
}
