// vm_rs_fallback.h — c6: render headers are also compiled stand-alone by unit tests that do not include the VM
// (no vm_migrate.h). Then every render storage macro means "the original declaration". No tokens of its own.
#pragma once
#ifndef VMS_RS
#define VMS_RS(n) n
#define VMS_RS_ACCESSOR(n, ...)
#define VMS_RS_NSREF(fn, name, ...) static __VA_ARGS__ name{};  // stand-alone: plain zero-initialised static (was the legacy declaration)
#define VMS_RS_LOCAL(local, slot, ...) __VA_ARGS__
#endif
