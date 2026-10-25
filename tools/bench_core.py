#!/usr/bin/env python3
"""Build identical Java workload for two VMs; alternate sequential measurements."""
import argparse,hashlib,json,subprocess,statistics
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--build-only',action='store_true');p.add_argument('--skip-build',action='store_true');p.add_argument('--repeats',type=int,default=5);args=p.parse_args()
root=build.ROOT;out=build.BUILD/'core-bench';out.mkdir(parents=True,exist_ok=True);classes=out/'classes';classes.mkdir(exist_ok=True)
def run(cmd,**kw):return subprocess.run([str(x) for x in cmd],cwd=root,check=True,**kw)
variants=[('baseline34',args.baseline.resolve()),('optimized35',root)]
if not args.skip_build:
    run(build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',root/'romfs/classes','-classpath',root/'romfs/classes','-d',classes,root/'benchmarks/OptimizationBench.java'])
    for name,source in variants:
        run(['gcc','-O2','-c',source/'src/puff.c','-o',out/(name+'.o')])
        run(['g++','-O2','-std=c++17','-fno-exceptions','-DBENCH_VM="'+str(source/'src/j2mevm.cpp')+'"','-DBENCH_ROM="'+str(root/'romfs/classes')+'"',root/'benchmarks/core_probe.cpp',out/(name+'.o'),'-o',out/name])
    (out/'build-manifest.json').write_text(json.dumps({'sources':{name:{str(p.relative_to(source/'src')):hashlib.sha256(p.read_bytes()).hexdigest() for p in (source/'src').rglob('*') if p.is_file()} for name,source in variants},'class_sha256':hashlib.sha256((classes/'OptimizationBench.class').read_bytes()).hexdigest(),'binaries':{name:hashlib.sha256((out/name).read_bytes()).hexdigest() for name,_ in variants}},indent=2)+'\n')
if args.build_only:raise SystemExit()
results=[]
for mode,iterations in [(0,80000),(1,16000),(2,100000),(3,8000)]:
    for repeat in range(args.repeats):
        order=variants if repeat%2==0 else list(reversed(variants))
        pair=[]
        for name,_ in order:
            proc=run([out/name,mode,iterations,classes,name],capture_output=True,text=True,timeout=120)
            row=json.loads(proc.stdout);row['repeat']=repeat+1;results.append(row);pair.append(row)
            print(json.dumps(row),flush=True)
            (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        if pair[0]['checksum']!=pair[1]['checksum']:raise RuntimeError('semantic checksum divergence: '+str(pair))
summary=[]
for mode in range(4):
    stats={name:statistics.median(r['process_cpu_ms'] for r in results if r['mode']==mode and r['variant']==name) for name,_ in variants}
    summary.append({'mode':mode,'median_cpu_ms':stats,'speedup':stats['baseline34']/stats['optimized35'],'cpu_reduction_percent':100*(1-stats['optimized35']/stats['baseline34'])})
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
