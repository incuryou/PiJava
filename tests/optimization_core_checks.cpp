#include "../src/frame_buffer_pool.h"
#include "../src/execution_profile.h"
#include "../src/frame_metrics.h"
#include "../src/rgb_fill.h"
#include "../src/runtime_profile.h"
#include "../src/native_class_dispatch.h"
#include "../src/poll_gate.h"
#include <cassert>
#include <random>
#include <thread>
#include <chrono>
#include <iostream>
static int64_t fake_cpu=0;
static int64_t cpu(){return fake_cpu;}
int main() {
    FrameBufferPool<uint64_t,4,1024> pool;
    std::vector<uint64_t> a,b;
    pool.acquire(a,32);a.assign(32,123);auto old=a.data();pool.release(a);
    assert(a.empty() && a.capacity()==0 && pool.retained_bytes()<=1024);
    pool.acquire(b,32);assert(b.empty() && b.capacity()>=32);
#if J2ME_OPT_FRAME_POOL
    assert(b.data()==old && pool.hits==1);
#endif
    b.assign(32,0);for(auto x:b)assert(x==0);
    pool.release(b);pool.acquire(a,2000);a.assign(2000,7);pool.release(a);
    assert(pool.retained_bytes()<=1024 && a.capacity()==0);
    for(int i=0;i<2000;++i){pool.acquire(a,1+(i%80));a.assign(1+(i%80),i);pool.release(a);assert(pool.retained_bytes()<=1024);}
    pool.clear();assert(pool.retained_bytes()==0);
    // Exercise all 64 slot bits, non-power capacities, empty/full lists and
    // active caller buffers across randomized nested acquire/release lifetimes.
    FrameBufferPool<uint64_t,64,8192> fuzz_pool;
    std::array<std::vector<uint64_t>,96> active;
    std::mt19937 pool_rng(0x233);
    for(int i=0;i<50000;++i) {
        auto& v=active[pool_rng()%active.size()];
        if(pool_rng()%3==0) {fuzz_pool.release(v);assert(v.empty()&&!v.capacity());}
        else {
            const size_t need=1+pool_rng()%300;
            fuzz_pool.acquire(v,need);assert(v.empty()&&v.capacity()>=need);
            v.assign(need,uint64_t(i));
        }
        assert(fuzz_pool.retained_bytes()<=8192);
    }
    for(auto& v:active)fuzz_pool.release(v);
    fuzz_pool.clear();assert(!fuzz_pool.retained_bytes());
    ExecutionProfile profile;profile.enabled=true;profile.cpu_clock=cpu;
    MethodCpuCounters outer,inner;
    fake_cpu=10;profile.activate(&outer);fake_cpu=20;
    {ExecutionProfile::Context scope(profile,&inner);fake_cpu=45;}
    fake_cpu=60;profile.activate(nullptr);
    assert(outer.self_cpu_ns==25 && inner.self_cpu_ns==25);
    fake_cpu=70;{ExecutionProfile::Context scope(profile,&outer);profile.reset();fake_cpu=100;}
    assert(profile.active()==nullptr && outer.self_cpu_ns==25);
    fake_cpu=-1;profile.activate(&outer);fake_cpu=-1;profile.activate(nullptr);assert(profile.clock_failures==1);
    RuntimeProfile runtime;runtime.cpu_clock=cpu;
    fake_cpu=100;auto w=runtime.begin_worker(0);
    fake_cpu=120;auto p=runtime.begin_paint(10);
    fake_cpu=125;auto g=runtime.begin(RuntimeProfile::Kind::GC,12);
    fake_cpu=135;runtime.end(RuntimeProfile::Kind::GC,g,20);
    fake_cpu=160;runtime.end_paint(p,30);
    fake_cpu=200;runtime.end_worker(w,1000);
    auto rw=runtime.take();assert(rw.worker_cpu_ns==100 && rw.paint_cpu_ns==40 && rw.gc_cpu_ns==10);
    assert(rw.worker_paint_cpu_ns==40 && rw.worker_gc_cpu_ns==10 && rw.worker_paint_gc_cpu_ns==10);
    assert(rw.cpu_samples==3 && !rw.cpu_unavailable && rw.worker_gross_us==1000);
    fake_cpu=-1;w=runtime.begin_worker(0);runtime.end_worker(w,999);assert(runtime.take().cpu_unavailable==1);
    FrameMetrics frames;std::vector<uint8_t> rgb(30,1);
    frames.record(1000,rgb);frames.record(17000,rgb);rgb[29]=2;frames.record(34000,rgb);
    auto f=frames.take();assert(f.completed_draws==3 && f.content_frames==2 && f.unchanged_draws==1);
    assert(f.draw_intervals.count==2 && f.draw_intervals.quantile_upper(95)>=17000);
    frames.record(50000,rgb);assert(frames.take().unchanged_draws==1);
    frames.reset();frames.record(100,rgb);assert(frames.take().content_frames==1);
    frames.content_enabled=false;frames.record(200,rgb);assert(frames.take().content_unavailable==1);
    DurationHistogram h;for(unsigned i=0;i<100;++i)h.add(i*1000);h.add(500000);
    assert(h.quantile_upper(50)>=50000 && h.quantile_upper(50)<=50250 && h.max_us==500000);
    assert(h.quantile_upper(100)==500000 && h.overflow==1);
    std::mt19937 rng(0x233);unsigned cases=0;
    for(int i=0;i<12000;++i) {
        int width=1+rng()%97,height=1+rng()%67,padding=rng()%20;
        size_t stride=width*3+padding,bytes=stride*height+32;
        std::vector<uint8_t> actual(bytes,0xA5),expected=actual;
        uint8_t r=rng(),g2=rng(),b2=rng();if(i%4==0)g2=b2=r;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){size_t off=16+y*stride+x*3;expected[off]=r;expected[off+1]=g2;expected[off+2]=b2;}
        rgb_fill::rectangle(actual.data()+16,stride,width,height,r,g2,b2);assert(actual==expected);++cases;
    }
    PollGateState gate;gate.waiting=true;gate.certified=true;gate.reads=4;gate.deadline_ms=103;
    assert(gate.wait_us(100,3,false)==0 && gate.wait_us(100,3,true)==500);
    assert(gate.wait_us(102,3,true)==83 && gate.wait_us(103,3,true)==0);
    assert(gate.wait_us(100,0,true)==0 && gate.wait_us(100,std::numeric_limits<double>::infinity(),true)==0);
    assert(gate.wait_us(100,std::numeric_limits<double>::quiet_NaN(),true)==0);
    gate.certified=false;assert(gate.wait_us(100,3,true)==0);
    const auto before=j2me_cpu_clock::now_ns();std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto after=j2me_cpu_clock::now_ns();assert(before<0 || after>=before);
    assert(native_class_id("not/a/native/class")==NativeClassId::Other);
    std::cout<<"PASS OptimizationCore bounded frame buffers, 12000 exact RGB differentials, CPU overlap/unavailability, profiler generations and content/quantiles\n";
}
