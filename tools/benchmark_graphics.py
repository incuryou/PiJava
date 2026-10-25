#!/usr/bin/env python3
"""Fixed custom-VM/native/raster paint comparison. Never infer Switch FPS."""
import argparse
import json
from pathlib import Path
import platform
import statistics
import subprocess
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', type=Path, required=True, help='0.1.76 project directory containing src/j2mevm.cpp')
parser.add_argument('--rounds', type=int, default=512)
parser.add_argument('--runs', type=int, default=3)
parser.add_argument('--skip-compile', action='store_true')
args = parser.parse_args()
root=build.ROOT
baseline=args.baseline.resolve()
binaries={'baseline':build.BUILD/'graphics-paint-baseline77', 'optimized':build.BUILD/'graphics-paint-current77'}
if not args.skip_compile:
    build.run('Compiling fixed paint bytecode',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',root/'romfs/classes','-classpath',root/'romfs/classes','-d',build.BUILD/'test-classes',root/'tests/java/GraphicsPaintBenchmark.java'])
    puff=build.BUILD/'graphics-benchmark-puff77.o'
    build.run('Compiling decompressor',[build.executable('gcc'),'-O2','-c',root/'src/puff.c','-o',puff])
    for kind,binary in binaries.items():
        flags=['-DGRAPHICS_BENCH_VM="'+str(baseline/'src/j2mevm.cpp')+'"'] if kind=='baseline' else ['-DGRAPHICS_BENCH_PROFILE']
        build.run('Compiling '+kind+' fixed paint benchmark',[build.executable('g++'),'-O2','-std=c++17','-fno-exceptions']+flags+[root/'tests/graphics_paint_benchmark.cpp',puff,'-o',binary])
raw={'baseline':[],'optimized':[],'optimized_sampled':[]}
orders=[['baseline','optimized','optimized_sampled'],['optimized_sampled','baseline','optimized'],['optimized','optimized_sampled','baseline']]
for run in range(args.runs):
    for kind in orders[run%len(orders)]:
        command=[str(binaries['baseline' if kind=='baseline' else 'optimized']),str(args.rounds)]
        if kind=='optimized_sampled':command.append('--profile')
        raw[kind].append(json.loads(subprocess.run(command,cwd=root,capture_output=True,text=True,timeout=60,check=True).stdout))
summary=[]
for count in (84,433):
    modes={}
    hashes=set()
    for kind,runs in raw.items():
        samples=[s for run in runs for s in run['samples'] if s['image_calls']==count]
        hashes.update(s['pixel_hash'] for s in samples)
        modes[kind]={key:statistics.median(s[key]/args.rounds for s in samples) for key in ('vm_us','native_us','raster_us')}
    if len(hashes)!=1:raise RuntimeError('baseline/optimized/profile pixel hashes differ')
    old=modes['baseline'];new=modes['optimized'];sampled=modes['optimized_sampled']
    summary.append(dict(image_calls=count,clip_calls=count+1,pixel_hash=next(iter(hashes)),pixel_hash_equal=True,medians_per_paint=modes,
        vm_reduction_percent=100*(1-new['vm_us']/old['vm_us']),native_reduction_percent=100*(1-new['native_us']/old['native_us']),
        sampled_vm_reduction_percent=100*(1-sampled['vm_us']/old['vm_us']),sampled_native_reduction_percent=100*(1-sampled['native_us']/old['native_us']),
        sampled_profile_vm_overhead_percent=100*(sampled['vm_us']/new['vm_us']-1),
        per_run_vm_medians_us={kind:[statistics.median(s['vm_us']/args.rounds for s in run['samples'] if s['image_calls']==count) for run in runs] for kind,runs in raw.items()},
        baseline_cost_fraction=dict(raster=old['raster_us']/old['vm_us'],native_excluding_raster=(old['native_us']-old['raster_us'])/old['vm_us'],vm_excluding_native=(old['vm_us']-old['native_us'])/old['vm_us'])))
data=dict(scope='Desktop fixed preloaded-image VM/native/raster clipped alpha-atlas paints; does not predict real-JAR or Switch FPS.',
    baseline='0.1.76 checkpoint',rounds=args.rounds,runs=args.runs,samples_per_mode=args.runs*5,
    workload=dict(target=[240,320],atlas=[128,128],clip=[8,12],alpha='0/128/255',bytecode_unchanged=True),
    changes=['one map lookup per state read instead of count + operator[]','read color only in color-consuming methods','read strokeStyle only for stroke primitives','resolve target pixels only in methods requiring them'],
    profiling=dict(sample_period=64,clock='steady_clock nanoseconds, sampled paints only',image_native_includes_raster=True,default_in_benchmark='disabled'),
    interpretation='Native/raster phase differences approximate the fixed workload cost; timing phases are separate executions and contain host noise.',
    summary=summary,raw=raw,host=dict(system=platform.system(),machine=platform.machine(),compiler=subprocess.run([build.executable('g++'),'--version'],capture_output=True,text=True,check=True).stdout.splitlines()[0]))
output=root/'verification/graphics-performance77.json'
output.write_text(json.dumps(data,indent=2)+'\n')
for item in summary:print(json.dumps(item,indent=2))
print(output)
