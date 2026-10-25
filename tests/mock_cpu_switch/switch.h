#pragma once
#include <cstdint>
using u64=uint64_t;using u32=uint32_t;using Handle=u32;using Result=u32;
constexpr u32 CUR_THREAD_HANDLE=0xFFFF8000;
constexpr u32 InfoType_ThreadTickCount=25,InfoType_ThreadTickCountDeprecated=0xF0000002;
constexpr u64 TickCountInfo_Total=UINT64_MAX;
#define R_SUCCEEDED(r) ((r)==0)
#define R_FAILED(r) ((r)!=0)
Result svcGetInfo(u64*,u32,Handle,u64);
