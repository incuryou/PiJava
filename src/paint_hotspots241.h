#pragma once
#include "vm_rs_fallback.h"
// SPDX-License-Identifier: GPL-3.0-or-later
// Fixed-space checkpoint sampler. No per-bytecode clock or allocation, and no
// Java callbacks, GC roots or yield policy changes. Sampling is biased toward
// methods at existing interpreter safepoints; counts are NOT method CPU times.
namespace paint_hotspots241 {
constexpr size_t Capacity=64;
struct Hit {Method* method=nullptr;Method* caller=nullptr;Method* phase=nullptr;uint64_t count=0;int pc=0;};
struct Snapshot {Hit hits[Capacity];size_t used=0;uint64_t samples=0,dropped=0;int64_t wall_us=0;};
 VMS_RS_NSREF(vms_ref_current, current, Snapshot) VMS_RS_NSREF(vms_ref_pending, pending, Snapshot)
 VMS_RS_NSREF(vms_ref_active, active, bool) VMS_RS_NSREF(vms_ref_have_pending, have_pending, bool)
 VMS_RS_NSREF(vms_ref_paints, paints, uint64_t) VMS_RS_NSREF(vms_ref_slow_paints, slow_paints, uint64_t) VMS_RS_NSREF(vms_ref_total_samples, total_samples, uint64_t) VMS_RS_NSREF(vms_ref_omitted_slow, omitted_slow, uint64_t)
 VMS_RS_NSREF(vms_ref_longest_us, longest_us, int64_t)
static Snapshot worker; // derived diagnostics, cleared on suite/restore reset
static void reset(){worker={};current={};pending={};active=have_pending=false;paints=slow_paints=total_samples=omitted_slow=0;longest_us=0;}
static void begin(){current={};active=true;}
static void sample(const std::vector<Frame>& stack){
    if(!active||stack.empty())return;
    Method* m=stack.back().method;Method* caller=stack.size()>1?stack[stack.size()-2].method:nullptr;
    Method* phase=nullptr;
    // Capture the enclosing M3G operation even when a math/array helper is on
    // top. Limit stack walking; it happens only at existing slice checkpoints.
    for(size_t n=stack.size(),seen=0;n&&seen<32;--n,++seen){Method* p=stack[n-1].method;
        const auto& c=VMS_CL(g_classes)[p->class_id].name;
        if(c=="javax/microedition/m3g/Graphics3D"||c=="javax/microedition/m3g/Texture2D"||
           c=="javax/microedition/m3g/Image2D"||c=="javax/microedition/m3g/Triangle"){phase=p;break;}}
    ++current.samples;++total_samples;
    for(size_t i=0;i<current.used;++i){auto& h=current.hits[i];
        if(h.method==m&&h.caller==caller&&h.phase==phase){++h.count;h.pc=stack.back().pc;return;}}
    if(current.used==Capacity){++current.dropped;return;}
    current.hits[current.used++]={m,caller,phase,1,int(stack.back().pc)};
}
static void sample_worker(const std::vector<Frame>& stack){
    if(stack.empty())return;
    Method* m=stack.back().method;Method* caller=stack.size()>1?stack[stack.size()-2].method:nullptr;
    ++worker.samples;
    for(size_t i=0;i<worker.used;++i){auto& h=worker.hits[i];if(h.method==m&&h.caller==caller){++h.count;h.pc=stack.back().pc;return;}}
    if(worker.used==Capacity){++worker.dropped;return;}
    worker.hits[worker.used++]={m,caller,nullptr,1,stack.back().pc};
}
static void finish(int64_t wall_us){
    if(!active)return;active=false;++paints;current.wall_us=std::max<int64_t>(0,wall_us);
    longest_us=std::max(longest_us,current.wall_us);
    if(current.wall_us<50000)return;
    ++slow_paints;
    if(!have_pending||current.wall_us>pending.wall_us){if(have_pending)++omitted_slow;pending=current;have_pending=true;}
    else ++omitted_slow;
}
static std::string label(Method* m){return m?VMS_CL(g_classes)[m->class_id].name+"."+m->name+m->desc:"none";}
static void report(){
    uint64_t calls=0,fallbacks=0,methods=0;
    for(const auto& c:VMS_CL(g_classes))for(const auto& m:c.methods){calls+=m.read_leaf_calls;fallbacks+=m.read_leaf_fallbacks;if(m.read_leaf_kind==1)++methods;}
    log_line("READLEAF calls="+std::to_string(calls)+" fallbacks="+std::to_string(fallbacks)+" methods="+std::to_string(methods)+" scope=suite_cumulative pure_reads_only=1 exact_bytecode_budget=1");
    std::sort(worker.hits,worker.hits+worker.used,[](const Hit& a,const Hit& b){return a.count>b.count;});
    log_line("WORKERCHECK samples="+std::to_string(worker.samples)+" dropped="+std::to_string(worker.dropped)+" scope=since_report sampling=existing_interpreter_checkpoints not_method_CPU_time=1");
    for(size_t i=0;i<std::min<size_t>(8,worker.used);++i){const auto& h=worker.hits[i];
        log_line("WORKERHOT rank="+std::to_string(i+1)+" samples="+std::to_string(h.count)+" pc="+std::to_string(h.pc)+" method="+label(h.method)+" caller="+label(h.caller));}
    worker={};

    log_line("PAINTCHECK schema=1 paints="+std::to_string(paints)+" slow_paints="+std::to_string(slow_paints)+
        " threshold_us=50000 longest_us="+std::to_string(longest_us)+" samples="+std::to_string(total_samples)+
        " omitted_slow="+std::to_string(omitted_slow)+" scope=suite_cumulative sampling=existing_interpreter_checkpoints not_method_CPU_time=1");
    if(!have_pending)return;
    std::sort(pending.hits,pending.hits+pending.used,[](const Hit& a,const Hit& b){return a.count>b.count;});
    log_line("PAINTSLOW schema=1 wall_us="+std::to_string(pending.wall_us)+" samples="+std::to_string(pending.samples)+
        " overflow_samples="+std::to_string(pending.dropped)+" unique_sites="+std::to_string(pending.used)+
        " selection=longest_since_report native_time=not_attributed logic_fps=unavailable");
    for(size_t i=0;i<std::min<size_t>(8,pending.used);++i){const auto& h=pending.hits[i];
        log_line("PAINTHOT rank="+std::to_string(i+1)+" samples="+std::to_string(h.count)+" pc="+std::to_string(h.pc)+
            " method="+label(h.method)+" caller="+label(h.caller)+" m3g_phase="+label(h.phase)+" scope=checkpoint_samples_not_CPU");}
    pending={};have_pending=false;
}
}
