// render_surface.h — P2.1-c6, contract C09: RenderSurface state machine, P2 scope = ONE state (CPU_VALID).
// GPU_VALID / BOTH_VALID / PENDING_GPU are reserved for P5 (not declared, so nothing can enter them by accident).
// P2 rules: every transition must be CPU_VALID -> CPU_VALID; any other request is counted as illegal and the
// state stays CPU_VALID (never crashes). This header owns NO pixels: VMS_RS(g_fb) stays the CPU buffer, VMS_RS(g_fb_dirty) keeps
// its original meaning, no SurfaceVersion counter and no C08 trace in this batch. Nothing in the paint/present
// path calls it yet; it is the interface P5 will hook into (unit-tested in tests/render_surface_checks.cpp).
#pragma once
#include <cstdint>
namespace j2me_render {
enum class SurfaceState : uint8_t { CPU_VALID = 0 };
struct SurfaceStateMachine {
    SurfaceState state() const noexcept { return state_; }
    // Request a state change. P2: only CPU_VALID is a legal target.
    bool transition(SurfaceState to) noexcept {
        if (to == SurfaceState::CPU_VALID && state_ == SurfaceState::CPU_VALID) { ++transitions_; return true; }
        ++illegal_; state_ = SurfaceState::CPU_VALID; return false;
    }
    bool transition_raw(uint8_t to) noexcept {  // for callers holding a serialized/unknown value (P5 states)
        if (to == uint8_t(SurfaceState::CPU_VALID)) return transition(SurfaceState::CPU_VALID);
        ++illegal_; state_ = SurfaceState::CPU_VALID; return false;
    }
    // CPU access points (P5 will wait for the GPU fence here). P2: pass-through checks only.
    bool begin_cpu_write() noexcept { ++cpu_writes_; return state_ == SurfaceState::CPU_VALID; }
    bool cpu_read() noexcept { ++cpu_reads_; return state_ == SurfaceState::CPU_VALID; }
    // 2D <-> 3D ordering observation hooks (Graphics3D.bindTarget / releaseTarget).
    void on_bind3d() noexcept { ++bind3d_; ++depth3d_; }
    bool on_release3d() noexcept { if (!depth3d_) { ++illegal_; return false; } --depth3d_; ++release3d_; return true; }
    uint64_t illegal_transitions() const noexcept { return illegal_; }
    uint64_t transitions() const noexcept { return transitions_; }
    uint64_t cpu_writes() const noexcept { return cpu_writes_; }
    uint64_t cpu_reads() const noexcept { return cpu_reads_; }
    uint32_t bound3d_depth() const noexcept { return depth3d_; }
private:
    SurfaceState state_ = SurfaceState::CPU_VALID;
    uint64_t illegal_ = 0, transitions_ = 0, cpu_writes_ = 0, cpu_reads_ = 0, bind3d_ = 0, release3d_ = 0;
    uint32_t depth3d_ = 0;
};
static_assert(sizeof(SurfaceState) == 1, "single-byte state");
}  // namespace j2me_render
