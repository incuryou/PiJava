#include "../src/optimization_config.h"
#include "../src/runtime/value.h"
#include "../src/array_storage.h"
#include "../src/runtime/library_index.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace j2me_runtime;
int main(){
    std::mt19937_64 rng(0x235cafe);
    for(int i=0;i<200000;i++){
        const int32_t n=int32_t(rng());const int32_t ref=int32_t(rng()&Value::max_reference);const int64_t wide=int64_t(rng());
        auto a=Value::of_i(n),b=Value::of_ref(ref),c=Value::of_l(wide);
        assert(a.i==n && !a.ref && !a.is_long);assert(b.ref==ref && !b.i && !b.is_long);assert(c.l==wide && !c.ref && c.is_long);
        Value d;std::memcpy(&d,&c,sizeof d);assert(d.l==wide && d.is_long && !d.ref);
    }
    assert(Value::of_ref(-1).ref==-1);assert(Value::of_l(INT64_MIN).l==INT64_MIN);
    ArrayStorage a,b;assert(a.reset("[Ljava/lang/Object;",512));assert(b.reset("[Ljava/lang/Object;",512));
    auto v=a.reference_version();a.set(3,Value::of_ref(17));assert(a.reference_version()==v+1&&a.last_reference_write()==3);
    assert(b.copy_from(a,0,0,512)&&b.get(3).ref==17&&b.last_reference_write()==SIZE_MAX);
    v=b.reference_version();assert(!b.copy_from(a,510,0,5)&&b.reference_version()==v);
    LibraryIndex index;std::vector<ScalarKey> keys;uint64_t version=1,epoch=0;size_t last=SIZE_MAX;
    auto read=[&](size_t i,ScalarKey& k){k=keys.at(i);return k.kind!=0;};
    auto query=[&](ScalarKey k){return index.find(7,keys.size(),version,last,epoch,k,read);};
    for(int i=0;i<150000;i++){
        unsigned op=rng()%8;
        if(op==0&&keys.size()<300){keys.push_back({rng()%201,1+uint32_t(rng()%6)});++version;last=keys.size()-1;}
        else if(op==1&&!keys.empty()){keys.erase(keys.begin()+rng()%keys.size());++version;last=SIZE_MAX;}
        else if(op==2&&!keys.empty()){size_t p=rng()%keys.size();keys[p]={rng()%201,uint32_t(rng()%7)};++version;last=p;}
        else if(op==3&&!keys.empty()){keys[rng()%keys.size()].value=rng()%201;++epoch;}
        else if(op==4&&i%31==0){keys.clear();++version;last=SIZE_MAX;}
        ScalarKey key{rng()%201,1+uint32_t(rng()%6)};int expected=-1;bool safe=true;
        for(size_t j=0;j<keys.size();j++){if(!keys[j].kind)safe=false;if(expected<0&&keys[j]==key)expected=int(j);}
        const int got=query(key);assert(got==(safe?expected:LibraryIndex::fallback));
        assert(index.retained_bytes()<=index.byte_budget());
    }
    keys.assign(16384,ScalarKey{});for(size_t i=0;i<keys.size();++i)keys[i]={uint64_t(i),1};++version;
    assert(query({16383,1})==16383);assert(query({16383,2})==-1);
    // Budget and table-count eviction; all slots are cold/rebuilt, never stale.
    for(int id=1;id<100;id++){int got=index.find(id,keys.size(),version,SIZE_MAX,epoch,{111,1},read);assert(got==111);assert(index.retained_bytes()<=index.byte_budget());}
    index.sweep([](int){return false;});assert(index.retained_bytes()==0);
    keys[111]={77777,1};++version;assert(query({111,1})==-1);assert(query({77777,1})==111);
    keys.push_back({77778,1});++version;assert(query({77778,1})==LibraryIndex::fallback);
    keys={{12,1},{12,1}};++version;assert(query({12,1})==0);
    keys={{uint64_t(INT64_MIN),3},{uint64_t(INT64_MAX),3}};++version;assert(query({uint64_t(INT64_MIN),3})==0);
    index.clear();assert(index.retained_bytes()==0);
    std::cout<<"PASS 235 core: 200000 tagged values, 150000 index differential operations, revisions/copy/eviction/weak sweep/bounds; slot_bytes="<<sizeof(Value)<<"\n";
}
