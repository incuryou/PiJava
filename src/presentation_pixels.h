#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Keep staging separate from the Java-visible software framebuffer. The GPU
// receives immutable bytes for each submission; FPS text never changes getRGB.
inline bool prepare_present_pixels(const std::vector<uint8_t>& rgb, int width, int height,
                                   std::vector<uint32_t>& rgba, std::vector<uint32_t>& overlay) {
    if (width != 240 || height != 320 || rgb.size() != std::size_t(width)*height*3) return false;
    rgba.resize(std::size_t(width)*height);
    for (std::size_t i=0;i<rgba.size();++i)
        rgba[i]=uint32_t(rgb[i*3]) | (uint32_t(rgb[i*3+1])<<8) | (uint32_t(rgb[i*3+2])<<16) | 0xff000000u;
    overlay.resize(256*16);
    for(int y=0;y<16;++y)
        for(int x=0;x<256;++x) overlay[std::size_t(y)*256+x]=rgba[std::size_t(y/2)*width+x/2];
    return true;
}

struct PresentStats {
    uint64_t count=0, total_us=0, max_us=0, acquire_us=0, prepare_us=0, submit_us=0, gpu_wait_us=0;
    void add(uint64_t total,uint64_t acquire,uint64_t prepare,uint64_t submit,uint64_t gpu_wait=0) {
        ++count; total_us+=total; if(total>max_us)max_us=total;
        acquire_us+=acquire; prepare_us+=prepare; submit_us+=submit; gpu_wait_us+=gpu_wait;
    }
    double average_ms(uint64_t value) const { return count ? value/(1000.0*count) : 0; }
};
