#include "../src/runtime_profile.h"
#include <cassert>
#include <iostream>
int main() {
    RuntimeProfile p;
    auto worker=p.begin_worker(100);
    auto paint=p.begin_paint(110);
    auto nested=p.begin_paint(120);
    auto cap=p.begin(RuntimeProfile::Kind::CapWait,125);
    p.end(RuntimeProfile::Kind::CapWait,cap,135);
    auto gc=p.begin(RuntimeProfile::Kind::GC,135);
    p.end(RuntimeProfile::Kind::GC,gc,140);
    p.end_paint(nested,150);p.end_paint(paint,160);
    auto poll=p.begin(RuntimeProfile::Kind::PollWait,165);
    p.end(RuntimeProfile::Kind::PollWait,poll,175);
    auto wait=p.begin(RuntimeProfile::Kind::NativeWait,175);
    p.end(RuntimeProfile::Kind::NativeWait,wait,180);
    gc=p.begin(RuntimeProfile::Kind::GC,180);p.end(RuntimeProfile::Kind::GC,gc,185);
    p.end_worker(worker,200);
    gc=p.begin(RuntimeProfile::Kind::GC,200);p.end(RuntimeProfile::Kind::GC,gc,210);
    auto w=p.take();
    assert(w.worker_gross_us==100 && w.worker_syncpaint_us==50);
    assert(w.worker_capwait_us==10 && w.worker_pollwait_us==10 && w.worker_nativewait_us==5);
    assert(w.worker_paint_blocked_us==10 && w.gc_us==20 && w.worker_gc_us==10 && w.worker_paint_gc_us==5);
    assert(w.gc_count==3 && w.worker_gc_count==2 && w.remaining_wall_us()==30);
    assert(p.take().worker_gross_us==0);
    auto host_wait=p.begin(RuntimeProfile::Kind::CapWait,300);p.end(RuntimeProfile::Kind::CapWait,host_wait,400);
    assert(p.take().worker_capwait_us==0);
    worker=p.begin_worker(500);paint=p.begin_paint(510);gc=p.begin(RuntimeProfile::Kind::GC,515);
    p.reset();
    auto fresh=p.begin_worker(600);
    assert(!p.end_worker(worker,700));assert(!p.end_paint(paint,700));assert(!p.end(RuntimeProfile::Kind::GC,gc,700));
    assert(p.worker_depth==1 && p.paint_depth==0);
    p.end_worker(fresh,650);w=p.take();assert(w.worker_gross_us==50 && w.gc_us==0 && w.worker_syncpaint_us==0);
    {RuntimeProfileScope scope(p,RuntimeProfile::Kind::GC);p.reset();}
    assert(p.take().gc_count==0);
    worker=p.begin_worker(700);auto inner=p.begin_worker(710);p.end_worker(inner,720);p.end_worker(worker,730);
    assert(p.take().worker_gross_us==30);
    std::cout<<"PASS RuntimeProfileChecks wall decomposition, paint union, actual waits, GC overlap and generation guards\n";
}
