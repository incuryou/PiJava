#ifndef J2ME_SWITCH_AUDIO_H
#define J2ME_SWITCH_AUDIO_H

// Main-thread-only PCM output. This object exclusively owns libnx audout.
// API/ownership reference: libnx v4.12.0 services/audout.h and audout.c.
#include <switch.h>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <chrono>
#include "audio_engine.h"
#include "audio_stats.h"
#include "optimization_config.h"

namespace j2me_audio {
class SwitchAudioSink {
public:
    static constexpr size_t BufferFrames = 1024;
    static constexpr size_t BufferCount = 4;
    static constexpr size_t BufferBytes = BufferFrames * 2 * sizeof(int16_t);
    // Each block is 21.33 ms; the complete queue is at most 85.33 ms.
    using Stats = SinkStats;
    using Clock = uint64_t (*)();
private:
    struct Block { AudioOutBuffer descriptor{}; bool queued = false; };
    Block blocks_[BufferCount]{};
    bool initialized_ = false, running_ = false, suspended_ = false;
    Result lastError_ = 0;
    const char* operation_ = "none";
    Stats stats_{};
    static uint64_t steadyMicros() {
        return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    Clock clock_ = steadyMicros;
    uint64_t lastPumpUs_ = 0;
    bool pumpClockValid_ = false;
    static uint64_t elapsed(uint64_t begin, uint64_t end) {
        return end >= begin ? end - begin : 0;
    }

    // Closing IAudioOut returns ownership even when a stop/flush IPC failed.
    // Never free a buffer while its audio service session remains open.
    void releaseResources() {
        if (initialized_) audoutExit();
        initialized_ = running_ = false;
        pumpClockValid_ = false;
        for (auto& b : blocks_) {
            std::free(b.descriptor.buffer);
            b = Block{};
        }
    }
    Result fail(Result rc, const char* operation) {
        ++stats_.errors;
        lastError_ = rc;
        operation_ = operation;
        if (initialized_ && running_) (void)audoutStopAudioOut();
        releaseResources();
        return rc;
    }
    Result collectReleased(bool normalPump = false) {
        // libnx's getter provides space for ONE pointer, not a linked list.
        // Fetch at most our four buffers plus one empty result. No event wait.
        for (size_t n = 0; n <= BufferCount; ++n) {
            AudioOutBuffer* released = nullptr;
            u32 count = 0;
            Result rc = audoutGetReleasedAudioOutBuffer(&released, &count);
            if (R_FAILED(rc)) return fail(rc, "audoutGetReleasedAudioOutBuffer");
            if (!count) return 0;
            bool found = false;
            for (auto& b : blocks_) if (released == &b.descriptor && b.queued) {
                b.queued = false;
                if (normalPump) ++stats_.released_buffers;
                found = true;
                break;
            }
            if (count != 1 || !found)
                return fail(MAKERESULT(Module_Libnx, LibnxError_BadInput), "unexpected released audio buffer");
        }
        return fail(MAKERESULT(Module_Libnx, LibnxError_BadInput), "audio release count exceeded queue");
    }
    Result append(Block& b, Engine* engine) {
        // Released storage is ours again. Always initialize it to silence.
        std::memset(b.descriptor.buffer, 0, BufferBytes);
        if (engine) {
            const uint64_t begin = clock_();
            engine->render(static_cast<int16_t*>(b.descriptor.buffer), BufferFrames);
            const uint64_t mixUs = elapsed(begin, clock_());
            ++stats_.render_blocks;
            stats_.render_frames += BufferFrames;
            stats_.total_mix_us += mixUs;
            if (mixUs > stats_.max_mix_us) stats_.max_mix_us = mixUs;
            const auto* samples = static_cast<const int16_t*>(b.descriptor.buffer);
            uint32_t blockPeak = 0;
            for (size_t n = 0; n < BufferFrames * 2; ++n) {
                const int sample = samples[n];
                const uint32_t magnitude = uint32_t(sample < 0 ? -sample : sample);
                if (magnitude > blockPeak) blockPeak = magnitude;
            }
            if (blockPeak) ++stats_.nonzero_blocks;
            if (blockPeak > stats_.peak_abs_sample) stats_.peak_abs_sample = blockPeak;
        }
        armDCacheFlush(b.descriptor.buffer, BufferBytes);
        Result rc = audoutAppendAudioOutBuffer(&b.descriptor);
        if (R_FAILED(rc)) return fail(rc, "audoutAppendAudioOutBuffer");
        b.queued = true;
        return 0;
    }
    Result fillAndStart(Engine* engine, int maxNew = -1) {
        int added = 0;
        for (auto& b : blocks_) if (!b.queued) {
            if (maxNew >= 0 && added >= maxNew) break;
            Result rc = append(b, engine);
            if (R_FAILED(rc)) return rc;
            ++added;
        }
        if (!running_) {
            bool any = false;
            for (const auto& b : blocks_) if (b.queued) { any = true; break; }
            if (!any) return 0;
            Result rc = audoutStartAudioOut();
            if (R_FAILED(rc)) return fail(rc, "audoutStartAudioOut");
            running_ = true;
            lastPumpUs_ = clock_();
            pumpClockValid_ = true;
        }
        return 0;
    }
    Result clearQueue() {
        // Intentional lifecycle stops must not become a pump-gap warning.
        pumpClockValid_ = false;
        if (running_) {
            // Lifecycle only: Stop synchronizes with playback, so it can wait
            // briefly for the current block. The normal pump never waits.
            Result rc = audoutStopAudioOut();
            if (R_FAILED(rc)) return fail(rc, "audoutStopAudioOut");
            running_ = false;
        }
        bool flushed = false;
        Result rc = audoutFlushAudioOutBuffers(&flushed);
        if (R_FAILED(rc)) return fail(rc, "audoutFlushAudioOutBuffers");
        rc = collectReleased();
        if (R_FAILED(rc)) return rc;
        // A successful flush is not by itself permission to overwrite a
        // descriptor. All appended buffers must have been returned to us.
        for (const auto& b : blocks_) if (b.queued)
            return fail(MAKERESULT(Module_Libnx, LibnxError_BadInput), "audio buffer retained after flush");
        return 0;
    }
public:
    explicit SwitchAudioSink(Clock clock = steadyMicros) : clock_(clock ? clock : steadyMicros) {}
    SwitchAudioSink(const SwitchAudioSink&) = delete;
    SwitchAudioSink& operator=(const SwitchAudioSink&) = delete;
    ~SwitchAudioSink() { (void)shutdown(); }

    Result init() {
        if (initialized_) return 0;
        static_assert(BufferBytes == 0x1000, "audout buffers require page-sized storage");
        static_assert(Engine::SampleRate == 48000, "engine and audout sample rates must match");
        lastError_ = 0;
        operation_ = "none";
        suspended_ = false;
        Result rc = audoutInitialize();
        if (R_FAILED(rc)) return fail(rc, "audoutInitialize");
        initialized_ = true;
        if (audoutGetSampleRate() != 48000 || audoutGetChannelCount() != 2 ||
            audoutGetPcmFormat() != PcmFormat_Int16)
            return fail(MAKERESULT(Module_Libnx, LibnxError_BadInput), "audout PCM format mismatch");
        if (hosversionBefore(4, 0, 0))
            return fail(MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer), "audout flush requires HOS 4.0+");
        for (auto& b : blocks_) {
            b.descriptor.buffer = memalign(0x1000, BufferBytes);
            if (!b.descriptor.buffer)
                return fail(MAKERESULT(Module_Libnx, LibnxError_OutOfMemory), "allocate audout PCM buffers");
            b.descriptor.buffer_size = BufferBytes;
            b.descriptor.data_size = BufferBytes;
        }
        return fillAndStart(nullptr);
    }
    // maxNew < 0 refills every free block. skipRenderIfQueuedAtLeast >= 0 collects
    // finished blocks but does not mix while that many blocks are still queued.
    Result pump(Engine& engine, int maxNew = -1, int skipRenderIfQueuedAtLeast = -1) {
        if (!initialized_ || suspended_) return 0;
        const uint64_t now = clock_();
        ++stats_.pump_count;
        if (pumpClockValid_) {
            const uint64_t gap = elapsed(lastPumpUs_, now);
            if (gap > stats_.max_pump_gap_us) stats_.max_pump_gap_us = gap;
        }
        lastPumpUs_ = now;
        pumpClockValid_ = true;
        const bool hadRunningQueue = running_ && queuedBuffers() != 0;
        Result rc = collectReleased(true);
        if (R_FAILED(rc)) return rc;
        const size_t queued = queuedBuffers();
        if (queued < stats_.min_queued_buffers) stats_.min_queued_buffers = queued;
        if (hadRunningQueue && !queued) ++stats_.queue_empty_polls;
        if (skipRenderIfQueuedAtLeast >= 0 && queued >= (size_t)skipRenderIfQueuedAtLeast) return 0;
        return fillAndStart(&engine, maxNew);
    }
    // Refill the bounded queue when at most two blocks remain. A late frame
    // can consume more than one 21 ms block; replenishing only one leaves
    // the next late frame underfed. Three queued blocks still skip mixing.
    Result pumpFrame(Engine& engine) { return pump(engine, -1, 3); }
    // With at least two known queued descriptors, cache ownership for at
    // most 1 ms; one descriptor may already be partly played. Avoid spinning
    // on IPC thousands of times between block releases.
    // Lifecycle operations invalidate pumpClockValid_. Clock rollback forces
    // a real collection; raw pump()/pumpFrame() remain available for callers.
    Result pumpThrottled(Engine& engine, bool frame = false) {
        if(!initialized_ || suspended_)return 0;
        ++stats_.throttled_calls;
        if(J2ME_OPT_AUDIO_POLL && pumpClockValid_ && running_ && queuedBuffers()>=2) {
            const uint64_t now=clock_();
            if(now>=lastPumpUs_ && now-lastPumpUs_<1000) {
                ++stats_.skipped_release_polls;return 0;
            }
        }
        return frame ? pumpFrame(engine) : pump(engine);
    }
    Result suspend(Engine& engine, bool value) {
        engine.pause(value);
        if (suspended_ == value) return 0;
        suspended_ = value;
        if (!initialized_) return 0;
        return value ? clearQueue() : fillAndStart(&engine);
    }
    // Drop all players and old queued PCM when changing/resetting a VM suite.
    Result reset(Engine& engine) {
        engine.reset();
        engine.pause(suspended_);
        if (!initialized_) return 0;
        Result rc = clearQueue();
        if (R_FAILED(rc) || suspended_) return rc;
        return fillAndStart(nullptr);
    }
    Result shutdown() {
        if (!initialized_) { releaseResources(); return 0; }
        Result rc = clearQueue();
        releaseResources();
        return rc;
    }
    bool available() const { return initialized_; }
    size_t queuedBuffers() const {
        size_t count = 0;
        for (const auto& b : blocks_) if (b.queued) ++count;
        return count;
    }
    const Stats& metrics() const { return stats_; }
    uint64_t lastServiceUs() const { return pumpClockValid_?lastPumpUs_:0; } // Owner-only diagnostic read; no IPC/fence.
    Stats takeStats() {
        const Stats result = stats_;
        stats_ = Stats{};
        return result;
    }
    bool suspended() const { return suspended_; }
    Result lastError() const { return lastError_; }
    const char* errorOperation() const { return operation_; }
};
} // namespace j2me_audio
#endif
