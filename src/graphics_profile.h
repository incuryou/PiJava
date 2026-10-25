#pragma once
#include <chrono>
#include <cstdint>
#include <string>

// Timings are collected on one in 64 Canvas paints. Native times are inclusive;
// raster_ns is a subset of image_native_ns, never an additional CPU cost.
struct GraphicsProfileWindow {
    uint64_t sampled_paints=0, image_calls=0, clip_calls=0, other_calls=0, raster_calls=0;
    int64_t image_native_ns=0, clip_native_ns=0, other_native_ns=0, raster_ns=0;
};
struct GraphicsGlobalWindow {
    uint64_t native_calls=0, offscreen_calls=0, sampled_calls=0, sampled_offscreen_calls=0, raster_calls=0;
    int64_t native_ns=0, offscreen_ns=0, raster_ns=0;
};
struct GraphicsProfile {
    bool enabled=false, sampling=false;
    uint64_t paint_sequence=0, call_sequence=0, generation=1;
    unsigned native_depth=0;
    bool global_sampling=false;
    GraphicsGlobalWindow global_pending;
    GraphicsProfileWindow pending;
    void reset(bool on=true) {++generation;enabled=on;sampling=global_sampling=false;native_depth=0;paint_sequence=call_sequence=0;pending={};global_pending={};}
    uint64_t begin_paint() {
        sampling=enabled&&((paint_sequence++ & 63)==0);
        if(sampling)++pending.sampled_paints;
        return generation;
    }
    void end_paint(uint64_t token) {if(token==generation)sampling=false;}
    void end_paint() {sampling=false;}
    GraphicsGlobalWindow take_global() {auto result=global_pending;global_pending={};return result;}
    GraphicsProfileWindow take() {auto result=pending;pending={};return result;}
};
class GraphicsProfileScope {
    GraphicsProfile* profile=nullptr;
    int category=0;
    int64_t started=0;
    uint64_t generation=0;
    static int64_t clock_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
public:
    GraphicsProfileScope(GraphicsProfile& p, const std::string& name) {
        if(!p.sampling)return;
        profile=&p;generation=p.generation;
        if(name=="drawImage"||name=="drawRegion") {category=1;++p.pending.image_calls;}
        else if(name=="setClip"||name=="clipRect") {category=2;++p.pending.clip_calls;}
        else ++p.pending.other_calls;
        started=clock_ns();
    }
    explicit GraphicsProfileScope(GraphicsProfile& p) {
        if(!p.sampling)return;
        profile=&p;generation=p.generation;category=3;++p.pending.raster_calls;started=clock_ns();
    }
    GraphicsProfileScope(GraphicsProfile& p, int category_id) {
        if(!p.sampling)return;
        profile=&p;generation=p.generation;
        category=category_id;
        if(category_id==1)++p.pending.image_calls;
        else if(category_id==2)++p.pending.clip_calls;
        else ++p.pending.other_calls;
        started=clock_ns();
    }
    ~GraphicsProfileScope() {
        if(!profile || generation!=profile->generation)return;
        int64_t elapsed=clock_ns()-started;
        if(category==1)profile->pending.image_native_ns+=elapsed;
        else if(category==2)profile->pending.clip_native_ns+=elapsed;
        else if(category==3)profile->pending.raster_ns+=elapsed;
        else profile->pending.other_native_ns+=elapsed;
    }
};

// Every outer Graphics native call is counted, including Image destinations and
// calls outside Canvas.paint. Unselected calls take no clock and classify no
// method name. Nested raster timing is a subset of selected native wall time.
class GraphicsGlobalScope {
    GraphicsProfile& profile;
    uint64_t generation;
    bool outer=false, sampled=false, offscreen=false;
    int64_t started=0;
    static int64_t clock_ns() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
public:
    GraphicsGlobalScope(GraphicsProfile& p,bool image_target):profile(p),generation(p.generation),offscreen(image_target) {
        if(!p.enabled)return;
        outer=p.native_depth++==0;
        if(!outer)return;
        ++p.global_pending.native_calls;if(offscreen)++p.global_pending.offscreen_calls;
        sampled=p.global_sampling=((p.call_sequence++ & 63)==0);
        if(sampled) {++p.global_pending.sampled_calls;if(offscreen)++p.global_pending.sampled_offscreen_calls;started=clock_ns();}
    }
    ~GraphicsGlobalScope() {
        if(generation!=profile.generation || !profile.enabled)return;
        if(profile.native_depth)--profile.native_depth;
        if(!outer)return;
        if(sampled) {
            const auto elapsed=clock_ns()-started;
            profile.global_pending.native_ns+=elapsed;
            if(offscreen)profile.global_pending.offscreen_ns+=elapsed;
        }
        profile.global_sampling=false;
    }
};
class GraphicsGlobalRasterScope {
    GraphicsProfile* profile=nullptr;
    uint64_t generation=0;
    int64_t started=0;
    static int64_t clock_ns() {return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
public:
    explicit GraphicsGlobalRasterScope(GraphicsProfile& p) {
        if(!p.global_sampling)return;
        profile=&p;generation=p.generation;started=clock_ns();++p.global_pending.raster_calls;
    }
    ~GraphicsGlobalRasterScope() {if(profile && generation==profile->generation)profile->global_pending.raster_ns+=clock_ns()-started;}
};
