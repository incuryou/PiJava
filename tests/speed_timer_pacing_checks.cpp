#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <iostream>
static int checks=0;
static void check(bool ok,const char* why){++checks;if(!ok){std::cerr<<"FAIL TimerIsolation "<<why<<"\n";std::exit(1);}}
static void set(int ref,const char* name,const char* desc,Value value){auto& o=VMS_HEAP(g_heap)[ref];const auto* f=find_field(o.cls,name,desc);check(f!=nullptr,"field present");o.fields[size_t(f->slot)]=value;}
int main(){
    VMS_CL(g_classpath)={"romfs/classes"};VMS_DIAG(g_log_draw)=false;
    VMS_EXEC(g_speed_cap)=60;speed_sync_clock();
    const int qc=ensure_class("java/util/TimerQueue"),tc=ensure_class("java/util/TimerTask");
    const int q=alloc_obj(qc),task=alloc_obj(tc),background=alloc_obj(tc),arr=alloc_array("[Ljava/util/TimerTask;",2);
    set(q,"tasks","[Ljava/util/TimerTask;",Value::of_ref(arr));set(q,"count","I",Value::of_i(2));
    set(task,"period","J",Value::of_l(-41));set(task,"nextTime","J",Value::of_l(game_current_ms()+41));
    set(background,"period","J",Value::of_l(-10));set(background,"nextTime","J",Value::of_l(game_current_ms()+10));
    VMS_HEAP(g_heap)[arr].elems.set(0,Value::of_ref(background));VMS_HEAP(g_heap)[arr].elems.set(1,Value::of_ref(task));
    TimerFrameTask drawn{q,task,VMS_HEAP(g_heap)[q].generation,VMS_HEAP(g_heap)[task].generation,41,100};
    int64_t due=0;
    check(timer_task_queued(drawn,due),"drawing task can differ from head");
    check(drawn.period_ms==41,"background head never replaces drawing period");
    set(task,"cancelled","Z",Value::of_i(1));check(!timer_task_queued(drawn,due),"cancelled task loses qualification");set(task,"cancelled","Z",Value::of_i(0));
    for(int step:{0,5,200}){set(task,"period","J",Value::of_l(step));check(!timer_task_queued(drawn,due),"period change invalidates qualification");}
    set(task,"period","J",Value::of_l(41));
    auto stale=drawn;++stale.task_generation;check(!timer_task_queued(stale,due),"reused task identity rejected");
    stale=drawn;++stale.queue_generation;check(!timer_task_queued(stale,due),"reused queue identity rejected");
    VMS_EXEC(g_threads).emplace_back();auto& owner=VMS_EXEC(g_threads).back();VMS_EXEC(g_running_thread)=&owner;current_thread_ref();
    owner.timer.draw=drawn;owner.timer.drawn=owner.timer.consumed=1;owner.timer.stable=4;owner.timer.draw_us=paint_clock_us();
    check(speed_timer_wait_ms(q,9,owner)==9&&owner.timer.stable==4,"no drawing cannot manufacture another proof");
    check(!owner.auto_fps.verified_gate_seen,"Timer proof does not enter legacy learner");
    owner.timer.stable=5;owner.timer.draw_us=paint_clock_us()-200000;g_timer_clock_owner=&owner;VMS_EXEC(g_game_clock).rate(paint_clock_us(),2.46);
    check(speed_timer_wait_ms(q,9,owner)==9&&owner.timer.stable==0&&VMS_EXEC(g_game_clock).scale==1,"stale proof releases its clock");
    owner.timer.draw=drawn;owner.timer.stable=5;owner.timer.draw_us=paint_clock_us();
    const int object=alloc_obj(ensure_class("java/lang/Object"));
    check(speed_timer_wait_ms(object,40,owner)==40&&owner.timer.stable==0,"ordinary Object.wait remains real time");
    owner.state=ThreadState::Waiting;owner.monitor=q;owner.reacquire=true;owner.restore_depth=1;
    owner.wake=monotonic_ms()-1;owner.wake_us=paint_clock_us()+100000;
    refresh_thread(owner);check(owner.state==ThreadState::Waiting,"millisecond rounding cannot wake Timer early");
    owner.wake_us=paint_clock_us()-1;refresh_thread(owner);check(owner.state==ThreadState::Runnable&&owner.wake_us==0,"Timer wakes at real deadline");
    owner.timer.stable=3;speed_pause_clock(true);check(owner.timer.stable==0&&!g_timer_clock_owner,"pause clears provisional proof");speed_pause_clock(false);
    JavaThreadContext peer;VMS_EXEC(g_monitors)[q].owner=&peer;VMS_EXEC(g_monitors)[q].depth=1;
    owner.timer.stable=5;g_timer_clock_owner=&owner;VMS_EXEC(g_game_clock).rate(paint_clock_us(),2.46);
    check(!monitor_enter(q),"monitor contention blocks Timer owner");
    VMS_EXEC(g_can_suspend)=true;suspend_current(false);VMS_EXEC(g_can_suspend)=false;
    check(!g_timer_clock_owner&&VMS_EXEC(g_game_clock).scale==1,"monitor blocking releases Timer clock before another drawer takes over");
    VMS_EXEC(g_monitors)[q].owner=nullptr;owner.state=ThreadState::Runnable;VMS_EXEC(g_yield)=false;
    owner.timer.stable=5;g_timer_clock_owner=&owner;VMS_EXEC(g_game_clock).rate(paint_clock_us(),2.46);
    finish_thread(owner);check(!g_timer_clock_owner&&VMS_EXEC(g_game_clock).scale==1,"terminated owner releases Timer clock");
    g_timer_clock_owner=&owner;stop_thread();
    check(!g_timer_clock_owner,"stop clears runtime-only Timer owner before heap/thread replacement");
    VMS_EXEC(g_running_thread)=nullptr;
    std::cout<<"PASS TimerIsolation "<<checks<<" checks\n";
}
