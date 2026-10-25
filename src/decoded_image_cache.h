#pragma once
#include <cstdint>
#include <cstddef>
#include <iterator>
#include <list>
#include <unordered_map>
#include <vector>
#include <algorithm>

// Only immutable JAR resource byte-vector identities are keys. The VM clears
// this cache on stop/reload, BEFORE those identities can be reused. It contains
// no Java references. Every hit copies into a NEW Image object, preserving
// object identity and avoiding mutable backing-store aliases.
class DecodedImageCache {
    struct Entry {
        const void* key=nullptr;
        int width=0,height=0;
        std::vector<uint8_t> rgb,alpha;
        size_t bytes()const{return rgb.capacity()+alpha.capacity();}
    };
    std::list<Entry> lru_;
    std::unordered_map<const void*,std::list<Entry>::iterator> index_;
    size_t retained_=0,budget_,limit_;
    void evict() {
        auto it=std::prev(lru_.end());retained_-=it->bytes();
        index_.erase(it->key);lru_.erase(it);++evictions;
    }
public:
    uint64_t hits=0,misses=0,inserts=0,evictions=0,bypasses=0;
    size_t peak_bytes=0;
    explicit DecodedImageCache(size_t budget=8u*1024u*1024u,size_t entries=256):budget_(budget),limit_(entries){}
    DecodedImageCache(const DecodedImageCache&)=delete;
    DecodedImageCache& operator=(const DecodedImageCache&)=delete;
    size_t retained_bytes()const{return retained_;}
    size_t byte_budget()const{return budget_;}
    size_t entries()const{return lru_.size();}
    void reset_counters(){hits=misses=inserts=evictions=bypasses=0;}
    void clear(){index_.clear();lru_.clear();retained_=peak_bytes=0;reset_counters();}
    bool get(const void* key,std::vector<uint8_t>& rgb,std::vector<uint8_t>& alpha,int& w,int& h) {
        auto it=index_.find(key);
        if(it==index_.end()){++misses;return false;}
        lru_.splice(lru_.begin(),lru_,it->second);
        const auto& e=*it->second;rgb=e.rgb;alpha=e.alpha;w=e.width;h=e.height;++hits;return true;
    }
    bool put(const void* key,const std::vector<uint8_t>& rgb,const std::vector<uint8_t>& alpha,int w,int h) {
        if(!key || w<=0 || h<=0 || uint64_t(w)*uint64_t(h)*3!=rgb.size() ||
           (!alpha.empty() && uint64_t(w)*uint64_t(h)!=alpha.size())){++bypasses;return false;}
        if(index_.count(key))return true;
        const bool solid=std::all_of(alpha.begin(),alpha.end(),[](uint8_t a){return a==255;});
        if(!limit_ || rgb.size()>budget_ || (!solid && alpha.size()>budget_-rgb.size())){++bypasses;return false;}
        const size_t requested=rgb.size()+(solid?0:alpha.size());
        while(!lru_.empty() && (retained_>budget_-requested || lru_.size()>=limit_))evict();
        Entry e;e.key=key;e.width=w;e.height=h;e.rgb=rgb;if(!solid)e.alpha=alpha;
        const size_t bytes=e.bytes();
        if(bytes>budget_){++bypasses;return false;}
        while(!lru_.empty() && retained_>budget_-bytes)evict();
        lru_.push_front(std::move(e));index_[key]=lru_.begin();retained_+=bytes;
        peak_bytes=std::max(peak_bytes,retained_);++inserts;return true;
    }
};
