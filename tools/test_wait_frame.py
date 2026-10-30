#!/usr/bin/env python3
"""Actual Java remainder-wait loops and conservative wait isolation checks."""
import argparse,json,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--case',action='append',help='mode/cap; repeat to select cases')
p.add_argument('--skip-compile',action='store_true')
p.add_argument('--evidence',type=Path,default=build.BUILD/'wait-frame-evidence')
a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
b=build.BUILD;b.mkdir(exist_ok=True);classes=b/'wait-test-classes';classes.mkdir(exist_ok=True)
binary=b/'WaitFrameChecks'
if not a.skip_compile:
    subprocess.run([str(x) for x in build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/WaitFrameProbe.java']],check=True)
    puff=b/'wait-puff.o';subprocess.run(['gcc','-O1','-c',str(build.ROOT/'src/puff.c'),'-o',str(puff)],check=True)
    command=['g++','-O1','-std=c++17','-fno-exceptions',str(build.ROOT/'tests/WaitFrameChecks.cpp'),str(puff),'-o',str(binary)]
    r=subprocess.run(command,capture_output=True,text=True)
    (a.evidence/'build.log').write_text(r.stdout+r.stderr)
    print('build_rc',r.returncode,flush=True)
    if r.returncode:raise SystemExit(r.returncode)
cases=[tuple(map(int,x.split('/'))) for x in a.case] if a.case else [(m,c) for m in range(8) for c in [0,60]]+[(m,30) for m in range(4)]+[(m,60) for m in [8,9,10,12,13,14,15,16,17]]
results=[]
for mode,cap in cases:
    command=[str(binary),str(mode),str(cap),str(classes)];start=time.monotonic()
    r=subprocess.run(command,cwd=build.ROOT,capture_output=True,text=True,timeout=15)
    results.append(dict(case=f'{mode}/{cap}',argv=command,rc=r.returncode,seconds=time.monotonic()-start,stdout=r.stdout,stderr=r.stderr))
    (a.evidence/'results.json').write_text(json.dumps(dict(scope='desktop VM actual Java; not Switch hardware',results=results),indent=2))
    print(f'{mode}/{cap} rc={r.returncode}',r.stdout.strip(),flush=True)
print('PASS' if all(r['rc']==0 for r in results) else 'FAIL',sum(r['rc']==0 for r in results),'/',len(results),flush=True)
raise SystemExit(any(r['rc'] for r in results))
