#pragma once
#include <cstdint>
#include <cstddef>
#include <sys/mman.h>
using Result=uint32_t;
#define R_FAILED(x) ((x)!=0)
typedef enum {JitType_SetProcessMemoryPermission,JitType_CodeMemory} JitType;   // 0.2.113: mirrors libnx kernel/jit.h
struct Jit{JitType type=JitType_CodeMemory;void* memory=nullptr;void* rx_addr=nullptr;void* rw_addr=nullptr;size_t size=0;};
inline unsigned stub_dcache_flushes=0,stub_icache_invalidates=0;
inline void armDCacheFlush(void*,size_t){++stub_dcache_flushes;}         // 0.2.113: libnx arm/cache.h
inline void armICacheInvalidate(void*,size_t){++stub_icache_invalidates;}
inline unsigned stub_handles=0,stub_limit=1024,stub_peak=0;
inline bool stub_fail_write=false,stub_fail_exec=false;
inline Result jitCreate(Jit* j,size_t size){if(stub_handles>=stub_limit)return 53761;void* p=mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(p==MAP_FAILED)return 53249;*j={JitType_CodeMemory,p,p,p,size};++stub_handles;if(stub_handles>stub_peak)stub_peak=stub_handles;return 0;}
inline Result jitTransitionToWritable(Jit* j){if(stub_fail_write){stub_fail_write=false;return 1;}return mprotect(j->memory,j->size,PROT_READ|PROT_WRITE)?1:0;}
inline Result jitTransitionToExecutable(Jit* j){if(stub_fail_exec){stub_fail_exec=false;return 1;}return mprotect(j->memory,j->size,PROT_READ|PROT_EXEC)?1:0;}
inline Result jitClose(Jit* j){if(j->memory){munmap(j->memory,j->size);--stub_handles;}*j={};return 0;}
inline void* jitGetRxAddr(const Jit* j){return j->rx_addr;}
inline void* jitGetRwAddr(const Jit* j){return j->rw_addr;}
