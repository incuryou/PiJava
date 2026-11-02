#pragma once
#include "presentation/layers.h"
#include "presentation/gpu_filter.h"
#include "presentation/viewport.h"
#include "m3g_gpu_channel.h"
#include <memory>

namespace render_owner {
// Scheduled acquisition and an unscheduled menu present share this boundary.
// Reuse the producer's reservation; Queue::acquire_credit itself is deliberately
// non-reentrant and waiting a second time would deadlock on our own credit.
template<class Queue> inline bool acquire_for_present(Queue& queue) {
    return queue.stats().credit_reserved||queue.acquire_credit();
}
// No borrowed VM/canvas pointers. Chrome is frozen once per generation; repeated
// game frames share the same 3.7 MiB canvas instead of copying it on every submit.
struct FrameRequest {
    enum class Kind { Present, Geometry, Skin } kind=Kind::Present;
    std::unique_ptr<m3g_gpu::GeometryJob> geometry;
    std::unique_ptr<m3g_gpu::SkinJob> skin;
    uint64_t sequence=0,epoch=0,generation=0;
    std::shared_ptr<const std::vector<uint32_t>> chrome;
    std::shared_ptr<const std::vector<uint8_t>> rgb;
    j2me_present::gpu_filter::FrameKey key;
    j2me_present::Viewport overlay{0,0,0,0};
    int rotation=0;
    bool layered=false,shader=false,adaptive=false,invalidate=false;
};
struct ChromeSnapshot {
    uint64_t generation=0;
    std::shared_ptr<const std::vector<uint32_t>> pixels;
    std::shared_ptr<const std::vector<uint32_t>> freeze(const std::vector<uint32_t>& source,uint64_t current) {
        if(!pixels||generation!=current) {
            pixels=std::make_shared<const std::vector<uint32_t>>(source);generation=current;
        }
        return pixels;
    }
};
// Main-thread cache only. Filter/adaptive/chrome affect presentation, not RGB.
// Owned immutable storage outlives clear() while an accepted frame retains it.
struct RgbSnapshot {
    j2me_present::gpu_filter::FrameKey key;
    int rotation=0;
    uint64_t copies=0,copy_bytes=0,reuses=0;
    std::shared_ptr<const std::vector<uint8_t>> pixels;
    void clear(){pixels.reset();}
    std::shared_ptr<const std::vector<uint8_t>> freeze(const std::vector<uint8_t>& source,
            const j2me_present::gpu_filter::FrameKey& current,int turns,bool dirty) {
        if(dirty||!pixels||key.epoch!=current.epoch||key.frame!=current.frame||
           key.w!=current.w||key.h!=current.h||rotation!=turns||pixels->size()!=source.size()) {
            pixels=std::make_shared<const std::vector<uint8_t>>(source);
            key=current;rotation=turns;++copies;copy_bytes+=source.size();
        }else ++reuses;
        return pixels;
    }
};
}
