#!/usr/bin/env python3
"""Focused actual-Java checks for isolated Timer pacing and legacy timing paths."""
import argparse,json,subprocess,time
from concurrent.futures import ThreadPoolExecutor
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--quick',action='store_true')
p.add_argument('--skip-build',action='store_true',help='use the supplied API classes')
a=p.parse_args();b=build.BUILD;b.mkdir(exist_ok=True)
if not a.skip_build:build.rebuild_java(build.java_compiler(None))
classes=b/'test-classes';classes.mkdir(exist_ok=True)
subprocess.run([str(x) for x in build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes]+[build.ROOT/'tests/java'/f for f in ['TimerPathProbe.java','AutoFpsChecks.java','MixedFpsChecks.java']]],check=True)
puff=b/'timer-puff.o';subprocess.run(['gcc','-O1','-c',str(build.ROOT/'src/puff.c'),'-o',str(puff)],check=True)
def compile_one(item):
    source,name=item;binary=b/name
    r=subprocess.run(['g++','-O1','-std=c++17','-fno-exceptions',str(build.ROOT/'tests'/source),str(puff),'-o',str(binary)],capture_output=True,text=True)
    (b/(name+'-build.log')).write_text(r.stdout+r.stderr)
    print(name,'build_rc',r.returncode,flush=True)
    if r.returncode:raise RuntimeError(r.stderr)
    return binary
with ThreadPoolExecutor(max_workers=2) as pool:
    unit,runner=list(pool.map(compile_one,[('speed_timer_pacing_checks.cpp','TimerIsolation'),('timer_path_integration_checks.cpp','TimerPaths')]))
results=[]
def run(label,argv,timeout=20):
    start=time.monotonic();r=subprocess.run([str(x) for x in argv],cwd=build.ROOT,capture_output=True,text=True,timeout=timeout)
    results.append(dict(case=label,rc=r.returncode,seconds=time.monotonic()-start,argv=[str(x) for x in argv],stdout=r.stdout,stderr=r.stderr))
    (b/'timer-pacing-results.json').write_text(json.dumps(dict(scope='desktop VM actual Java loops; not Switch hardware',results=results),indent=2))
    print(label,'rc',r.returncode,r.stdout.strip(),flush=True)
run('isolation',[unit])
cases=[('TimerPathProbe',m,60) for m in [0,1,2,3,4,5,6,7,8,9,11,12,13,14,15]]
cases += [('TimerPathProbe',m,30) for m in [0,1,4,5,6,13,14,15]]
cases += [('TimerPathProbe',m,0) for m in [0,1,4,5]]
cases += [('AutoFpsChecks',m,60) for m in [0,1,2,6]]+[('MixedFpsChecks',m,60) for m in [3,12]]
cases += [('AutoFpsChecks',m,30) for m in [0,1]]+[('MixedFpsChecks',3,30)]
cases += [('AutoFpsChecks',1,0),('MixedFpsChecks',3,0)]
if a.quick:cases=[('TimerPathProbe',m,60) for m in [0,1,4,6]]+[('AutoFpsChecks',0,60)]
for cls,mode,cap in cases:run(f'{cls}/{mode}/{cap}',[runner,cls,mode,cap,classes])
print('PASS' if all(x['rc']==0 for x in results) else 'FAIL',sum(x['rc']==0 for x in results),'/',len(results),flush=True)
raise SystemExit(any(x['rc'] for x in results))
