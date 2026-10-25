#ifndef J2ME_VM_RESOURCE_VERSION_H
#define J2ME_VM_RESOURCE_VERSION_H
// P2.5: CPU-side resource identity and versions (no GPU cache, no consumer changes behaviour).
//  - ResourceKey = (epoch, heap slot, slot generation). epoch moves on every heap rebuild (launch / suite switch /
//    instant-state restore, including a failed restore's rollback); generation moves every time a slot is freed,
//    so a recycled slot never matches a key taken for its previous occupant.
//  - Every resource carries layout_version (shape: descriptor/length, image size) and content_version (bytes).
//    Both are stamps from one process-wide monotonic clock, so a fresh object never reuses an old stamp.
//  - Dirty barriers live at the storage choke points: ArrayStorage::reset/set/copy_from and the mutable byte view,
//    and the mutable image view (pixels_of) plus every image (re)allocation. Callers cannot write array bytes any
//    other way (storage is private). Versions are never serialized; J2S5 is unchanged.
//  - Borrow vs owned: a raw pointer from a writable/readable view is a SHORT borrow (valid only until the next VM
//    write, allocation or safepoint). Anything kept longer must be an owned snapshot that records its ResourceStamp
//    and is revalidated with resource_stamp_current() before use.
#include <cstdint>
namespace resver {
inline uint64_t g_clock = 0;   // process-wide, never reset, never serialized
inline uint32_t g_epoch = 1;   // heap identity epoch
struct Stats { uint64_t array_layout = 0, array_content = 0, array_view = 0, image_layout = 0, image_content = 0,
                        generations = 0, epochs = 0; };
inline Stats g_stats;
inline uint64_t tick() { return ++g_clock; }
inline void bump_epoch() { ++g_epoch; ++g_stats.epochs; }
} // namespace resver
struct ResourceKey { uint32_t epoch = 0; int32_t slot = 0; uint32_t generation = 0; };
inline bool operator==(const ResourceKey& a, const ResourceKey& b) { return a.epoch == b.epoch && a.slot == b.slot && a.generation == b.generation; }
struct ResourceStamp { ResourceKey key; uint64_t layout = 0, content = 0; };
#endif
