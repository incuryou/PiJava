#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include "optimization_config.h"

// Storage only: empty cached vectors are never Java objects/GC roots/snapshots.
// Power-of-two capacity bins and bitmaps avoid linear scans on every invocation.
// Metadata is fixed-size; both retained bytes and buffer count have hard bounds.
template<class T, size_t Slots = 32, size_t Budget = 256 * 1024>
class FrameBufferPool {
    static_assert(Slots > 0 && Slots <= 64, "fixed slot bitmap supports 1..64 slots");
    std::array<std::vector<T>, Slots> buffers_{};
    std::array<int, Slots> next_{};
    std::array<int, 64> heads_{};
    uint64_t bins_ = 0, free_ = all_slots();
    size_t retained_ = 0;
    static constexpr uint64_t all_slots() { return UINT64_MAX >> (64-Slots); }
    static unsigned first(uint64_t mask) { return unsigned(__builtin_ctzll(mask)); }
    static unsigned floor_bin(size_t n) { return 63u-unsigned(__builtin_clzll(uint64_t(n))); }
    void reset_index() { heads_.fill(-1); next_.fill(-1); bins_=0; free_=all_slots(); }
public:
    uint64_t hits = 0, misses = 0, returns = 0, discarded = 0;
    size_t peak_bytes = 0;
    FrameBufferPool() { reset_index(); }
    FrameBufferPool(const FrameBufferPool&) = delete;
    FrameBufferPool& operator=(const FrameBufferPool&) = delete;
    void acquire(std::vector<T>& output, size_t needed) {
        output.clear();
        if (output.capacity() >= needed) return;
#if J2ME_OPT_FRAME_POOL
        // Return a too-small caller buffer before consuming a cached slot.
        if(output.capacity())release(output);
        const unsigned bin=needed<=1?0:floor_bin(needed-1)+1;
        const uint64_t available=bin<64 ? bins_ & (UINT64_MAX<<bin) : 0;
        if(available) {
            const unsigned bucket=first(available);const int index=heads_[bucket];
            heads_[bucket]=next_[index];
            if(heads_[bucket]<0)bins_&=~(uint64_t(1)<<bucket);
            next_[index]=-1;free_|=uint64_t(1)<<index;
            retained_-=buffers_[index].capacity()*sizeof(T);
            output.swap(buffers_[index]);++hits;return;
        }
        // Round only cacheable allocations. Huge Java frames use normal vectors.
        if(bin<std::numeric_limits<size_t>::digits) {
            const size_t rounded=size_t(1)<<bin;
            if(rounded<=Budget/sizeof(T))needed=rounded;
        }
#endif
        output.reserve(needed);++misses;
    }
    void release(std::vector<T>& buffer) noexcept {
        buffer.clear();
#if J2ME_OPT_FRAME_POOL
        const size_t capacity=buffer.capacity();if(!capacity)return;
        // Division check avoids overflow in the hard budget test.
        if(free_ && capacity<=(Budget-retained_)/sizeof(T)) {
            const unsigned index=first(free_),bucket=floor_bin(capacity);
            free_&=~(uint64_t(1)<<index);next_[index]=heads_[bucket];heads_[bucket]=int(index);
            bins_|=uint64_t(1)<<bucket;
            buffers_[index].swap(buffer);retained_+=capacity*sizeof(T);++returns;
            if(retained_>peak_bytes)peak_bytes=retained_;return;
        }
        ++discarded;
#endif
        std::vector<T>().swap(buffer);
    }
    size_t retained_bytes() const { return retained_; }
    static constexpr size_t byte_budget() { return Budget; }
    void clear() noexcept {
        for(auto& b:buffers_)std::vector<T>().swap(b);
        retained_=0;reset_index();
    }
    void reset_counters() { hits=misses=returns=discarded=0;peak_bytes=retained_; }
};
