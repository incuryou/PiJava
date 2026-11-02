#pragma once
#include "switch_gpu_present.h"
#include "render_frame_request.h"
#include "render_owner_queue.h"
#include "p7_worker.h"
#include "m3g_gpu_skin_post.h"

// Constructed on main, but every Dk call (including failure cleanup/destruction)
// happens in Queue's sole core1 owner. No VM/log/menu state is accessed here.
class SwitchGpuOwnerBackend {
public:
    using Frame=render_owner::FrameRequest;
    void set_stage_callback(SwitchGpuPresent::StageCallback callback){presenter_.set_stage_callback(callback);}
    bool init(std::string& error){
        packed_.invalidate();
#ifdef __SWITCH__
        registered_=p7::register_thread_clock(p7::RenderSlot,threadGetCurHandle());
#else
        registered_=p7::register_thread_clock(p7::RenderSlot);
#endif
        if(!registered_){error="render_clock_registration";return false;}
        P7_SCOPE(Present);
        if(!presenter_.init()){error="deko_init";return false;}return true;
    }
    bool process(const Frame& frame,render_owner::Completion& result){
        P7_SCOPE(Present);
        const auto started=std::chrono::steady_clock::now();
        if(frame.kind==Frame::Kind::Geometry){
            if(!frame.geometry||!frame.geometry->result){result.error="geometry_request";return false;}
            auto output=presenter_.execute_geometry(frame.geometry->request);
            const bool ok=output.code!=m3g_gpu::Code::Failed;
            if(output.code==m3g_gpu::Code::Success)++geometry_completed_;else ++geometry_declined_;
            if(!ok)result.error=output.reason;
            snapshot_m3g_metrics();
            result.total_us=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
            frame.geometry->result->complete(std::move(output));return ok;
        }
        if(frame.kind==Frame::Kind::Skin){
            if(!frame.skin||!frame.skin->result){result.error="skin_request";return false;}
            auto output=presenter_.execute_skin(frame.skin->request);
            const bool gpu_completed=output.code==m3g_gpu::Code::Success;
            if(gpu_completed&&frame.skin->request.postprocess&&!m3g_gpu::postprocess_skin(frame.skin->request,output)){
                output.code=m3g_gpu::Code::Unsupported;output.reason="skin_postprocess_range";
            }
            const bool ok=output.code!=m3g_gpu::Code::Failed;
            if(gpu_completed)++skin_completed_;
            if(output.code!=m3g_gpu::Code::Success)++skin_declined_;
            if(!ok)result.error=output.reason;
            snapshot_m3g_metrics();
            result.total_us=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
            frame.skin->result->complete(std::move(output));return ok;
        }
        if(!frame.chrome||frame.chrome->size()!=j2me_present::canvas_pixels){result.error="chrome_snapshot";return false;}
        if(frame.invalidate)presenter_.invalidate_game();
        const uint8_t* game=nullptr;const uint32_t* raw=nullptr;
        if(frame.shader){
            if(frame.invalidate||packed_.needs_upload(frame.key)){
                if(!frame.rgb||!j2me_present::gpu_filter::pack(*frame.rgb,frame.key.w,frame.key.h,frame.key.filter,raw_,frame.adaptive,true)){
                    result.error="raw_snapshot";return false;
                }
                packed_.committed(frame.key);
            }
            raw=raw_.data();
        }else if(frame.layered){
            if(!frame.rgb||!j2me_present::game_rgba(*frame.rgb,frame.key.w,frame.key.h,rgba_,frame.key.filter)){
                result.error="rgb_snapshot";return false;
            }
            game=reinterpret_cast<const uint8_t*>(rgba_.data());
        }
        const bool ok=presenter_.present_layers(reinterpret_cast<const uint8_t*>(frame.chrome->data()),frame.generation,
            game,raw,frame.shader?&frame.key:nullptr,frame.overlay.w>0?&frame.overlay:nullptr);
        const auto timing=presenter_.lastTimings();
        result.acquire_wait_us=timing.acquire_wait_us;result.gpu_wait_us=timing.gpu_wait_us;
        result.upload_us=timing.upload_us;result.frame_upload_bytes=timing.frame_upload_bytes;
        result.chrome_upload_bytes=timing.chrome_upload_bytes;
        result.total_us=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count());
        // Submitted means commands accepted, never a claim of display scanout.
        if(!ok)result.error="deko_present";
        if(ok)++present_submitted_;
        return ok;
    }
    bool finish(std::string& error){P7_SCOPE(PresentWait);if(!presenter_.finish()){error="deko_finish";return false;}return true;}
    void shutdown(){
        packed_.invalidate();
        if(!registered_)return;
        {P7_SCOPE(Present);presenter_.close();snapshot_m3g_metrics();raw_.clear();rgba_.clear();}
        p7::unregister_thread_clock(p7::RenderSlot);
        registered_=false;
    }
    struct Counts {uint64_t presents=0,geometry=0,skin=0,declines=0,allocations=0,reuses=0,resident_bytes=0,peak_bytes=0;};
    Counts counts()const{return {present_submitted_.load(),geometry_completed_.load(),skin_completed_.load(),geometry_declined_.load()+skin_declined_.load(),m3g_allocations_.load(),m3g_reuses_.load(),m3g_resident_.load(),m3g_peak_.load()};}
private:
    std::atomic<uint64_t> present_submitted_{0},geometry_completed_{0},skin_completed_{0},geometry_declined_{0},skin_declined_{0};
    std::atomic<uint64_t> m3g_allocations_{0},m3g_reuses_{0},m3g_resident_{0},m3g_peak_{0};
    void snapshot_m3g_metrics(){const auto m=presenter_.m3g_metrics();m3g_allocations_=m.allocations;m3g_reuses_=m.reuses;m3g_resident_=m.resident_bytes;m3g_peak_=m.peak;}
    SwitchGpuPresent presenter_;
    bool registered_=false;
    std::vector<uint32_t> raw_,rgba_;
    j2me_present::gpu_filter::FrameCache packed_;
};
class SwitchGpuOwner {
public:
    SwitchGpuOwner():queue(backend){}
    SwitchGpuOwnerBackend backend;
    render_owner::Queue<SwitchGpuOwnerBackend> queue;
};
