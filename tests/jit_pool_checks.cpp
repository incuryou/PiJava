#include "../src/native_jit236.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace jit236;
int main(){
    auto baseline=code_pool_stats().blocks;
    std::string desc="(I)I";const uint8_t code[]={26,4,96,172};Program program;Spec spec{code,sizeof(code),&desc,8,1,2,false,false,nullptr};assert(verify(spec,program));auto bytes=emit_host(program);
    std::vector<Executable> compiled(1800);
    for(auto& e:compiled)assert(e.install(bytes));
    assert(code_pool_stats().slices==1800 && code_pool_stats().blocks-baseline==113);
    int32_t argument=41;
    for(auto& e:compiled){CodeCall call;assert(uint32_t(e.fn(&argument))==42);}
    // Free holes and refill them while the other immutable functions survive. 0.2.113: freed pages wait in the
    // quarantine (CodeQuarantinePages, FIFO), so the refill never lands on the most recently freed 512 pages; it uses
    // the never-written tail of the last block, the 388 pages that aged out, then 32 new blocks.
    std::set<void*> quarantined;std::vector<void*> freed;
    for(size_t i=0;i<compiled.size();i+=2){freed.push_back(reinterpret_cast<void*>(compiled[i].fn));compiled[i].close();}
    for(size_t i=freed.size()-CodeQuarantinePages;i<freed.size();++i)quarantined.insert(freed[i]);
    assert(code_pool_stats().quarantined_pages==CodeQuarantinePages && code_pool_stats().quarantine_releases==freed.size()-CodeQuarantinePages);
    for(size_t i=0;i<compiled.size();i+=2){assert(compiled[i].install(bytes));assert(!quarantined.count(reinterpret_cast<void*>(compiled[i].fn)));}
    assert(code_pool_stats().blocks-baseline==113+32);
    for(auto& e:compiled){CodeCall call;assert(uint32_t(e.fn(&argument))==42);}
    {CodeCall active;assert(!compiled[0].install(bytes));assert(compiled[0].executable());}
    compiled.clear();assert(!code_pool_stats().slices && !code_pool_stats().used_bytes);
    assert(code_pool_stats().blocks-baseline<=code_pool_stats().quarantined_pages);   // only quarantine-pinned blocks remain
    while(!code_quarantine().empty())release_oldest_quarantine();
    assert(code_pool_stats().blocks==baseline && !code_pool_stats().quarantined_pages);
    // Move/replace releases exactly one slice and never invalidates its peers.
    Executable a,b;assert(a.install(bytes)&&b.install(bytes));auto address=b.fn;a=std::move(b);assert(!b.fn && a.fn==address);a.close();
    assert(!code_pool_stats().slices);
    while(!code_quarantine().empty())release_oldest_quarantine();
    assert(code_pool_stats().blocks==baseline);
    std::cout<<"PASS pooled W^X: 1800 functions / 113 blocks, quarantined reuse, peer execution, active-call guard, move and reset\n";
}
