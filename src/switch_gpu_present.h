#pragma once
// Optional Horizon backend. The VM remains a software MIDP rasterizer; deko3d
// performs output scaling/composition and tiled present transfers. This is NOT
// a GPU implementation of Java Graphics or an emulator JIT.
#include <switch.h>
#include <deko3d.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "presentation/layers.h"
#include "presentation/gpu_filter.h"
#include "presentation/viewport.h"
#include "presentation/gpu_filter_assets.h"
#include "suite_identity.h"

class SwitchGpuPresent {
public:
    struct Timings {
        uint64_t acquire_wait_us=0,gpu_wait_us=0,upload_us=0;
        uint64_t acquire_before_present_us=0;
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
        if(!loadShader("romfs:/shaders/filter.vert.dksh",vertexCode_,vertexShader_,j2me_present::gpu_filter::vert_asset)||
           !loadShader("romfs:/shaders/filter.frag.dksh",fragmentCode_,fragmentShader_,j2me_present::gpu_filter::frag_asset))return fail();
        gainsMemory_=memory(kGainsBytes,DkMemBlockFlags_CpuCached|DkMemBlockFlags_GpuCached);
        if(!gainsMemory_)return fail();
        auto* gains=static_cast<uint32_t*>(dkMemBlockGetCpuAddr(gainsMemory_));if(!gains)return fail();
        const auto& table=display_filter::detail::scanlinesdx_gains();
        for(size_t i=0;i<table.size();++i)gains[i]=table[i];
        if(dkMemBlockFlushCpuCache(gainsMemory_,0,kGainsBytes)!=DkResult_Success)return fail();
        DkImageLayout gameLayout,chromeLayout;
        linear(gameLayout,j2me_present::game_w,j2me_present::game_h);
        linear(chromeLayout,j2me_present::output_w,j2me_present::output_h);
        chromeOffset_=alignUp(uint32_t(dkImageLayoutGetSize(&gameLayout)),dkImageLayoutGetAlignment(&chromeLayout));
        stagingBytes_=alignUp(chromeOffset_+uint32_t(dkImageLayoutGetSize(&chromeLayout)),DK_MEMBLOCK_ALIGNMENT);
        DkImageLayoutMaker om;dkImageLayoutMakerDefaults(&om,device_);
        om.flags=DkImageFlags_UsagePresent|DkImageFlags_Usage2DEngine|DkImageFlags_UsageRender;om.format=DkImageFormat_RGBA8_Unorm;
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
            s.inputMemory=memory(j2me_present::gpu_filter::max_input_bytes,DkMemBlockFlags_CpuCached|DkMemBlockFlags_GpuCached);
            if(!s.commandMemory||!s.stagingMemory||!s.outputMemory||!s.inputMemory)return fail();
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
    // Match CPU frame acquisition order: one held slot, acquired before Java
    // on scheduled turns. Staging and command memory are untouched here.
    bool acquire_frame(){
        if(!ready_||dkQueueIsInErrorState(queue_))return false;
        if(acquired_index_>=0)return true;
        const auto start=Clock::now();const int index=dkQueueAcquireImage(queue_,swapchain_);
        acquired_wait_us_=elapsed(start);
        if(index<0||index>=int(kSlots)||dkQueueIsInErrorState(queue_))return false;
        acquired_index_=index;return true;
    }
    // canvas:1280x720, game:480x640 CPU-prepared pixels. The five shader
    // presets instead pass raw logical RGBA + four header words and its key.
    // Pointers are consumed synchronously, never retained by GPU submission.
    bool present_layers(const uint8_t* canvas,uint64_t generation,const uint8_t* game=nullptr,
                        const uint32_t* raw=nullptr,const j2me_present::gpu_filter::FrameKey* key=nullptr,
                        const j2me_present::Viewport* overlay=nullptr){
        timings_={};if(!ready_||!canvas||(overlay&&!j2me_present::valid_viewport(*overlay)))return false;
        if(raw){
            if(game||!key||!j2me_present::gpu_filter::supported(key->filter)||!logical_screen::valid_extent(key->w,key->h)||
               raw[0]!=unsigned(key->w)||raw[1]!=unsigned(key->h)||raw[2]!=unsigned(key->filter)||raw[3]!=(key->adaptive?1u:0u))return false;
        }else invalidate_game();
        const bool acquired_before=acquired_index_>=0;
        if(!acquire_frame())return false;
        const int index=acquired_index_;timings_.acquire_wait_us=acquired_wait_us_;
        timings_.acquire_before_present_us=acquired_before?acquired_wait_us_:0;
        acquired_index_=-1;acquired_wait_us_=0;
        auto& s=slots_[index];auto start=Clock::now();
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
        if(raw&&s.frame.needs_upload(*key)){
            const uint32_t bytes=uint32_t((size_t(key->w)*key->h+4)*4);
            void* input=dkMemBlockGetCpuAddr(s.inputMemory);if(!input)return false;
            std::memcpy(input,raw,bytes);
            if(dkMemBlockFlushCpuCache(s.inputMemory,0,bytes)!=DkResult_Success)return false;
            s.frame.committed(*key);timings_.frame_upload_bytes=bytes;
        }
        timings_.upload_us=elapsed(start);dkCmdBufClear(s.commands);
        DkImageView chromeView,gameView,outView;dkImageViewDefaults(&chromeView,&s.chrome);
        dkImageViewDefaults(&gameView,&s.game);dkImageViewDefaults(&outView,&s.output);
        const DkImageRect canvasRect={0,0,0,j2me_present::output_w,j2me_present::output_h,1};
        const DkImageRect srcRect={0,0,0,j2me_present::game_w,j2me_present::game_h,1};
        const DkImageRect dstRect={j2me_present::game_x,j2me_present::game_y,0,j2me_present::game_w,j2me_present::game_h,1};
        const uint32_t flags=DkBlitFlag_FilterNearest|DkBlitFlag_ModeBlit;
        dkCmdBufBlitImage(s.commands,&chromeView,&canvasRect,&outView,&canvasRect,flags,0);
        if(game){dkCmdBufBarrier(s.commands,DkBarrier_Full,0);dkCmdBufBlitImage(s.commands,&gameView,&srcRect,&outView,&dstRect,flags,0);}
        if(raw)drawFilter(s,outView,*key);
        // Reapply the existing opaque FPS box after game pixels. The canonical
        // chrome already contains its pixels; other chrome stays underneath.
        if(overlay){
            const DkImageRect rect={uint32_t(overlay->x),uint32_t(overlay->y),0,uint32_t(overlay->w),uint32_t(overlay->h),1};
            dkCmdBufBarrier(s.commands,DkBarrier_Full,0);
            dkCmdBufBlitImage(s.commands,&chromeView,&rect,&outView,&rect,flags,0);
        }
        dkCmdBufBarrier(s.commands,DkBarrier_Full,0);dkCmdBufSignalFence(s.commands,&s.fence,true);
        dkQueueSubmitCommands(queue_,dkCmdBufFinishList(s.commands));s.pending=true;
        dkQueuePresentImage(queue_,swapchain_,index);return !dkQueueIsInErrorState(queue_);
    }
    // Invalidation changes metadata only. Memory is still written exclusively
    // after the corresponding slot fence, including transitions and restores.
    void invalidate_game(){for(auto& s:slots_)s.frame.invalidate();}
    void close(){
        ready_=false;if(queue_)dkQueueWaitIdle(queue_);
        if(swapchain_){dkSwapchainDestroy(swapchain_);swapchain_=nullptr;}
        if(queue_){dkQueueDestroy(queue_);queue_=nullptr;}
        for(auto& s:slots_){
            if(s.commands)dkCmdBufDestroy(s.commands);
            if(s.commandMemory)dkMemBlockDestroy(s.commandMemory);
            if(s.outputMemory)dkMemBlockDestroy(s.outputMemory);
            if(s.stagingMemory)dkMemBlockDestroy(s.stagingMemory);
            if(s.inputMemory)dkMemBlockDestroy(s.inputMemory);
            s=Slot{};
        }
        if(gainsMemory_){dkMemBlockDestroy(gainsMemory_);gainsMemory_=nullptr;}
        if(vertexCode_){dkMemBlockDestroy(vertexCode_);vertexCode_=nullptr;}
        if(fragmentCode_){dkMemBlockDestroy(fragmentCode_);fragmentCode_=nullptr;}
        if(device_){dkDeviceDestroy(device_);device_=nullptr;}
        chromeOffset_=stagingBytes_=0;timings_={};acquired_index_=-1;acquired_wait_us_=0;
    }
    bool ready()const{return ready_;}
    const Timings& lastTimings()const{return timings_;}
    uint64_t staging_bytes()const{return uint64_t(stagingBytes_+alignUp(j2me_present::gpu_filter::max_input_bytes,DK_MEMBLOCK_ALIGNMENT))*kSlots;}
private:
    using Clock=std::chrono::steady_clock;
    enum :uint32_t{kSlots=3,kCommandBytes=65536,kGameBytes=j2me_present::game_w*j2me_present::game_h*4,kChromeBytes=j2me_present::output_w*j2me_present::output_h*4,kGainsBytes=48*64*4};
    struct Slot{
        DkCmdBuf commands=nullptr;DkMemBlock commandMemory=nullptr,stagingMemory=nullptr,outputMemory=nullptr,inputMemory=nullptr;
        DkImage game={},chrome={},output={};DkFence fence={};uint8_t* staging=nullptr;bool pending=false;
        j2me_present::LayerGeneration generation;
        j2me_present::gpu_filter::FrameCache frame;
    };
    static uint32_t alignUp(uint32_t v,uint32_t a){return(v+a-1)&~(a-1);}
    static uint64_t elapsed(Clock::time_point t){return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-t).count());}
    DkMemBlock memory(uint32_t size,uint32_t flags){DkMemBlockMaker m;dkMemBlockMakerDefaults(&m,device_,alignUp(size,DK_MEMBLOCK_ALIGNMENT));m.flags=flags;return dkMemBlockCreate(&m);}
    bool loadShader(const char* path,DkMemBlock& code,DkShader& shader,const j2me_present::gpu_filter::ShaderAsset& expected){
        FILE* file=std::fopen(path,"rb");if(!file)return false;
        bool ok=std::fseek(file,0,SEEK_END)==0;long bytes=ok?std::ftell(file):-1;
        if(bytes<24||bytes>1024*1024||unsigned(bytes)!=expected.bytes||std::fseek(file,0,SEEK_SET)!=0){std::fclose(file);return false;}
        // Whole DKSH at offset zero; deko3d parses its control section. Code
        // blocks retain the documented unusable tail and live until idle.
        code=memory(uint32_t(bytes)+DK_SHADER_CODE_UNUSABLE_SIZE,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code);
        void* data=code?dkMemBlockGetCpuAddr(code):nullptr;
        ok=data&&std::fread(data,1,size_t(bytes),file)==size_t(bytes);
        ok=std::fclose(file)==0&&ok;if(!ok||std::memcmp(data,"DKSH",4)!=0)return false;
        // Release deko3d trusts DKSH offsets; IsValid only checks its object
        // magic. Pin every byte, including stage/program/code metadata, before
        // the SDK touches it. Pins are generated from the matching UAM stage.
        rms_suite_identity_detail::Sha256 hash;hash.update(static_cast<const uint8_t*>(data),size_t(bytes));
        if(hash.finish()!=expected.sha256)return false;
        DkShaderMaker maker;dkShaderMakerDefaults(&maker,code,0);dkShaderInitialize(&shader,&maker);
        return dkShaderIsValid(&shader);
    }
    void drawFilter(Slot& s,const DkImageView& output,const j2me_present::gpu_filter::FrameKey& key){
        dkCmdBufBarrier(s.commands,DkBarrier_Full,DkInvalidateFlags_Shader|DkInvalidateFlags_L2Cache);
        const DkImageView* targets[]={&output};dkCmdBufBindRenderTargets(s.commands,targets,1,nullptr);
        const DkShader* shaders[]={&vertexShader_,&fragmentShader_};
        dkCmdBufBindShaders(s.commands,DkStageFlag_GraphicsMask,shaders,2);
        const DkBufExtents buffers[]={
            {dkMemBlockGetGpuAddr(s.inputMemory),uint32_t((size_t(key.w)*key.h+4)*4)},
            {dkMemBlockGetGpuAddr(gainsMemory_),kGainsBytes}};
        dkCmdBufBindStorageBuffers(s.commands,DkStage_Fragment,0,buffers,2);
        DkRasterizerState raster;dkRasterizerStateDefaults(&raster);raster.cullMode=DkFace_None;
        DkMultisampleState multisample;dkMultisampleStateDefaults(&multisample);
        DkColorState color;dkColorStateDefaults(&color);
        DkColorWriteState writes;dkColorWriteStateDefaults(&writes);
        DkDepthStencilState depth;dkDepthStencilStateDefaults(&depth);
        dkCmdBufBindRasterizerState(s.commands,&raster);dkCmdBufBindMultisampleState(s.commands,&multisample);
        dkCmdBufBindColorState(s.commands,&color);dkCmdBufBindColorWriteState(s.commands,&writes);dkCmdBufBindDepthStencilState(s.commands,&depth);
        const auto rect=j2me_present::game_viewport(key.w,key.h,key.adaptive);
        const DkViewport viewport={float(rect.x),float(rect.y),float(rect.w),float(rect.h),0,1};
        const DkScissor scissor={uint32_t(rect.x),uint32_t(rect.y),uint32_t(rect.w),uint32_t(rect.h)};
        dkCmdBufSetViewports(s.commands,0,&viewport,1);dkCmdBufSetScissors(s.commands,0,&scissor,1);
        dkCmdBufDraw(s.commands,DkPrimitive_Triangles,3,1,0,0);
    }
    void linear(DkImageLayout& l,uint32_t w,uint32_t h){DkImageLayoutMaker m;dkImageLayoutMakerDefaults(&m,device_);m.flags=DkImageFlags_PitchLinear|DkImageFlags_Usage2DEngine;m.format=DkImageFormat_RGBA8_Unorm;m.dimensions[0]=w;m.dimensions[1]=h;m.pitchStride=w*4;dkImageLayoutInitialize(&l,&m);}
    bool fail(){close();return false;}
    Slot slots_[kSlots];DkDevice device_=nullptr;DkQueue queue_=nullptr;DkSwapchain swapchain_=nullptr;
    DkMemBlock vertexCode_=nullptr,fragmentCode_=nullptr,gainsMemory_=nullptr;DkShader vertexShader_={},fragmentShader_={};
    uint32_t chromeOffset_=0,stagingBytes_=0;bool ready_=false;Timings timings_;
    int acquired_index_=-1;uint64_t acquired_wait_us_=0;
};
