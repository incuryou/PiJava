#pragma once
#include <cstdint>

// Weak heap identities. They are checked against generations before use and
// never become GC roots or part of a snapshot.
struct TimerFrameTask {
    int queue=0,task=0;
    uint32_t queue_generation=0,task_generation=0;
    int64_t period_ms=0,scheduled_ms=0;
    bool same_task(const TimerFrameTask& b) const {
        return queue && queue==b.queue && task==b.task && queue_generation==b.queue_generation &&
            task_generation==b.task_generation && period_ms==b.period_ms;
    }
    bool same_call(const TimerFrameTask& b) const {return same_task(b)&&scheduled_ms==b.scheduled_ms;}
};
struct TimerFramePacing {
    TimerFrameTask call,draw;
    uint64_t drawn=0,consumed=0;
    int64_t draw_us=0,group_us=0;
    unsigned stable=0;
    bool blocked=false,group=false,fresh_wait=false,queue_wait=false;
    void enter(const TimerFrameTask& next) {
        if(!call.same_call(next)){call=next;blocked=false;group=false;group_us=0;}
    }
    void clear_proof(){stable=0;group=false;fresh_wait=false;queue_wait=false;group_us=0;}
};
