#pragma once
#include <cstdint>
#include <limits>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <time.h>
#endif
// c5: the function-local static below is platform-owned. Inside the VM translation unit (vm_migrate.h seen,
// VMStorage present) it lives in VMStorage::platform; standalone users keep the plain static.
#if defined(__SWITCH__) && defined(J2ME_VM_MIGRATE_H)
VMS_PLAT_ACCESSOR(cpu_clock_info_type, int64_t)
#define J2ME_CPU_CLOCK_LOCAL(local, slot, ...) VMS_PLAT_LOCAL(local, slot, __VA_ARGS__)
#else
#define J2ME_CPU_CLOCK_LOCAL(local, slot, ...) __VA_ARGS__
#endif

namespace j2me_cpu_clock {
// CPU time of the calling OS thread, NOT elapsed wall time or device load.
// A failed query remains explicitly unavailable (-1); never substitute wall time.
inline int64_t now_ns() {
#ifdef __SWITCH__
    J2ME_CPU_CLOCK_LOCAL(info_type, cpu_clock_info_type, static int64_t info_type = -1;) // Legacy ID 0xF0000002 must not look negative.
    if (info_type == -2) return -1;
    u64 ticks = 0;
    if (info_type < 0) {
        if (R_SUCCEEDED(svcGetInfo(&ticks, InfoType_ThreadTickCount,
                                  CUR_THREAD_HANDLE, TickCountInfo_Total)))
            info_type = InfoType_ThreadTickCount;
        else if (R_SUCCEEDED(svcGetInfo(&ticks, InfoType_ThreadTickCountDeprecated,
                                       CUR_THREAD_HANDLE, TickCountInfo_Total)))
            info_type = InfoType_ThreadTickCountDeprecated;
        else { info_type = -2; return -1; }
    } else if (R_FAILED(svcGetInfo(&ticks, u32(info_type), CUR_THREAD_HANDLE,
                                  TickCountInfo_Total))) return -1;
    // Quotient/remainder conversion avoids overflow for long-running devices.
    constexpr uint64_t hz = 19200000ULL;
    const uint64_t sec = ticks / hz;
    if (sec > uint64_t(INT64_MAX) / 1000000000ULL) return -1;
    const uint64_t whole=sec*1000000000ULL, tail=(ticks%hz)*1000000000ULL/hz;
    if(tail>uint64_t(INT64_MAX)-whole)return -1;
    return int64_t(whole+tail);
#else
#ifdef CLOCK_THREAD_CPUTIME_ID
    timespec ts{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0 || ts.tv_sec < 0) return -1;
    if (uint64_t(ts.tv_sec) > uint64_t(INT64_MAX) / 1000000000ULL) return -1;
    const int64_t whole=int64_t(ts.tv_sec)*1000000000LL;
    if(ts.tv_nsec<0 || ts.tv_nsec>=1000000000L || ts.tv_nsec>INT64_MAX-whole)return -1;
    return whole+ts.tv_nsec;
#else
    return -1;
#endif
#endif
}
inline int64_t difference(int64_t begin, int64_t end) {
    return begin >= 0 && end >= begin ? end - begin : -1;
}
inline const char* source() {
#ifdef __SWITCH__
    return "svcGetInfo-thread-ticks";
#else
    return "CLOCK_THREAD_CPUTIME_ID";
#endif
}
} // namespace j2me_cpu_clock
