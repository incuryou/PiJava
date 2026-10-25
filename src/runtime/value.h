#pragma once
#include <cstdint>
#include <type_traits>
// JVM values are independent of the checkpoint wire format. The signed 31-bit
// handle still allows >1 billion objects; allocation enforces that explicit cap.
// No payload union: GC can continue to distinguish references from int bits.
struct Value {
    int32_t i = 0;
#if J2ME_OPT_COMPACT_VALUE
    int32_t ref : 31;
    uint32_t is_long : 1;
    int64_t l = 0;
    Value() : ref(0), is_long(0) {}
#else
    int32_t ref = 0;
    int64_t l = 0;
    bool is_long = false;
#endif
    static constexpr int32_t max_reference = 0x3fffffff;
    static Value of_i(int32_t v) { Value x; x.i = v; return x; }
    static Value of_ref(int32_t v) { Value x; x.ref = v; return x; }
    static Value of_l(int64_t v) { Value x; x.l = v; x.is_long = true; return x; }
};
static_assert(std::is_trivially_copyable<Value>::value, "values must be movable without ownership");
#if J2ME_OPT_COMPACT_VALUE
static_assert(sizeof(Value)==16, "compact tagged slot ABI must be 16 bytes");
#endif
