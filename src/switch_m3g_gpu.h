#pragma once
// Owner-thread-only offscreen work on the presenter's existing device/queue.
// No VM handles, window ownership, second device, or second swapchain.
#include <deko3d.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "m3g_gpu_types.h"
#include "m3g_gpu_order.h"
#include "presentation/gpu_filter_assets.h"
#include "m3g_gpu_shader_assets.h"
#include "suite_identity.h"

class SwitchM3GGpu {
public:
    struct Metrics {
        uint64_t allocations=0,reuses=0,resident_bytes=0,peak=0;
    };
    SwitchM3GGpu()=default;
    SwitchM3GGpu(const SwitchM3GGpu&)=delete;
    SwitchM3GGpu& operator=(const SwitchM3GGpu&)=delete;
    ~SwitchM3GGpu(){close();}
    void attach(DkDevice device,DkQueue queue){device_=device;queue_=queue;}
    // Owner-thread snapshot. Counts are cumulative; resident becomes zero at
    // close. Reuses counts required work blocks retained without allocation.
    const Metrics& metrics()const{return metrics_;}
    m3g_gpu::GeometryResult execute_geometry(const m3g_gpu::GeometryRequest& q){
        using namespace m3g_gpu;
        GeometryResult out;std::vector<uint32_t> ordered;auto error=validate(q);
        if(error.empty())error=prepare_geometry_order(q,ordered);
        if(error.empty())error=geometryCapability(q);
        const auto mapping=geometry_depth_mapping(q.near_depth,q.far_depth);
        if(error.empty()&&(!std::isfinite(mapping[0])||!std::isfinite(mapping[1])))error="depth_mapping";
        if(!error.empty()){out.code=Code::Unsupported;out.reason=std::move(error);return out;}
        if(!prepare(false)){out.reason="deko_geometry_unavailable";return out;}
        const uint32_t pixels=q.width*q.height,pixelBytes=pixels*4;
        const uint32_t vertexBytes=uint32_t(q.positions.size()/3)*20,indexBytes=uint32_t(q.indices.size())*4;
        const uint32_t uniformBytes=aligned(sizeof(GeometryUniform));
        const uint32_t vertexOffset=uniformBytes,indexOffset=aligned(vertexOffset+vertexBytes);
        const uint32_t textureOffset=aligned(indexOffset+indexBytes),textureBytes=std::max(4u,uint32_t(q.texture.size())*4);
        const uint32_t colorOffset=aligned(textureOffset+textureBytes),depthOffset=aligned(colorOffset+pixelBytes);
        const uint32_t inputBytes=depthOffset+pixelBytes,outputDepthOffset=aligned(pixelBytes),outputBytes=outputDepthOffset+pixelBytes;
        DkImageLayout colorLayout,depthLayout;
        imageLayout(colorLayout,q.width,q.height,DkImageFormat_RGBA8_Unorm);
        imageLayout(depthLayout,q.width,q.height,DkImageFormat_ZF32);
        const uint64_t colorSize=dkImageLayoutGetSize(&colorLayout),depthSize=dkImageLayoutGetSize(&depthLayout);
        const uint64_t imageDepthOffset=(colorSize+dkImageLayoutGetAlignment(&depthLayout)-1)&~uint64_t(dkImageLayoutGetAlignment(&depthLayout)-1);
        if(imageDepthOffset+depthSize>kBudget||!allocateWork(inputBytes,outputBytes,uint32_t(imageDepthOffset+depthSize))){out.reason="deko_geometry_budget_or_memory";return out;}
        auto* data=static_cast<uint8_t*>(dkMemBlockGetCpuAddr(upload_));
        GeometryUniform u={};std::memcpy(u.model_view,q.model_view.data(),64);std::memcpy(u.projection,q.projection.data(),64);
        std::memcpy(u.texture_matrix,q.texture_matrix.data(),64);
        std::memcpy(u.clip_transform,q.clip_transform.data(),64);
        u.config[0]=q.default_color;u.config[1]=q.texture_width;u.config[2]=q.texture_height;
        u.config[3]=(!q.texture.empty()?1u:0u)|(q.wrap_s?2u:0u)|(q.wrap_t?4u:0u)|(q.perspective_correct?16u:0u)|(q.canonical_clip?32u:0u);
        u.depth_range[0]=q.near_depth;u.depth_range[1]=q.far_depth;u.depth_range[2]=mapping[0];u.depth_range[3]=mapping[1];std::memcpy(data,&u,sizeof(u));
        auto* vertices=reinterpret_cast<float*>(data+vertexOffset);
        for(size_t i=0;i<q.positions.size()/3;++i){
            for(unsigned j=0;j<3;++j)vertices[5*i+j]=q.positions[3*i+j];
            vertices[5*i+3]=q.uv.empty()?0:q.uv[2*i];vertices[5*i+4]=q.uv.empty()?0:q.uv[2*i+1];
        }
        std::memcpy(data+indexOffset,ordered.data(),indexBytes);
        if(!q.texture.empty())std::memcpy(data+textureOffset,q.texture.data(),q.texture.size()*4);
        else std::memset(data+textureOffset,0,4);
        auto* rgba=reinterpret_cast<uint32_t*>(data+colorOffset);auto* depths=reinterpret_cast<float*>(data+depthOffset);
        for(uint32_t i=0;i<pixels;++i){rgba[i]=argbRgba(q.colors[i]);depths[i]=float(int(q.depth[i])+32768)/65536.0f;}
        if(!flush(inputBytes)){out.reason="deko_upload_flush";return out;}
        DkImage colorImage,depthImage;dkImageInitialize(&colorImage,&colorLayout,images_,0);
        dkImageInitialize(&depthImage,&depthLayout,images_,uint32_t(imageDepthOffset));
        DkImageView colorView,depthView;dkImageViewDefaults(&colorView,&colorImage);dkImageViewDefaults(&depthView,&depthImage);
        const DkImageRect rect={0,0,0,q.width,q.height,1};const auto uploadAddr=dkMemBlockGetGpuAddr(upload_);
        const DkCopyBuf colorInput={uploadAddr+colorOffset,0,0},depthInput={uploadAddr+depthOffset,0,0};
        dkCmdBufCopyBufferToImage(commands_,&colorInput,&colorView,&rect,0);
        dkCmdBufCopyBufferToImage(commands_,&depthInput,&depthView,&rect,0);
        dkCmdBufBarrier(commands_,DkBarrier_Full,DkInvalidateFlags_Shader|DkInvalidateFlags_L2Cache);
        const DkImageView* targets[]={&colorView};dkCmdBufBindRenderTargets(commands_,targets,1,&depthView);
        const DkShader* shaders[]={&vertexShader_,&fragmentShader_};dkCmdBufBindShaders(commands_,DkStageFlag_GraphicsMask,shaders,2);
        const DkBufExtents vbs[]={{uploadAddr+vertexOffset,vertexBytes},{uploadAddr+indexOffset,indexBytes}};
        dkCmdBufBindStorageBuffers(commands_,DkStage_Vertex,0,vbs,2);
        const DkBufExtents tex={uploadAddr+textureOffset,textureBytes};dkCmdBufBindStorageBuffers(commands_,DkStage_Fragment,2,&tex,1);
        dkCmdBufBindUniformBuffer(commands_,DkStage_Vertex,0,uploadAddr,uniformBytes);
        dkCmdBufBindUniformBuffer(commands_,DkStage_Fragment,0,uploadAddr,uniformBytes);
        DkRasterizerState raster;dkRasterizerStateDefaults(&raster);
        raster.cullMode=q.cull==Cull::None?DkFace_None:q.cull==Cull::Back?DkFace_Back:DkFace_Front;
        raster.frontFace=q.front_cw?DkFrontFace_CW:DkFrontFace_CCW;
        DkMultisampleState multisample;dkMultisampleStateDefaults(&multisample);
        DkColorState color;dkColorStateDefaults(&color);DkColorWriteState writes;dkColorWriteStateDefaults(&writes);
        DkDepthStencilState depth;dkDepthStencilStateDefaults(&depth);
        // Java depth test/write are independent. GL/deko disabling the test also
        // disables writes, so use ALWAYS for the test-disabled/write-enabled case.
        depth.depthTestEnable=true;depth.depthWriteEnable=q.depth_write;
        depth.depthCompareOp=q.depth_test?DkCompareOp_Lequal:DkCompareOp_Always;
        dkCmdBufBindRasterizerState(commands_,&raster);dkCmdBufBindMultisampleState(commands_,&multisample);
        dkCmdBufBindColorState(commands_,&color);dkCmdBufBindColorWriteState(commands_,&writes);dkCmdBufBindDepthStencilState(commands_,&depth);
        const DkViewport vp={float(q.viewport.x),float(q.viewport.y),float(q.viewport.w),float(q.viewport.h),0,1};
        const DkScissor sc={uint32_t(q.clip.x),uint32_t(q.clip.y),uint32_t(q.clip.w),uint32_t(q.clip.h)};
        dkCmdBufSetViewports(commands_,0,&vp,1);dkCmdBufSetScissors(commands_,0,&sc,1);
        dkCmdBufDraw(commands_,DkPrimitive_Triangles,uint32_t(q.indices.size()),1,0,0);
        dkCmdBufBarrier(commands_,DkBarrier_Full,0);
        const auto readAddr=dkMemBlockGetGpuAddr(readback_);
        const DkCopyBuf colorOutput={readAddr,0,0},depthOutput={readAddr+outputDepthOffset,0,0};
        dkCmdBufCopyImageToBuffer(commands_,&colorView,&rect,&colorOutput,0);
        dkCmdBufCopyImageToBuffer(commands_,&depthView,&rect,&depthOutput,0);
        if(!submit()){out.code=Code::Failed;out.reason="deko_geometry_completion";return out;}
        auto* output=static_cast<const uint8_t*>(dkMemBlockGetCpuAddr(readback_));
        auto* outputColor=reinterpret_cast<const uint32_t*>(output);auto* outputDepth=reinterpret_cast<const float*>(output+outputDepthOffset);
        out.colors.resize(pixels);out.depth.resize(pixels);
        for(uint32_t i=0;i<pixels;++i){
            if(!std::isfinite(outputDepth[i])||outputDepth[i]<0||outputDepth[i]>65535.0f/65536.0f){out.code=Code::Failed;out.reason="deko_depth_readback";out.colors.clear();out.depth.clear();return out;}
            out.colors[i]=argbRgba(outputColor[i]);out.depth[i]=int16_t(int(std::lround(outputDepth[i]*65536.0f))-32768);
        }
        out.code=Code::Success;out.reason="deko_geometry_gpu";return out;
    }
    m3g_gpu::SkinResult execute_skin(const m3g_gpu::SkinRequest& q){
        using namespace m3g_gpu;
        SkinResult out;auto error=validate(q);if(!error.empty()){out.code=Code::Unsupported;out.reason=std::move(error);return out;}
        if(!prepare(true)){out.reason="deko_skin_unavailable";return out;}
        const uint32_t positionBytes=uint32_t(q.positions.size())*4,normalBytes=std::max(4u,uint32_t(q.normals.size())*4);
        const uint32_t matrixBytes=std::max(4u,uint32_t(q.matrices.size())*4),rangeBytes=std::max(16u,uint32_t(q.ranges.size())*16);
        const uint32_t po=256,no=aligned(po+positionBytes),mo=aligned(no+normalBytes),ro=aligned(mo+matrixBytes);
        const uint32_t inputBytes=ro+rangeBytes,outputNormalOffset=aligned(positionBytes),outputBytes=outputNormalOffset+normalBytes;
        if(!allocateWork(inputBytes,outputBytes)){out.reason="deko_skin_budget_or_memory";return out;}
        auto* data=static_cast<uint8_t*>(dkMemBlockGetCpuAddr(upload_));std::memset(data,0,inputBytes);
        const uint32_t counts[4]={uint32_t(q.positions.size()/3),uint32_t(q.ranges.size()),q.normals.empty()?0u:1u,0};
        std::memcpy(data,counts,sizeof(counts));std::memcpy(data+po,q.positions.data(),positionBytes);
        if(!q.normals.empty())std::memcpy(data+no,q.normals.data(),q.normals.size()*4);
        if(!q.matrices.empty())std::memcpy(data+mo,q.matrices.data(),q.matrices.size()*4);
        auto* ranges=reinterpret_cast<Range*>(data+ro);
        for(size_t i=0;i<q.ranges.size();++i){auto& r=q.ranges[i];float w=float(r.weight);uint32_t bits;std::memcpy(&bits,&w,4);ranges[i]={r.first,r.count,bits,r.bone};}
        if(!flush(inputBytes)){out.reason="deko_upload_flush";return out;}
        const auto a=dkMemBlockGetGpuAddr(upload_),b=dkMemBlockGetGpuAddr(readback_);
        const DkBufExtents buffers[]={{a+po,positionBytes},{a+no,normalBytes},{a+mo,matrixBytes},{a+ro,rangeBytes},
                                      {b,positionBytes},{b+outputNormalOffset,normalBytes}};
        dkCmdBufBarrier(commands_,DkBarrier_Full,DkInvalidateFlags_Shader|DkInvalidateFlags_L2Cache);
        const DkShader* shaders[]={&skinShader_};dkCmdBufBindShaders(commands_,DkStageFlag_Compute,shaders,1);
        dkCmdBufBindStorageBuffers(commands_,DkStage_Compute,0,buffers,6);
        dkCmdBufBindUniformBuffer(commands_,DkStage_Compute,0,a,256);
        dkCmdBufDispatchCompute(commands_,(counts[0]+63)/64,1,1);
        if(!submit()){out.code=Code::Failed;out.reason="deko_skin_completion";return out;}
        auto* output=static_cast<const uint8_t*>(dkMemBlockGetCpuAddr(readback_));
        out.positions.resize(q.positions.size());std::memcpy(out.positions.data(),output,positionBytes);
        if(!q.normals.empty()){out.normals.resize(q.normals.size());std::memcpy(out.normals.data(),output+outputNormalOffset,q.normals.size()*4);}
        if(!finite(out.positions)||!finite(out.normals)){out.code=Code::Failed;out.reason="deko_skin_nonfinite";out.positions.clear();out.normals.clear();return out;}
        out.code=Code::Success;out.reason="deko_skin_gpu";return out;
    }
    void close(){
        // A signaled fence and every referenced resource retain their addresses
        // even if a bounded fence wait fails. Drain before freeing in that case.
        if(pending_&&queue_)dkQueueWaitIdle(queue_);
        pending_=false;
        if(commands_)dkCmdBufDestroy(commands_);commands_=nullptr;
        releaseWork();
        release(commandMemory_);release(vertexCode_);release(fragmentCode_);release(skinCode_);
        device_=nullptr;queue_=nullptr;
    }

private:
    static constexpr uint32_t kCommands=1024*1024,kBudget=32*1024*1024;
    struct alignas(16) GeometryUniform {
        float model_view[16],projection[16];uint32_t config[4];float depth_range[4];float texture_matrix[16],clip_transform[16];
    };
    static_assert(sizeof(GeometryUniform)==288,"shared GLSL std140 geometry ABI");
    struct Range {uint32_t first,count,weight,bone;};
    static uint32_t aligned(uint32_t n,uint32_t a=256){return (n+a-1)&~(a-1);}
    static uint32_t argbRgba(uint32_t c){return(c&0xff00ff00u)|((c&255u)<<16)|((c>>16)&255u);}
    static std::string geometryCapability(const m3g_gpu::GeometryRequest& q){
        // The shared order preparation already checks the clipping volume.
        for(size_t i=0;i<q.uv.size();i+=2)for(unsigned r=0;r<2;++r){
            float v=q.texture_matrix[r*4]*q.uv[i]+q.texture_matrix[r*4+1]*q.uv[i+1]+q.texture_matrix[r*4+3];
            // Keep float floor/mod and conversion well defined, and retain
            // texel resolution rather than accepting huge imprecise coordinates.
            if(!std::isfinite(v)||std::fabs(v)>1048576.0f)return "texture_coordinate_range";
        }
        return {};
    }
    void release(DkMemBlock& b){
        if(b){metrics_.resident_bytes-=dkMemBlockGetSize(b);dkMemBlockDestroy(b);}b=nullptr;
    }
    void releaseWork(){release(upload_);release(readback_);release(images_);}
    DkMemBlock memory(uint32_t bytes,uint32_t flags){
        if(pending_||!bytes||bytes>kBudget)return nullptr;
        const uint32_t size=aligned(bytes,DK_MEMBLOCK_ALIGNMENT);
        if(metrics_.resident_bytes+size>kBudget)return nullptr;
        DkMemBlockMaker m;dkMemBlockMakerDefaults(&m,device_,size);m.flags=flags;
        DkMemBlock block=dkMemBlockCreate(&m);
        if(block){++metrics_.allocations;metrics_.resident_bytes+=dkMemBlockGetSize(block);metrics_.peak=std::max(metrics_.peak,metrics_.resident_bytes);}
        return block;
    }
    bool loadShader(const char* path,DkMemBlock& code,DkShader& shader,
                    const j2me_present::gpu_filter::ShaderAsset& pin){
        FILE* f=std::fopen(path,"rb");if(!f)return false;
        bool ok=std::fseek(f,0,SEEK_END)==0;const long size=ok?std::ftell(f):-1;
        if(size<24||size>1024*1024||size!=long(pin.bytes)||std::fseek(f,0,SEEK_SET)!=0){std::fclose(f);return false;}
        code=memory(uint32_t(size)+DK_SHADER_CODE_UNUSABLE_SIZE,
                    DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code);
        void* data=code?dkMemBlockGetCpuAddr(code):nullptr;
        ok=data&&std::fread(data,1,size_t(size),f)==size_t(size);ok=std::fclose(f)==0&&ok;
        if(ok){rms_suite_identity_detail::Sha256 hash;hash.update(static_cast<const uint8_t*>(data),size_t(size));ok=hash.finish()==pin.sha256;}
        if(!ok){release(code);return false;}
        DkShaderMaker maker;dkShaderMakerDefaults(&maker,code,0);dkShaderInitialize(&shader,&maker);
        if(!dkShaderIsValid(&shader)){release(code);return false;}return true;
    }
    bool prepare(bool skin){
        if(pending_||!device_||!queue_||dkQueueIsInErrorState(queue_))return false;
        if(!commands_||!commandMemory_){
            if(commands_)dkCmdBufDestroy(commands_);commands_=nullptr;release(commandMemory_);
            DkCmdBufMaker cm;dkCmdBufMakerDefaults(&cm,device_);commands_=dkCmdBufCreate(&cm);
            commandMemory_=memory(kCommands,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached);
            if(!commands_||!commandMemory_)return false;
            dkCmdBufAddMemory(commands_,commandMemory_,0,kCommands);
        }
        // Shader code also counts against the resident budget. A new stage can
        // evict idle work storage, but never storage referenced by a pending job.
        uint64_t codeBytes=0;
        auto codeSize=[](const j2me_present::gpu_filter::ShaderAsset& p){return aligned(p.bytes+DK_SHADER_CODE_UNUSABLE_SIZE,DK_MEMBLOCK_ALIGNMENT);};
        if(skin){if(!skinCode_)codeBytes+=codeSize(m3g_gpu::shader_assets::comp);}
        else {if(!vertexCode_)codeBytes+=codeSize(m3g_gpu::shader_assets::vert);if(!fragmentCode_)codeBytes+=codeSize(m3g_gpu::shader_assets::frag);}
        if(metrics_.resident_bytes+codeBytes>kBudget){
            if(skin)release(images_); // compute does not use cached render images
            if(metrics_.resident_bytes+codeBytes>kBudget)releaseWork();
        }
        if(skin){if(!skinCode_&&!loadShader("romfs:/shaders/m3g_skin.comp.dksh",skinCode_,skinShader_,m3g_gpu::shader_assets::comp))return false;}
        else {
            if(!vertexCode_&&!loadShader("romfs:/shaders/m3g.vert.dksh",vertexCode_,vertexShader_,m3g_gpu::shader_assets::vert))return false;
            if(!fragmentCode_&&!loadShader("romfs:/shaders/m3g.frag.dksh",fragmentCode_,fragmentShader_,m3g_gpu::shader_assets::frag))return false;
        }
        dkCmdBufClear(commands_);return true;
    }
    bool allocateWork(uint32_t inputBytes,uint32_t outputBytes,uint32_t imageBytes=0){
        if(pending_||!inputBytes||!outputBytes||inputBytes>kBudget||outputBytes>kBudget||imageBytes>kBudget)return false;
        DkMemBlock* blocks[]={&upload_,&readback_,&images_};
        const uint32_t needed[]={aligned(inputBytes,DK_MEMBLOCK_ALIGNMENT),aligned(outputBytes,DK_MEMBLOCK_ALIGNMENT),aligned(imageBytes,DK_MEMBLOCK_ALIGNMENT)};
        uint32_t capacity[3],target[3];uint64_t fixed=metrics_.resident_bytes;
        for(unsigned i=0;i<3;++i){capacity[i]=*blocks[i]?dkMemBlockGetSize(*blocks[i]):0;fixed-=capacity[i];target[i]=std::max(capacity[i],needed[i]);}
        const uint64_t minimum=fixed+needed[0]+needed[1]+needed[2];
        if(minimum>kBudget)return false; // keep existing cache untouched on a budget rejection
        auto total=[&](){return fixed+target[0]+target[1]+target[2];};
        // Evict storage unused by this request before shrinking useful capacity.
        if(total()>kBudget&&!needed[2])target[2]=0;
        for(unsigned i=0;i<3&&total()>kBudget;++i)if(target[i]>needed[i])target[i]=needed[i];
        // Retire every replacement before allocating any new block, so growth
        // cannot transiently exceed the resident budget or invalidate a fence.
        uint64_t reused=0;
        for(unsigned i=0;i<3;++i){if(capacity[i]!=target[i])release(*blocks[i]);else if(needed[i]&&*blocks[i])++reused;}
        const uint32_t flags[]={DkMemBlockFlags_CpuCached|DkMemBlockFlags_GpuCached,
            DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached,DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image};
        // Readback remains CPU-uncached. Reusing its allocation cannot leave
        // stale ARM cache lines after the next GPU completion fence.
        for(unsigned i=0;i<3;++i)if(target[i]&&!*blocks[i]){*blocks[i]=memory(target[i],flags[i]);if(!*blocks[i])return false;}
        metrics_.reuses+=reused;return true;
    }
    bool flush(uint32_t bytes){return dkMemBlockFlushCpuCache(upload_,0,bytes)==DkResult_Success;}
    bool submit(){
        dkCmdBufBarrier(commands_,DkBarrier_Full,DkInvalidateFlags_L2Cache);
        dkCmdBufSignalFence(commands_,&fence_,true);
        dkQueueSubmitCommands(queue_,dkCmdBufFinishList(commands_));pending_=true;
        // Submit alone may retain queued commands on the CPU. Flush before wait.
        dkQueueFlush(queue_);
        bool ok=dkFenceWait(&fence_,1000000000LL)==DkResult_Success;
        if(!ok)dkQueueWaitIdle(queue_);
        pending_=false;return ok&&!dkQueueIsInErrorState(queue_);
    }
    void imageLayout(DkImageLayout& l,uint32_t w,uint32_t h,DkImageFormat format){
        DkImageLayoutMaker m;dkImageLayoutMakerDefaults(&m,device_);
        // ZF32 cannot be a pitch-linear image. DMA performs raw block/linear
        // conversion through DkCopyBuf (no 2D engine or depth-format conversion).
        m.flags=DkImageFlags_UsageRender;m.format=format;m.dimensions[0]=w;m.dimensions[1]=h;
        dkImageLayoutInitialize(&l,&m);
    }
    DkDevice device_=nullptr;DkQueue queue_=nullptr;DkCmdBuf commands_=nullptr;
    DkMemBlock commandMemory_=nullptr,vertexCode_=nullptr,fragmentCode_=nullptr,skinCode_=nullptr;
    DkMemBlock upload_=nullptr,readback_=nullptr,images_=nullptr;
    DkShader vertexShader_={},fragmentShader_={},skinShader_={};DkFence fence_={};bool pending_=false;
    Metrics metrics_;
};
