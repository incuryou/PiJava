#pragma once
#include "jit_budget.h"
#include <algorithm>
#include <array>
#include <chrono>
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
struct CodePoolStats {size_t blocks=0,peak_blocks=0,slices=0,used_bytes=0,reserved_bytes=0,peak_reserved_bytes=0;
    // 0.2.113: page-reuse hygiene counters (reported on the JITREGION line)
    size_t quarantined_pages=0,quarantine_releases=0,fresh_installs=0,reused_installs=0,dedicated_blocks=0,stale_fault_skips=0;
    uint64_t create_calls=0,create_failures=0,create_wall_us=0,create_max_us=0;};
inline CodePoolStats& code_pool_stats(){static CodePoolStats s;return s;}
// Capacity opportunities, independent of diagnostics resets. Only retirement
// of a live ordinary slice advances this, never a failed allocation/reclaim.
inline uint64_t& retirement_sequence(){static uint64_t sequence=0;return sequence;}
// Fit23: amortize scarce native CodeMemory objects across 512 immutable pages.
// Self-tests retain their isolated smaller arena; individual slices, page
// granularity, quarantine length and the total byte budget are unchanged.
constexpr size_t CodePage=4096,CodeBlockBytes=2097152,CodeDedicatedBytes=262144,CodeMaxSliceBytes=262144;
constexpr unsigned CodeBlockPages=unsigned(CodeBlockBytes/CodePage);
constexpr unsigned CodeBlockWords=(CodeBlockPages+63)/64;
static_assert(CodeBlockPages==512 && CodeBlockBytes%CodePage==0 && CodeDedicatedBytes%CodePage==0,"invalid code arena geometry");
// Callers provide 1..64 pages and a range wholly inside an arena. Avoid a
// word-width shift when one allocation occupies all 64 pages.
constexpr uint64_t page_bits(unsigned pages){return pages==64?UINT64_MAX:((uint64_t(1)<<pages)-1);}
constexpr size_t CodePoolMaxBytes=jit_budget::RegionBytes+jit_budget::LeafBytes;
// Upper bound for ordinary mappings; actual admission charges each mapping's
// bytes, including dedicated arenas. The native object count may be smaller.
constexpr size_t CodePoolMaxBlocks=CodePoolMaxBytes/CodeBlockBytes;
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
    size_t bytes=0;
    std::array<uint64_t,CodeBlockWords> used{},written{};   // written: pages that ever held code
    unsigned page_count()const{return unsigned(bytes/CodePage);}
    static bool intersects(const std::array<uint64_t,CodeBlockWords>& bits,unsigned first,unsigned pages){
        while(pages){const unsigned bit=first%64,n=std::min(pages,64-bit);
            if(bits[first/64]&(page_bits(n)<<bit))return true;first+=n;pages-=n;}
        return false;
    }
    static void mark(std::array<uint64_t,CodeBlockWords>& bits,unsigned first,unsigned pages,bool value){
        while(pages){const unsigned bit=first%64,n=std::min(pages,64-bit);const uint64_t mask=page_bits(n)<<bit;
            if(value)bits[first/64]|=mask;else bits[first/64]&=~mask;first+=n;pages-=n;}
    }
    ~CodeBlock(){
        if(!allocated)return;
#ifdef __SWITCH__
        jitClose(&memory);
#elif defined(__unix__) || defined(__APPLE__)
        munmap(memory,bytes);
#endif
        --code_pool_stats().blocks;code_pool_stats().reserved_bytes-=bytes;
    }
    bool create(size_t capacity,const char*& error,uint32_t& result){
        const auto began=std::chrono::steady_clock::now();
        auto observed=[&](bool ok){auto& stats=code_pool_stats();const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count();const auto elapsed=uint64_t(us>0?us:0);++stats.create_calls;stats.create_failures+=!ok;stats.create_wall_us+=elapsed;stats.create_max_us=std::max(stats.create_max_us,elapsed);return ok;};
#ifdef __SWITCH__
        Result rc=jitCreate(&memory,capacity);
        if(R_FAILED(rc)){error="jitCreate_failed";result=rc;return observed(false);}
        jit_type_name()=memory.type==JitType_CodeMemory?"code_memory":memory.type==JitType_SetProcessMemoryPermission?"set_process_memory_permission":"other";
#elif defined(__unix__) || defined(__APPLE__)
        memory=mmap(nullptr,capacity,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(memory==MAP_FAILED){memory=nullptr;error="mmap_failed";return observed(false);}
#else
        error="unsupported_platform";return observed(false);
#endif
        bytes=capacity;allocated=true;auto& s=code_pool_stats();++s.blocks;if(s.blocks>s.peak_blocks)s.peak_blocks=s.blocks;
        s.reserved_bytes+=bytes;s.peak_reserved_bytes=std::max(s.peak_reserved_bytes,s.reserved_bytes);
        return observed(true);
    }
    int find(unsigned pages,bool fresh_only=false)const{
        if(!pages||pages>page_count())return -1;
        for(unsigned start=0;start+pages<=page_count();++start)
            if(!intersects(used,start,pages)&&(!fresh_only||!intersects(written,start,pages)))return int(start);
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
        // libnx's CodeMemory backend keeps separate RW/RX aliases mapped.
        // Its executable transition only sweeps both whole aliases and sets
        // this flag. Publish the written range explicitly instead; using the
        // full transition here would sweep 2 MiB for every tiny new function.
        const bool code_memory=memory.type==JitType_CodeMemory;
        if(code_memory)memory.is_executable=false;
        else {Result rc=jitTransitionToWritable(&memory);
            if(R_FAILED(rc)){error="jitTransitionToWritable_failed";result=rc;return false;}}
        auto* rw=static_cast<uint8_t*>(jitGetRwAddr(&memory))+offset;
        std::memcpy(rw,bytes.data(),bytes.size());
        armDCacheFlush(rw,bytes.size());   // 0.2.113: explicit clean of the written range while the RW view is mapped
        if(!code_memory){Result rc=jitTransitionToExecutable(&memory);
            if(R_FAILED(rc)){error="jitTransitionToExecutable_failed";result=rc;return false;}}
        armICacheInvalidate(static_cast<uint8_t*>(jitGetRxAddr(&memory))+offset,bytes.size());   // 0.2.113: and of the RX range
        if(code_memory)memory.is_executable=true;
#elif defined(__unix__) || defined(__APPLE__)
        executable=false;
        if(mprotect(memory,this->bytes,PROT_READ|PROT_WRITE)){error="mprotect_rw_failed";return false;}
        auto* dst=static_cast<char*>(memory)+offset;
        const unsigned first=unsigned(offset/CodePage);const bool reused=intersects(written,first,1);
        if(fault_stale()&&reused)++code_pool_stats().stale_fault_skips;   // test-only: keep the previous code
        else std::memcpy(dst,bytes.data(),bytes.size());
        __builtin___clear_cache(dst,dst+bytes.size());
        if(mprotect(memory,this->bytes,PROT_READ|PROT_EXEC)){error="mprotect_rx_failed";return false;}
#else
        return false;
#endif
        {const unsigned first=unsigned(offset/CodePage),n=unsigned((bytes.size()+CodePage-1)/CodePage);mark(written,first,n,true);}
        executable=true;error="none";result=0;return true;
    }
};
inline std::vector<std::weak_ptr<CodeBlock>>& code_blocks(){static std::vector<std::weak_ptr<CodeBlock>> blocks;return blocks;}
struct QuarantinedPages {std::shared_ptr<CodeBlock> block;unsigned page=0,pages=0;};
inline std::deque<QuarantinedPages>& code_quarantine(){static std::deque<QuarantinedPages> q;return q;}
inline void release_oldest_quarantine(){
    auto& q=code_quarantine();if(q.empty())return;auto& e=q.front();CodeBlock::mark(e.block->used,e.page,e.pages,false);
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
        if(!block->dedicated&&retirement_sequence()<UINT64_MAX)++retirement_sequence();
        if(block->dedicated){}   // self-test pages stay reserved for the whole process
        else if(fault_stale()){CodeBlock::mark(block->used,page,pages,false);}   // test-only: immediate reuse (0.2.112 behaviour)
        else {code_quarantine().push_back({block,page,pages});s.quarantined_pages+=pages;
            while(s.quarantined_pages>CodeQuarantinePages)release_oldest_quarantine();}
        block.reset();page=pages=0;
    }
    bool install(const std::vector<uint8_t>& bytes,const char*& error,uint32_t& result,InstallMode mode=InstallMode::Normal){
        if(active_code_calls()){error="install_during_compiled_call";return false;}
        close();
        if(bytes.empty()||bytes.size()>CodeMaxSliceBytes){error="empty_or_too_large";return false;}
        const unsigned need=unsigned((bytes.size()+CodePage-1)/CodePage);
        auto& blocks=code_blocks();int slot=-1;
        auto pick=[&](bool fresh_only)->bool{
            if(mode==InstallMode::Dedicated){for(auto& b:dedicated_blocks())if(b->executable&&(slot=b->find(need,true))>=0){block=b;return true;}return false;}
            for(auto it=blocks.begin();it!=blocks.end();){auto b=it->lock();if(!b){it=blocks.erase(it);continue;}
                if(b->executable&&(slot=b->find(need,fresh_only))>=0){block=std::move(b);return true;}++it;}
            return false;};
        bool create_failed=false;
        auto fresh_block=[&]()->bool{
            const size_t capacity=mode==InstallMode::Dedicated?CodeDedicatedBytes:CodeBlockBytes;
            if(capacity>CodePoolMaxBytes||code_pool_stats().reserved_bytes>CodePoolMaxBytes-capacity)return false;
            auto b=std::make_shared<CodeBlock>();if(!b->create(capacity,error,result)){create_failed=true;return false;}
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
            // Horizon may exhaust CodeMemory resources before our byte budget.
            // The compiled-call guard above excludes execution during recovery.
            if(!ok&&create_failed&&(mode!=InstallMode::Normal||result!=0xCE01u))return false;
            bool reclaimed=false;
            while(!ok&&!code_quarantine().empty()){
                release_oldest_quarantine();reclaimed=true;
                ok=pick(mode==InstallMode::Fresh); // Fresh never reuses written pages.
            }
            // Fully retired blocks may disappear before pick can use a slot.
            // Retry creation once, only after actual reclamation; never spin.
            if(!ok&&reclaimed)ok=fresh_block();
        }
        if(!ok){if(!create_failed)error="code_pool_budget_full";return false;}
        page=unsigned(slot);pages=need;
        const bool reused=CodeBlock::intersects(block->written,page,pages);
        CodeBlock::mark(block->used,page,pages,true);
        auto& s=code_pool_stats();++s.slices;s.used_bytes+=pages*CodePage;if(reused)++s.reused_installs;else ++s.fresh_installs;
        if(!block->write(page*CodePage,bytes,error,result)){close();return false;}
        return true;
    }
};
}
