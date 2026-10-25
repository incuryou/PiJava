#include "../src/presentation_pixels.h"
#include <cassert>
#include <cstdio>

int main() {
    std::vector<uint8_t> rgb(240*320*3);
    for(int y=0;y<320;++y) for(int x=0;x<240;++x) {
        size_t i=(y*240+x)*3;
        rgb[i]=uint8_t(x); rgb[i+1]=uint8_t(y); rgb[i+2]=uint8_t(x^y);
    }
    const auto unchanged=rgb;
    std::vector<uint32_t> rgba,overlay;
    assert(prepare_present_pixels(rgb,240,320,rgba,overlay));
    assert(rgba.size()==240*320 && overlay.size()==256*16);
    for(int y=0;y<320;++y) for(int x=0;x<240;++x) {
        uint32_t c=rgba[y*240+x];
        assert((c&255)==unsigned(x) && ((c>>8)&255)==unsigned(y&255));
        assert(((c>>16)&255)==unsigned((x^y)&255) && (c>>24)==255);
    }
    // Compare every patch pixel with the existing 2x CPU presentation coordinates.
    for(int y=0;y<16;++y) for(int x=0;x<256;++x) {
        size_t i=((y/2)*240+x/2)*3;
        uint32_t expected=uint32_t(rgb[i]) | (uint32_t(rgb[i+1])<<8) | (uint32_t(rgb[i+2])<<16) | 0xff000000u;
        assert(overlay[y*256+x]==expected);
    }
    overlay[0]=0; rgba[0]=0;
    assert(rgb==unchanged); // GPU staging/overlay never changes Java-visible pixels.
    const auto oldrgba=rgba,oldoverlay=overlay;
    assert(!prepare_present_pixels(rgb,320,240,rgba,overlay));
    rgb.pop_back();assert(!prepare_present_pixels(rgb,240,320,rgba,overlay));
    assert(rgba==oldrgba && overlay==oldoverlay);
    PresentStats stats;stats.add(10000,6000,2000,1000,1000);stats.add(20000,12000,3000,4000,1000);
    assert(stats.count==2 && stats.total_us==30000 && stats.max_us==20000);
    assert(stats.average_ms(stats.total_us)==15.0 && stats.average_ms(stats.acquire_us)==9.0);
    assert(stats.prepare_us==5000 && stats.submit_us==5000 && stats.gpu_wait_us==2000);
    stats={};assert(stats.average_ms(stats.total_us)==0);
    std::puts("PASS PresentationChecks RGBA channels, orientation, exact CPU patch, immutable source and timings");
}
