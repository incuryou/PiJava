#pragma once
#include <cstdint>
#include "m3g_raster_scope.h"
// Main-thread lifecycle bridge. Desktop and disabled render-owner paths are no-ops.
// Hooks never enter Java; a barrier can wait for an uncancellable SDK acquire.
// true means safely quiescent (including a joined failed backend), not success
// of the preceding draw. false forbids VM/window mutation by the caller.
namespace host_render_lifecycle {
inline bool (*barrier_callback)()=nullptr;
inline bool (*epoch_callback)(uint64_t)=nullptr;
inline void set_callbacks(bool (*barrier)(),bool (*epoch)(uint64_t)) {
    barrier_callback=barrier;epoch_callback=epoch;
}
inline bool barrier(m3g_raster_scope::Boundary reason=m3g_raster_scope::Boundary::Lifecycle){return m3g_raster_scope::flush_all(reason)&&(!barrier_callback||barrier_callback());}
inline bool epoch_changed(uint64_t epoch){return !epoch_callback||epoch_callback(epoch);}
}
