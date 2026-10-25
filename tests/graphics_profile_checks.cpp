#include "../src/graphics_profile.h"
#include <cassert>
#include <iostream>

int main() {
    GraphicsProfile p;
    p.begin_paint();assert(!p.sampling);p.end_paint();
    p.reset();
    for(int i=0;i<130;++i) {
        p.begin_paint();assert(p.sampling==(i%64==0));
        {GraphicsProfileScope native(p,"drawImage");GraphicsProfileScope raster(p);}
        {GraphicsProfileScope clip(p,"setClip");}
        {GraphicsProfileScope other(p,"getColor");}
        p.end_paint();assert(!p.sampling);
    }
    auto w=p.take();
    assert(w.sampled_paints==3&&w.image_calls==3&&w.clip_calls==3&&w.other_calls==3&&w.raster_calls==3);
    assert(w.image_native_ns>=w.raster_ns&&w.raster_ns>=0&&w.clip_native_ns>=0&&w.other_native_ns>=0);
    assert(p.take().sampled_paints==0);
    p.reset(false);p.begin_paint();assert(!p.sampling);p.end_paint();
    assert(p.take().image_calls==0);
    p.reset();
    for(int i=0;i<130;++i) {
        GraphicsGlobalScope native(p,true);
        GraphicsGlobalRasterScope raster(p);
    }
    auto global=p.take_global();
    assert(global.native_calls==130 && global.offscreen_calls==130 && global.sampled_calls==3 && global.sampled_offscreen_calls==3 && global.raster_calls==3);
    assert(global.native_ns>=global.raster_ns && global.offscreen_ns==global.native_ns);
    assert(p.take().sampled_paints==0); // Image Graphics without Canvas.paint.
    p.reset();
    {GraphicsGlobalScope outer(p,false);GraphicsGlobalScope nested(p,true);}
    global=p.take_global();assert(global.native_calls==1 && global.offscreen_calls==0 && global.sampled_calls==1);
    auto token=p.begin_paint();
    {GraphicsProfileScope old(p,"drawImage");GraphicsGlobalScope old_global(p,true);p.reset();p.begin_paint();}
    p.end_paint(token);assert(p.sampling && !p.global_sampling && p.native_depth==0);
    assert(p.take().image_native_ns==0 && p.take_global().native_ns==0);
    p.end_paint();p.reset(false);
    {GraphicsGlobalScope disabled(p,true);GraphicsGlobalRasterScope raster(p);}
    assert(p.take_global().native_calls==0);
    std::cout<<"PASS GraphicsProfileChecks Canvas and global 1/64 sampling, offscreen calls, raster subset, reset generations and disabled guards\n";
}
