#pragma once
// Independent, compile-time rollback switches. JAR bytecode and Java timing
// policy are never rewritten by these runtime implementation optimizations.
#ifndef J2ME_OPT_FRAME_POOL
#define J2ME_OPT_FRAME_POOL 0 // Experimental: host call-heavy benchmark regressed; opt-in only.
#endif
#ifndef J2ME_OPT_GC_SCRATCH
#define J2ME_OPT_GC_SCRATCH 1
#endif
#ifndef J2ME_OPT_NATIVE_CLASS_CACHE
#define J2ME_OPT_NATIVE_CLASS_CACHE 1
#endif
#ifndef J2ME_OPT_GFX_STATE
#define J2ME_OPT_GFX_STATE 1
#endif
#ifndef J2ME_OPT_RGB_FILL
#define J2ME_OPT_RGB_FILL 1
#endif
#ifndef J2ME_OPT_AUDIO_POLL
#define J2ME_OPT_AUDIO_POLL 1
#endif
#ifndef J2ME_OPT_ZXX_HANDOFF
#define J2ME_OPT_ZXX_HANDOFF 1
#endif
#ifndef J2ME_OPT_POLL_MARGIN
#define J2ME_OPT_POLL_MARGIN 1
#endif
#ifndef J2ME_PROFILE_DEFAULT
#define J2ME_PROFILE_DEFAULT 0
#endif

#ifndef J2ME_OPT_FAST_DISPATCH
#define J2ME_OPT_FAST_DISPATCH 1
#endif

#ifndef J2ME_OPT_IMAGE_CACHE
#define J2ME_OPT_IMAGE_CACHE 1
#endif
#ifndef J2ME_OPT_FAST_PNG
#define J2ME_OPT_FAST_PNG 1
#endif
#ifndef J2ME_OPT_GATE_RECOVERY
#define J2ME_OPT_GATE_RECOVERY 1
#endif

#ifndef J2ME_OPT_ASCII_SCAN
#define J2ME_OPT_ASCII_SCAN 1
#endif

// 0.2.35: independent architectural features, all keep a reference fallback.
#ifndef J2ME_OPT_LIBRARY_INDEX
#define J2ME_OPT_LIBRARY_INDEX 1
#endif
#ifndef J2ME_OPT_COMPACT_VALUE
#define J2ME_OPT_COMPACT_VALUE 1
#endif
#include "vm_migrate.h"
