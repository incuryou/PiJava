#pragma once
#include "render3d/jobs.h"
#include <cstdlib>
#include <memory>

// Main-thread ownership of native image preparation on the existing P7 worker.
namespace native_prep_runtime {
inline std::unique_ptr<render3d::PrepQueue> owned;
inline uint32_t epoch=0;
inline unsigned pause_depth=0;
inline bool exit_registered=false;
inline render3d::PrepQueue* prep_queue(){return owned.get();}
inline void shutdown(){if(owned)owned->close();owned.reset();}
inline void configure(bool requested,uint32_t current_epoch){
    shutdown();epoch=current_epoch;
    if(requested&&p7::pool().enabled()){
        if(!exit_registered){std::atexit(shutdown);exit_registered=true;}
        owned.reset(new render3d::PrepQueue(p7::pool(),{32,16*1024*1024},epoch));
        if(pause_depth)owned->freeze(epoch);
    }
}
// Complete/cancel the dispatcher before P7 may discard old-epoch work.
inline void epoch_changed(uint32_t current_epoch){
    if(owned)owned->reset_epoch(current_epoch);
    epoch=current_epoch;
    if(owned&&pause_depth)owned->freeze(epoch);
}
class Pause {
public:
    Pause(){
        if(pause_depth++==0&&owned){
            if(!owned->freeze(epoch)||owned->drain(epoch)!=render3d::PrepDrainCode::Drained)shutdown();
        }
    }
    ~Pause(){if(--pause_depth==0&&owned)owned->resume(epoch);}
    Pause(const Pause&)=delete;Pause& operator=(const Pause&)=delete;
};
}
