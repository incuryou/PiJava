#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Host storage only. Only unreachable primitive arrays donate buffers during
// the serial GC sweep; no Java handles, roots, or snapshot fields live here.
class PrimitiveBufferPool {
public:
    static constexpr size_t MaxBuffer=4096, Slots=512, Budget=256*1024;
    uint64_t hits=0,misses=0,returns=0,discarded=0;
    size_t peak_bytes=0;
private:
    std::array<std::vector<uint8_t>,Slots> buffers_{};
    std::array<int16_t,Slots> next_{};
    std::array<int16_t,MaxBuffer> heads_{};
    int free_head_=0;
    std::array<uint64_t,MaxBuffer/64> bins_{};
    uint64_t words_=0;
    size_t retained_=0;
    void reset_index(){
        heads_.fill(-1);for(size_t i=0;i<Slots;++i)next_[i]=i+1<Slots?int(i+1):-1;
        free_head_=0;bins_.fill(0);words_=0;retained_=0;
    }
public:
    PrimitiveBufferPool(){reset_index();}
    PrimitiveBufferPool(const PrimitiveBufferPool&)=delete;
    PrimitiveBufferPool& operator=(const PrimitiveBufferPool&)=delete;
    // Caller supplies an empty replacement vector. resize() afterwards must
    // value-initialize every live byte, including data left by a prior owner.
    void acquire(std::vector<uint8_t>& out,size_t needed){
        if(!needed)return;
        if(needed<=MaxBuffer && words_) {
            const size_t bucket=needed-1,first_word=bucket/64;
            uint64_t available=bins_[first_word] & (UINT64_MAX<<(bucket%64));
            size_t word=first_word;
            if(!available){
                // The summary skips empty words in O(1), including the word63
                // boundary without ever shifting a 64-bit value by 64.
                const uint64_t later=first_word==63?0:words_&(UINT64_MAX<<(first_word+1));
                if(later){word=unsigned(__builtin_ctzll(later));available=bins_[word];}
            }
            if(available){
                const size_t b=word*64+unsigned(__builtin_ctzll(available));const int i=heads_[b];
                heads_[b]=next_[i];if(heads_[b]<0){bins_[word]&=~(uint64_t(1)<<(b%64));if(!bins_[word])words_&=~(uint64_t(1)<<word);}
                retained_-=buffers_[i].capacity();out.swap(buffers_[i]);
                next_[i]=int16_t(free_head_);free_head_=i;++hits;return;
            }
        }
        // Exact reserves preserve the old cold/burst allocation size. A cache
        // miss must not add padding to thousands of still-live Java arrays.
        ++misses;out.reserve(needed);
    }
    void release(std::vector<uint8_t>& buffer){
        const size_t n=buffer.capacity();
        if(!n)return;
        if(n>MaxBuffer||free_head_<0||n>Budget-retained_){++discarded;return;}
        const int i=free_head_;free_head_=next_[i];const size_t b=n-1;
        buffer.clear();buffers_[i].swap(buffer);next_[i]=heads_[b];heads_[b]=i;bins_[b/64]|=uint64_t(1)<<(b%64);words_|=uint64_t(1)<<(b/64);
        retained_+=n;if(retained_>peak_bytes)peak_bytes=retained_;++returns;
    }
    size_t retained_bytes()const{return retained_;}
    static constexpr size_t byte_budget(){return Budget;}
    void clear(){for(auto& b:buffers_)std::vector<uint8_t>().swap(b);reset_index();hits=misses=returns=discarded=0;peak_bytes=0;}
};
