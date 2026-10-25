#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace j2me_runtime {
// A derived index, not an owning Java collection. Slots contain immutable scalar
// values and original dense-array positions. Original Java arrays remain the
// sole authority for contents/order, synchronization, enumeration and snapshots.
struct ScalarKey {
    uint64_t value = 0;
    uint32_t kind = 0;
    bool operator==(const ScalarKey& b) const { return value==b.value && kind==b.kind; }
};
class LibraryIndex {
    struct Slot { uint64_t value=0; uint32_t kind=0; uint32_t position=0; };
    struct Table {
        int array=0; size_t count=0; uint64_t version=0, key_epoch=0, used=0;
        bool valid=false, rejected=false;
        std::vector<Slot> slots;
    };
public:
    struct Stats { uint64_t lookups=0,hits=0,rebuilds=0,appended=0,fallbacks=0,probes=0; };
    static constexpr size_t max_tables=32, max_keys=16384, budget=2*1024*1024;
    static constexpr int fallback=-2;
    size_t retained_bytes() const { size_t n=0;for(const auto& t:tables_)n+=t.slots.capacity()*sizeof(Slot);return n; }
    size_t byte_budget() const { return budget; }
    Stats stats() const { return stats_; }
    void reset_stats() { stats_={}; }
    void clear() { for(auto& t:tables_)t=Table{};clock_=0; }
    template<class Live> void sweep(Live live) { for(auto& t:tables_)if(t.array&&!live(t.array))t=Table{}; }
    // Reader must return false for any unreviewed/custom/side-effectful key.
    // No partial native result may escape in that case: run original Java find.
    template<class Reader>
    int find(int array, size_t count, uint64_t version, size_t last_write,
             uint64_t key_epoch, const ScalarKey& key, Reader read) {
        ++stats_.lookups;
        if(array<=0||!key.kind||count>max_keys){++stats_.fallbacks;return fallback;}
        if(!count)return -1;
        Table* t=nullptr;
        for(auto& item:tables_)if(item.array==array){t=&item;break;}
        if(!t){
            t=&*std::min_element(tables_.begin(),tables_.end(),[](const Table& a,const Table& b){return a.used<b.used;});
            *t=Table{};t->array=array;
        }
        t->used=++clock_;
        const bool fresh=t->version==version && t->key_epoch==key_epoch && t->count==count;
        if(fresh&&t->rejected){++stats_.fallbacks;return fallback;}
        if(!fresh||!t->valid){
            const bool append=t->valid && t->key_epoch==key_epoch && count==t->count+1 &&
                version==t->version+1 && last_write==t->count && count*2<=t->slots.size();
            if(append){
                ScalarKey k;
                if(!read(count-1,k)||!k.kind){reject(*t,count,version,key_epoch);++stats_.fallbacks;return fallback;}
                insert(*t,k,uint32_t(count-1));++stats_.appended;
            }else{
                size_t capacity=2;while(capacity<count*2)capacity*=2;
                // Account vector capacity, not size; evict before allocation.
                *t=Table{t->array,0,0,0,t->used,false,false,{}};
                while(retained_bytes()+capacity*sizeof(Slot)>budget){
                    Table* victim=nullptr;
                    for(auto& v:tables_)if(&v!=t&&!v.slots.empty()&&(!victim||v.used<victim->used))victim=&v;
                    if(!victim){++stats_.fallbacks;return fallback;}
                    *victim=Table{};
                }
                t->slots.resize(capacity);
                for(size_t i=0;i<count;++i){
                    ScalarKey k;
                    if(!read(i,k)||!k.kind){reject(*t,count,version,key_epoch);++stats_.fallbacks;return fallback;}
                    insert(*t,k,uint32_t(i));
                }
                ++stats_.rebuilds;
            }
            t->version=version;t->key_epoch=key_epoch;t->count=count;t->valid=true;t->rejected=false;
        }else ++stats_.hits;
        const size_t mask=t->slots.size()-1;size_t at=hash(key)&mask;
        for(size_t probes=0;probes<t->slots.size();++probes,at=(at+1)&mask){
            ++stats_.probes;const auto& slot=t->slots[at];
            if(!slot.kind)return -1;
            if(slot.kind==key.kind&&slot.value==key.value)return int(slot.position);
        }
        ++stats_.fallbacks;return fallback; // Cannot happen at <=50% load; fail closed.
    }
private:
    static uint64_t hash(const ScalarKey& key){
        uint64_t x=key.value^(uint64_t(key.kind)*0x9e3779b97f4a7c15ULL);
        x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);
    }
    static void insert(Table& t,const ScalarKey& key,uint32_t index){
        const size_t mask=t.slots.size()-1;size_t at=hash(key)&mask;
        for(size_t n=0;n<t.slots.size();++n,at=(at+1)&mask){
            auto& s=t.slots[at];
            if(!s.kind){s={key.value,key.kind,index};return;}
            // Dense legacy semantics choose the FIRST equal key even if malformed
            // input arrays contain duplicates. Never overwrite that position.
            if(s.kind==key.kind&&s.value==key.value)return;
        }
    }
    static void reject(Table& t,size_t count,uint64_t version,uint64_t epoch){
        std::vector<Slot>().swap(t.slots);t.count=count;t.version=version;t.key_epoch=epoch;t.valid=false;t.rejected=true;
    }
    std::array<Table,max_tables> tables_{};
    uint64_t clock_=0;
    Stats stats_{};
};
} // namespace j2me_runtime
