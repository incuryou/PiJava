#!/usr/bin/env python3
"""Execute production host decisions/core with injected clock and I/O, not device FPS."""
from pathlib import Path
import subprocess
import build


def main():
    vm = (build.ROOT/'src/j2mevm.cpp').read_text()
    start = vm.index('template<class Schedule,class Sleep>\nstatic p1::Work p1_host_work_turn(')
    runtime = vm[start:vm.index('\nstatic void p1_host_post_present(', start)]
    common = r'''#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include "p1_host_turn.h"
#include "frame_schedule.h"
#define VMS_PLAT(x) x
#define VMS_EXEC(x) x
#define VMS_RS(x) x
struct CityHostSpan {CityHostSpan(const char*){}};
static int64_t now=100000,g_last_present_us=100000,g_last_slice_us=0;
static int g_slice_ms=4;static bool g_fb_dirty=false,runnable=false,pending=false,boundary=false,incomplete=false;
static uint64_t g_present_frames=0;
static int advances=0,naps=0,shows=0;static int64_t nap=0,delay=0,waited=0;
static int64_t g_frame_work_finished_us=0,g_frame_work_us=0;
static FrameWaitBudget g_frame_wait_budget;static WakeStats g_wake_stats;
static bool game_runnable(){return runnable;}
static bool p1_pending_work(){return pending;}
static bool host_frame_ready242(){return boundary;}
static bool speed_group_incomplete(){return incomplete;}
static int64_t idle_sleep_ms(){return nap;}
static void p1_host_decision(int,const p1::Turn&){}
static void p1_flush(const char*){}
static void audio_pump_frame(){}
static void advance_game_frame(){++advances;}
static bool speed_ahead(){return false;}
static void speed_wait_paced_sleep(){}
static int64_t paint_clock_us(){return now;}
static int64_t game_wake_delay(){return delay;}
static void wait_game_wake(int64_t ms){waited+=ms;now+=ms*1000;}
static void p1_host_post_present(bool,bool){}
static void log_line(const std::string&){}
'''
    body = r'''
static int failures=0,checks=0;
static void check(bool ok,const char* why){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<why<<'\n';}}
static p1::Work old_cpu(const p1::Turn& t){
 if(t.boundary_ready)return p1::Work::Ready;
 if(t.incomplete||t.pending)return p1::Work::Advance;
 if(!t.dirty&&!t.heartbeat&&t.last_slice_us>=1000&&t.runnable)return p1::Work::Worker;
 if(t.nap_ms>0)return p1::Work::Idle;
 return p1::Work::CpuScheduled;
}
static p1::Work normalized(p1::Work w){return w==p1::Work::GpuScheduled?p1::Work::CpuScheduled:w;}
int main(){
 // Pin all CPU branch boundaries against gpuf3, and require backend-neutral policy.
 for(unsigned bits=0;bits<64;++bits)for(int64_t slice:{0,999,1000,15000})for(int64_t n:{0,1,8}){
  p1::Turn t{false,bool(bits&1),bool(bits&2),bool(bits&4),bool(bits&8),bool(bits&16),bool(bits&32),slice,n};
  check(p1::decide(t)==old_cpu(t),"CPU work policy unchanged");
  t.gpu=true;check(normalized(p1::decide(t))==old_cpu(t),"GPU may run clean workers before blocking display");
 }
 for(unsigned bits=0;bits<32;++bits)for(int clear:{0,2,3,4}){
  bool in=bits&1,exit=bits&2,dirty=bits&4,heart=bits&8,inc=bits&16;
  const bool expected=(exit||!in||dirty||clear<3||heart)&&!inc;
  check(p1::should_show(in,exit,false,dirty,clear,heart,inc)==expected,"CPU present policy unchanged");
  check(p1::should_show(in,exit,true,dirty,clear,heart,inc)==expected,"GPU must not present unchanged game except heartbeat/chrome");
 }
 // Execute the real runtime dispatch: GPU must also honor ordinary idle naps.
 for(bool gpu:{false,true}){
  runnable=pending=boundary=incomplete=g_fb_dirty=false;nap=5;naps=advances=0;
  auto work=p1_host_work_turn(gpu,false,[](bool){++advances;},[](int64_t n){++naps;assert(n==5);});
  check(work==p1::Work::Idle&&naps==1&&advances==0,"actual runtime GPU idle sleeps without a display submission");
  // Execute actual scheduler core; acquire before deadline read and bounded wait.
  nap=0;delay=3;waited=0;advances=0;g_frame_wait_budget={};g_frame_wait_budget.observe(8000,500);
  g_last_slice_us=0;
  sched_game_work(gpu,[]{now+=16000;},[](int64_t){});
  check(waited==3&&advances==1,"both backend cores use bounded near-wake bridge");
  delay=0;shows=0;g_last_present_us=now;g_fb_dirty=false;g_frame_work_finished_us=0;
  check(!sched_present_turn(true,false,gpu,3,[]{++shows;})&&shows==0,"actual core avoids empty GPU submission");
  g_fb_dirty=true;check(sched_present_turn(true,false,gpu,3,[]{++shows;})&&shows==1,"actual core always submits a new game frame");
  g_fb_dirty=false;g_last_present_us=now-50000;
  check(sched_present_turn(true,false,gpu,3,[]{++shows;}),"50ms heartbeat is retained");
  incomplete=true;check(!sched_present_turn(true,false,gpu,0,[]{++shows;}),"incomplete paint group remains hidden");incomplete=false;
 }
 std::cout<<"policy checks="<<checks<<" failures="<<failures<<" (clock/I/O model, not device FPS)\n";
 return failures?1:0;
}
'''
    out=build.BUILD/'gpu-scheduling';out.mkdir(parents=True,exist_ok=True)
    cpp=out/'policy.cpp';cpp.write_text(common+runtime+'\n#include "scheduler_core.inc"\n'+body)
    exe=out/'policy'
    subprocess.run(['g++','-std=c++17','-O1','-I'+str(build.ROOT/'src'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    host=(build.ROOT/'src/platform/switch_host.inc').read_text()
    # The actual Switch binding must acquire the GPU slot before Java when scheduled.
    assert 'sched_game_work(VMS_PLAT(g_gpu_active),acquire_game_frame,' in host
    assert 'scheduled_frame(acquire_game_frame,game_wake_delay,wait_game_wake,advance_game_frame,' in host
    # CPU needs three buffer clear passes; the GPU has one canonical chrome
    # canvas plus per-slot generations. A cached canvas must not leave this at
    # one forever, or the shared policy still submits every idle turn.
    begin=host.index('        if(ok) {',host.index('static void present()'))
    end=host.index('        log_line("RENDER GPU submission',begin)
    success=host[begin:end]
    warm=out/'chrome.cpp'
    warm.write_text(common+r'''
static int g_cpu_cleared=1;
static void complete(uint64_t acquire=0,uint64_t early=0){const bool ok=true;const int64_t done=now;
struct {uint64_t acquire_wait_us=0,acquire_before_present_us=0;} timing{acquire,early};
'''+success+r'''
}
int main(){complete();assert(g_cpu_cleared>=3&&"GPU canonical chrome must finish warmup");
assert(!p1::should_show(true,false,true,false,g_cpu_cleared,false,false));
now=10000;g_frame_work_finished_us=1000;g_frame_work_us=8000;
complete(6000,0);assert(g_frame_wait_budget.recent_submit_us()==3000);
assert(g_frame_work_finished_us==0&&g_frame_wait_budget.recent_work_us()==8000);
g_frame_wait_budget={};g_frame_work_finished_us=7000;
complete(6000,6000);assert(g_frame_wait_budget.recent_submit_us()==3000);
}
''')
    exe=out/'chrome'
    subprocess.run(['g++','-std=c++17','-O1','-I'+str(build.ROOT/'src'),str(warm),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    begin=host.index('static void acquire_game_frame()')
    acquire=host[begin:host.index('\n}\n',begin)+3]
    cpp=out/'fallback.cpp'
    cpp.write_text(r'''#include <cassert>
#include <string>
#define VMS_PLAT(x) x
static bool g_gpu_active=false,acquire_ok=true;
static int gpu_calls=0,cpu_calls=0,closes=0;
struct Gpu{bool acquire_frame(){++gpu_calls;return acquire_ok;}};
static Gpu g_gpu_present;
static void log_line(const std::string&){}
static void close_presenter(){++closes;g_gpu_active=false;}
static void acquire_cpu_frame(){assert(!g_gpu_active);++cpu_calls;}
'''+acquire+r'''
int main(){acquire_game_frame();assert(cpu_calls==1&&gpu_calls==0);
#ifdef J2ME_ENABLE_DEKO3D
g_gpu_active=true;acquire_game_frame();assert(gpu_calls==1&&cpu_calls==1&&closes==0);
acquire_ok=false;acquire_game_frame();assert(gpu_calls==2&&cpu_calls==2&&closes==1&&!g_gpu_active);
acquire_game_frame();assert(gpu_calls==2&&cpu_calls==3&&closes==1);
#endif
}
''')
    for gpu in (False,True):
        exe=out/('fallback-gpu' if gpu else 'fallback-cpu')
        subprocess.run(['g++','-std=c++17']+(['-DJ2ME_ENABLE_DEKO3D'] if gpu else [])+[str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    print('PASS production policy/core/runtime + CPU-equivalence matrix; scheduled Switch hook uses backend frame acquisition')


if __name__=='__main__':main()
