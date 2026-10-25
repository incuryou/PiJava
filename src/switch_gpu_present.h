#pragma once
// Optional Horizon backend. The VM remains a software MIDP rasterizer; deko3d
// performs output scaling/composition and tiled present transfers. This is NOT
// a GPU implementation of Java Graphics or an emulator JIT.
#include <switch.h>
#include <deko3d.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include "presentation/layers.h"

class SwitchGpuPresent {
public:
    struct Timings {
        uint64_t acquire_wait_us=0,gpu_wait_us=0,upload_us=0;
        uint64_t frame_upload_bytes=0,chrome_upload_bytes=0;
    };
    SwitchGpuPresent()=default;
    SwitchGpuPresent(const SwitchGpuPresent&)=delete;
    SwitchGpuPresent& operator=(const SwitchGpuPresent&)=delete;
    ~SwitchGpuPresent(){close();}
    // No other Framebuffer/swapchain may own this window. deko3d fatal driver
    // callbacks cannot in general be converted into a recoverable bool result.
    bool init(NWindow* window=nullptr){
        if(ready_)return true;close();if(!window)window=nwindowGetDefault();
        if(!window||!nwindowIsValid(window))return false;
        DkDeviceMaker dm;dkDeviceMakerDefaults(&dm);device_=dkDeviceCreate(&dm);if(!device_)return fail();
        DkQueueMaker qm;dkQueueMakerDefaults(&qm,device_);qm.flags=DkQueueFlags_Graphics|DkQueueFlags_DisableZcull;
        queue_=dkQueueCreate(&qm);if(!queue_)return fail();
        DkImageLayout gameLayout,chromeLayout;
        linear(gameLayout,j2me_present::game_w,j2me_present::game_h);
        linear(chromeLayout,j2me_present::output_w,j2me_present::output_h);
        chromeOffset_=alignUp(uint32_t(dkImageLayoutGetSize(&gameLayout)),dkImageLayoutGetAlignment(&chromeLayout));
        stagingBytes_=alignUp(chromeOffset_+uint32_t(dkImageLayoutGetSize(&chromeLayout)),DK_MEMBLOCK_ALIGNMENT);
        DkImageLayoutMaker om;dkImageLayoutMakerDefaults(&om,device_);
        om.flags=DkImageFlags_UsagePresent|DkImageFlags_Usage2DEngine;om.format=DkImageFormat_RGBA8_Unorm;
        om.dimensions[0]=j2me_present::output_w;om.dimensions[1]=j2me_present::output_h;
        DkImageLayout outputLayout;dkImageLayoutInitialize(&outputLayout,&om);
        const uint32_t outputBytes=alignUp(uint32_t(dkImageLayoutGetSize(&outputLayout)),DK_MEMBLOCK_ALIGNMENT);
        const DkImage* images[kSlots];
        for(unsigned i=0;i<kSlots;i++){
            auto& s=slots_[i];DkCmdBufMaker cm;dkCmdBufMakerDefaults(&cm,device_);s.commands=dkCmdBufCreate(&cm);
            if(!s.commands)return fail();
            s.commandMemory=memory(kCommandBytes,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);
            s.stagingMemory=memory(stagingBytes_,DkMemBlockFlags_CpuCached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image);
            s.outputMemory=memory(outputBytes,DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image);
            if(!s.commandMemory||!s.stagingMemory||!s.outputMemory)return fail();
            dkCmdBufAddMemory(s.commands,s.commandMemory,0,kCommandBytes);
            s.staging=static_cast<uint8_t*>(dkMemBlockGetCpuAddr(s.stagingMemory));if(!s.staging)return fail();
            dkImageInitialize(&s.game,&gameLayout,s.stagingMemory,0);
            dkImageInitialize(&s.chrome,&chromeLayout,s.stagingMemory,chromeOffset_);
            dkImageInitialize(&s.output,&outputLayout,s.outputMemory,0);images[i]=&s.output;
        }
        DkSwapchainMaker sm;dkSwapchainMakerDefaults(&sm,device_,window,images,kSlots);
        swapchain_=dkSwapchainCreate(&sm);if(!swapchain_)return fail();dkSwapchainSetSwapInterval(swapchain_,1);
        ready_=!dkQueueIsInErrorState(queue_);if(!ready_)return fail();return true;
    }
    // canvas is exactly 1280x720 RGBA; game is null for fully composed menus,
    // import screens or pause overlays, otherwise exactly 480x640 RGBA. The
    // shared CPU preparation already fits/samples the original Java canvas;
    // the GPU copies 1:1 so both presenters use identical rounding and pixels.
    // Generation must change whenever the caller modifies canvas bytes.
    bool present_layers(const uint8_t* canvas,uint64_t generation,const uint8_t* game=nullptr){
        timings_={};if(!ready_||!canvas)return false;
        auto start=Clock::now();const int index=dkQueueAcquireImage(queue_,swapchain_);
        timings_.acquire_wait_us=elapsed(start);
        if(index<0||index>=int(kSlots)||dkQueueIsInErrorState(queue_))return false;
        auto& s=slots_[index];start=Clock::now();
        // Never mutate staging bytes or command memory still referenced by GPU.
        // Wait only for the reused slot, NOT for every queued frame to go idle.
        if(s.pending && dkFenceWait(&s.fence,1000000000LL)!=DkResult_Success)return false;
        timings_.gpu_wait_us=elapsed(start);s.pending=false;start=Clock::now();
        if(s.generation.needs_upload(generation)){
            std::memcpy(s.staging+chromeOffset_,canvas,kChromeBytes);
            if(dkMemBlockFlushCpuCache(s.stagingMemory,chromeOffset_,kChromeBytes)!=DkResult_Success)return false;
            s.generation.committed(generation);timings_.chrome_upload_bytes=kChromeBytes;
        }
        if(game){std::memcpy(s.staging,game,kGameBytes);if(dkMemBlockFlushCpuCache(s.stagingMemory,0,kGameBytes)!=DkResult_Success)return false;timings_.frame_upload_bytes=kGameBytes;}
        timings_.upload_us=elapsed(start);dkCmdBufClear(s.commands);
        DkImageView chromeView,gameView,outView;dkImageViewDefaults(&chromeView,&s.chrome);
        dkImageViewDefaults(&gameView,&s.game);dkImageViewDefaults(&outView,&s.output);
        const DkImageRect canvasRect={0,0,0,j2me_present::output_w,j2me_present::output_h,1};
        const DkImageRect srcRect={0,0,0,j2me_present::game_w,j2me_present::game_h,1};
        const DkImageRect dstRect={j2me_present::game_x,j2me_present::game_y,0,j2me_present::game_w,j2me_present::game_h,1};
        const uint32_t flags=DkBlitFlag_FilterNearest|DkBlitFlag_ModeBlit;
        dkCmdBufBlitImage(s.commands,&chromeView,&canvasRect,&outView,&canvasRect,flags,0);
        if(game){dkCmdBufBarrier(s.commands,DkBarrier_Full,0);dkCmdBufBlitImage(s.commands,&gameView,&srcRect,&outView,&dstRect,flags,0);}
        dkCmdBufBarrier(s.commands,DkBarrier_Full,0);dkCmdBufSignalFence(s.commands,&s.fence,true);
        dkQueueSubmitCommands(queue_,dkCmdBufFinishList(s.commands));s.pending=true;
        dkQueuePresentImage(queue_,swapchain_,index);return !dkQueueIsInErrorState(queue_);
    }
    void close(){
        ready_=false;if(queue_)dkQueueWaitIdle(queue_);
        if(swapchain_){dkSwapchainDestroy(swapchain_);swapchain_=nullptr;}
        if(queue_){dkQueueDestroy(queue_);queue_=nullptr;}
        for(auto& s:slots_){
            if(s.commands)dkCmdBufDestroy(s.commands);
            if(s.commandMemory)dkMemBlockDestroy(s.commandMemory);
            if(s.outputMemory)dkMemBlockDestroy(s.outputMemory);
            if(s.stagingMemory)dkMemBlockDestroy(s.stagingMemory);
            s=Slot{};
        }
        if(device_){dkDeviceDestroy(device_);device_=nullptr;}
        chromeOffset_=stagingBytes_=0;timings_={};
    }
    bool ready()const{return ready_;}
    const Timings& lastTimings()const{return timings_;}
    uint64_t staging_bytes()const{return uint64_t(stagingBytes_)*kSlots;}
private:
    using Clock=std::chrono::steady_clock;
    enum :uint32_t{kSlots=3,kCommandBytes=65536,kGameBytes=j2me_present::game_w*j2me_present::game_h*4,kChromeBytes=j2me_present::output_w*j2me_present::output_h*4};
    struct Slot{
        DkCmdBuf commands=nullptr;DkMemBlock commandMemory=nullptr,stagingMemory=nullptr,outputMemory=nullptr;
        DkImage game={},chrome={},output={};DkFence fence={};uint8_t* staging=nullptr;bool pending=false;
        j2me_present::LayerGeneration generation;
    };
    static uint32_t alignUp(uint32_t v,uint32_t a){return(v+a-1)&~(a-1);}
    static uint64_t elapsed(Clock::time_point t){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-t).count());}
    DkMemBlock memory(uint32_t size,uint32_t flags){DkMemBlockMaker m;dkMemBlockMakerDefaults(&m,device_,alignUp(size,DK_MEMBLOCK_ALIGNMENT));m.flags=flags;return dkMemBlockCreate(&m);}
    void linear(DkImageLayout& l,uint32_t w,uint32_t h){DkImageLayoutMaker m;dkImageLayoutMakerDefaults(&m,device_);m.flags=DkImageFlags_PitchLinear|DkImageFlags_Usage2DEngine;m.format=DkImageFormat_RGBA8_Unorm;m.dimensions[0]=w;m.dimensions[1]=h;m.pitchStride=w*4;dkImageLayoutInitialize(&l,&m);}
    bool fail(){close();return false;}
    Slot slots_[kSlots];DkDevice device_=nullptr;DkQueue queue_=nullptr;DkSwapchain swapchain_=nullptr;
    uint32_t chromeOffset_=0,stagingBytes_=0;bool ready_=false;Timings timings_;
};
