// Timer frames have their own evidence. Never certify them into auto_fps:
// its verified gate belongs to the existing clock-polling classifier.
static bool timer_field_long(int ref,const char* name,int64_t& value) {
    if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return false;
    const auto& o=VMS_HEAP(g_heap)[ref];if(o.cls<0||o.is_array)return false;
    const Field* f=find_field(o.cls,name,"J");
    if(!f||f->slot<0||size_t(f->slot)>=o.fields.size())return false;
    value=as_long(o.fields[size_t(f->slot)]);return true;
}
static bool timer_drawing_task(JavaThreadContext& t,TimerFrameTask& out) {
    if(!VMS_CL(g_class_index).count("java/util/TimerQueue"))return false;
    auto inspect=[&](const std::vector<Frame>& frames) {
        for(size_t i=1;i<frames.size();++i) {
            const auto& parent=frames[i-1];const auto& task=frames[i];
            if(!parent.method||!task.method||parent.locals.empty()||task.locals.empty()||
               parent.method->name!="run"||parent.method->desc!="()V"||
               VMS_CL(g_classes)[parent.method->class_id].name!="java/util/TimerQueue"||
               task.method->name!="run"||task.method->desc!="()V")continue;
            const int q=parent.locals[0].ref,r=task.locals[0].ref;
            if(q<=0||r<=0||size_t(q)>=VMS_HEAP(g_heap).size()||size_t(r)>=VMS_HEAP(g_heap).size()||
               !type_compatible(r,"java/util/TimerTask"))continue;
            int64_t period=0,scheduled=0;
            if(!timer_field_long(r,"period",period)||!timer_field_long(r,"scheduledTime",scheduled)||period==INT64_MIN)return false;
            out={q,r,VMS_HEAP(g_heap)[q].generation,VMS_HEAP(g_heap)[r].generation,period<0?-period:period,scheduled};return true;
        }
        return false;
    };
    if(inspect(t.frames))return true;
    for(const auto& root:VMS_HEAP(g_root_stacks))if(root.second==&t&&root.first&&root.first!=&t.frames&&inspect(*root.first))return true;
    return false;
}
static void speed_timer_release(JavaThreadContext& t) {
    t.timer.clear_proof();
    if(g_timer_clock_owner==&t) {
        VMS_EXEC(g_game_clock).rate(paint_clock_us(),1.0);g_timer_clock_owner=nullptr;
    }
}
static void speed_timer_block(JavaThreadContext& t) {
    TimerFrameTask call;
    if(timer_drawing_task(t,call)){t.timer.enter(call);t.timer.blocked=true;}
    speed_timer_release(t);
}
static void speed_timer_expire_clock() {
    if(!g_timer_clock_owner)return;
    JavaThreadContext* live=g_timer_clock_owner==&VMS_EXEC(g_main_thread)?g_timer_clock_owner:nullptr;
    if(!live)for(auto& t:VMS_EXEC(g_threads))if(&t==g_timer_clock_owner){live=&t;break;}
    if(!live) {g_timer_clock_owner=nullptr;VMS_EXEC(g_game_clock).rate(paint_clock_us(),1.0);return;}
    if(live->state==ThreadState::Blocked||live->state==ThreadState::Joining||live->state==ThreadState::Terminated||
       paint_clock_us()-live->timer.draw_us>std::max<int64_t>(120000,int64_t(speed_period_us())*4))speed_timer_release(*live);
}
static bool timer_task_queued(const TimerFrameTask& task,int64_t& next_due) {
    if(task.queue<=0||task.task<=0||size_t(task.queue)>=VMS_HEAP(g_heap).size()||size_t(task.task)>=VMS_HEAP(g_heap).size())return false;
    const auto& q=VMS_HEAP(g_heap)[task.queue];const auto& tk=VMS_HEAP(g_heap)[task.task];
    if(q.generation!=task.queue_generation||tk.generation!=task.task_generation||q.cls<0||tk.cls<0||q.is_array||tk.is_array||
       VMS_CL(g_classes)[q.cls].name!="java/util/TimerQueue")return false;
    const Field* tasks=find_field(q.cls,"tasks","[Ljava/util/TimerTask;");const Field* count=find_field(q.cls,"count","I");
    const Field* cancelled=find_field(tk.cls,"cancelled","Z");
    if(!tasks||!count||!cancelled||tasks->slot<0||count->slot<0||cancelled->slot<0||
       size_t(tasks->slot)>=q.fields.size()||size_t(count->slot)>=q.fields.size()||
       size_t(cancelled->slot)>=tk.fields.size()||tk.fields[size_t(cancelled->slot)].i)return false;
    int64_t period=0;if(!timer_field_long(task.task,"period",period)||period==INT64_MIN||
        (period<0?-period:period)!=task.period_ms||task.period_ms<8||task.period_ms>120)return false;
    const int ar=q.fields[size_t(tasks->slot)].ref,n=q.fields[size_t(count->slot)].i;
    if(ar<=0||size_t(ar)>=VMS_HEAP(g_heap).size()||n<=0||size_t(n)>VMS_HEAP(g_heap)[ar].elems.size())return false;
    bool found=false;for(int i=0;i<n;++i)if(VMS_HEAP(g_heap)[ar].elems.get(size_t(i)).ref==task.task)found=true;
    return found&&timer_field_long(VMS_HEAP(g_heap)[ar].elems.get(0).ref,"nextTime",next_due);
}
static bool speed_timer_helper(JavaThreadContext& t) {
    if(!speed_period_us()||!t.timer.group||t.timer.stable<5)return false;
    TimerFrameTask active;
    if(paint_clock_us()-t.timer.group_us>std::max<int64_t>(120000,int64_t(speed_period_us())*4)) {
        speed_timer_release(t);return false;
    }
    if(!timer_drawing_task(t,active)||!t.timer.draw.same_call(active))return false;
    if(t.sleep_paced||t.frame_sleep_bridge||t.auto_fps.polling_cycle(VMS_EXEC(g_game_clock).value_us(paint_clock_us()),VMS_EXEC(g_game_clock).scale)) {
        speed_timer_release(t);return false;
    }
    return &t==VMS_EXEC(g_speed_budget_owner);
}
static double speed_timer_drawing(JavaThreadContext& t,int64_t now,bool legacy_timing) {
    TimerFrameTask active;
    if(!speed_period_us()||!timer_drawing_task(t,active)||active.period_ms<8||active.period_ms>120) {
        speed_timer_release(t);t.timer={};return 0;
    }
    const bool completed=t.timer.draw.same_task(active)&&!t.timer.draw.same_call(active)&&!t.timer.blocked&&
        t.timer.drawn!=t.timer.consumed&&paint_clock_us()-t.timer.draw_us<=std::max<int64_t>(120000,int64_t(speed_period_us())*4);
    t.timer.enter(active);
    if(legacy_timing||t.timer.blocked) {speed_timer_release(t);return 0;}
    const bool same=t.timer.draw.same_task(active);
    // A returned callback followed by this task's next scheduled invocation is
    // also a real Timer boundary. Multiple paints or cap waits may have made
    // the queue overdue, so requiring Object.wait would never bootstrap it.
    if(completed&&!t.timer.fresh_wait) {
        t.timer.consumed=t.timer.drawn;t.timer.fresh_wait=true;
        if(t.timer.stable<1000000)++t.timer.stable;
    }
    double target=0;
    if(same&&t.timer.fresh_wait) {
        t.timer.fresh_wait=false;
        if(t.timer.stable>=5) {
            t.timer.group=true;t.timer.group_us=now;
            // A live legacy clock remains authoritative, including another
            // drawing thread. Timer evidence cannot replace its learner.
            if(g_timer_clock_owner==&t||(!g_timer_clock_owner&&VMS_EXEC(g_game_clock).scale<=1.01)) {
                target=std::max(1.0,std::min(8.0,active.period_ms*1000.0/speed_period_us()));g_timer_clock_owner=&t;
            }
        }
    } else if(!t.timer.draw.same_call(active)) speed_timer_release(t);
    if(!same)t.timer.stable=0;
    t.timer.draw=active;t.timer.drawn=t.owned_draws;t.timer.draw_us=now;
    return target;
}
static int64_t speed_timer_wait_ms(int queue_ref,int64_t timeout_ms,JavaThreadContext& t) {
    t.wake_us=0;t.timer.queue_wait=false;
    int64_t head_due=0;
    TimerFrameTask active;
    if(!speed_period_us()||timeout_ms<=0||timeout_ms>120||r4::state.active||queue_ref!=t.timer.draw.queue||
       t.timer.blocked||timer_drawing_task(t,active)||!timer_task_queued(t.timer.draw,head_due)||
       paint_clock_us()-t.timer.draw_us>std::max<int64_t>(120000,int64_t(speed_period_us())*4)) {
        speed_timer_block(t);return timeout_ms;
    }
    if(t.timer.drawn!=t.timer.consumed&&t.frame_due_us) {
        t.frame_due_us=0;t.timer.consumed=t.timer.drawn;t.timer.fresh_wait=true;
        if(t.timer.stable<1000000)++t.timer.stable;
        ++g_frame_sleep_stats.timer_paced;
    }
    t.timer.queue_wait=true;
    const double scale=VMS_EXEC(g_game_clock).scale;
    if(t.timer.stable>=5&&scale>1.01&&g_timer_clock_owner==&t&&head_due<=INT64_MAX/1000) {
        const int64_t now=paint_clock_us(),game=VMS_EXEC(g_game_clock).value_us(now);
        const int64_t left=std::max<int64_t>(1,int64_t(std::ceil(double(head_due*1000-game)/scale)));
        t.wake_us=now+left;++g_frame_sleep_stats.timer_scaled;
        return std::max<int64_t>(1,(left+999)/1000);
    }
    return timeout_ms;
}
