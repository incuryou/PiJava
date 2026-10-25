#ifndef J2ME_TEST_JIT_HEADER
#define J2ME_TEST_JIT_HEADER "../src/native_jit236.h"
#endif
#include J2ME_TEST_JIT_HEADER
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
    stub_limit=argc>1?unsigned(std::atoi(argv[1])):1024;
    std::string desc="(I)I";const uint8_t code[]={26,4,96,172};jit236::Program p;jit236::Spec s{code,sizeof(code),&desc,8,1,2,false,false,nullptr};assert(jit236::verify(s,p));auto bytes=jit236::emit_host(p);
    std::vector<jit236::Executable> funcs(1800);unsigned installed=0;
    for(auto& f:funcs){if(!f.install(bytes))break;++installed;}
    int32_t input=41;for(unsigned i=0;i<installed;++i)assert(uint32_t(funcs[i].fn(&input))==42);
    std::cout<<"installed="<<installed<<" requested=1800 handle_limit="<<stub_limit<<" peak_handles="<<stub_peak<<" last_error="<<jit236::Executable::last_error()<<"\n";
#ifdef J2ME_TEST_BASELINE
    assert(installed==stub_limit);
#else
    assert(installed==1800 && stub_peak==113);
    // A permission transition failure poisons the shared block. Its peers
    // must report non-executable so VM dispatch falls back before any jump.
    stub_fail_exec=true;jit236::Executable failed;assert(!failed.install(bytes));
    assert(!funcs.back().executable());
#endif
    funcs.clear();
#ifndef J2ME_TEST_BASELINE
    // 0.2.113: freed pages are quarantined, which keeps their blocks (handles) alive until they age out.
    while(!jit236::code_quarantine().empty())jit236::release_oldest_quarantine();
#endif
    assert(!stub_handles);
#ifndef J2ME_TEST_BASELINE
    assert(stub_dcache_flushes>=installed && stub_icache_invalidates>=installed && jit236::jit_type_name()==std::string("code_memory"));
    jit236::Executable one;assert(one.install(bytes));stub_fail_write=true;
    assert(!failed.install(bytes) && !one.executable());one.close();
    while(!jit236::code_quarantine().empty())jit236::release_oldest_quarantine();assert(!stub_handles);
    assert(one.install(bytes));assert(uint32_t(one.fn(&input))==42);one.close();
    while(!jit236::code_quarantine().empty())jit236::release_oldest_quarantine();assert(!stub_handles);
#endif
}
