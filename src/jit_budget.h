#pragma once
#include <cstddef>
// NSP/application-mode target. Memory is committed only for admitted hot code.
// Overrides are for controlled 2/8/16 MiB comparisons, not game-specific rules.
#ifndef J2ME_JIT_REGION_MIB
#define J2ME_JIT_REGION_MIB 16
#endif
#ifndef J2ME_JIT_LEAF_MIB
#define J2ME_JIT_LEAF_MIB 4
#endif
namespace jit_budget {
constexpr size_t RegionBytes=size_t(J2ME_JIT_REGION_MIB)*1024*1024;
constexpr size_t LeafBytes=size_t(J2ME_JIT_LEAF_MIB)*1024*1024;
constexpr size_t RegionEntries=2048, LeafEntries=512;
static_assert(RegionBytes>=65536 && LeafBytes>=65536,"JIT budget too small");
}
