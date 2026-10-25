#pragma once
#include <cstdint>

// Shared Switch outer-turn decisions. Java work remains in the VM; these
// predicates only select the existing host branches and presentation order.
namespace p1 {
struct Turn {
    bool gpu=false,dirty=false,heartbeat=false,runnable=false,pending=false;
    bool boundary_ready=false,incomplete=false;
    int64_t last_slice_us=0,nap_ms=0;
};
enum class Work { Ready, Advance, Worker, Idle, GpuScheduled, CpuScheduled };
inline Work decide(const Turn& t) {
    if(t.boundary_ready)return Work::Ready;
    if(t.incomplete || t.pending)return Work::Advance;
    if(!t.gpu && !t.dirty && !t.heartbeat && t.last_slice_us>=1000 && t.runnable)
        return Work::Worker;
    if(t.nap_ms>0)return Work::Idle;
    return t.gpu ? Work::GpuScheduled : Work::CpuScheduled;
}
inline bool second_worker(bool ahead,bool dirty,bool runnable) {
    return !ahead && !dirty && runnable;
}
inline bool speculate(bool in_game,bool exit_sheet,bool runnable,bool ahead,bool allowed) {
    return in_game && !exit_sheet && runnable && !ahead && allowed;
}
inline bool should_show(bool in_game,bool exit_sheet,bool gpu,bool dirty,
                        int cpu_cleared,bool heartbeat,bool incomplete) {
    return (exit_sheet || !in_game || gpu || dirty || cpu_cleared<3 || heartbeat) && !incomplete;
}
}
