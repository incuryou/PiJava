#!/usr/bin/env python3
"""Actual Java frame-remainder sleeps under the production desktop scheduler."""
import argparse,json,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',type=Path,help='alternate unchanged j2mevm.cpp for red verification')
p.add_argument('--evidence',type=Path,default=build.BUILD/'residual-sleep-evidence')
a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
classes=build.BUILD/'residual-classes';classes.mkdir(parents=True,exist_ok=True)
subprocess.run([str(x) for x in build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/ResidualSleepProbe.java']],check=True)
puff=a.evidence/'puff.o';subprocess.run(['gcc','-O1','-c',str(build.ROOT/'src/puff.c'),'-o',str(puff)],check=True)
binary=a.evidence/'ResidualSleepChecks'
command=['g++','-O1','-std=c++17','-fno-exceptions']
if a.baseline:command+=['-DJ2ME_TEST_VM="'+str(a.baseline.resolve())+'"']
command+=[str(build.ROOT/'tests/ResidualSleepChecks.cpp'),str(puff),'-o',str(binary)]
r=subprocess.run(command,capture_output=True,text=True);(a.evidence/'build.log').write_text(r.stdout+r.stderr)
print('build_rc',r.returncode,flush=True)
if r.returncode:raise SystemExit(r.returncode)
results=[]
for mode,cap in [(0,60),(3,60),(0,0),(3,0),(0,30),(3,30),(1,60),(2,60)]:
    argv=[str(binary),str(mode),str(cap),str(classes)];start=time.monotonic()
    r=subprocess.run(argv,cwd=build.ROOT,capture_output=True,text=True,timeout=15)
    results.append(dict(mode=mode,cap=cap,argv=argv,rc=r.returncode,seconds=time.monotonic()-start,stdout=r.stdout,stderr=r.stderr))
    (a.evidence/'results.json').write_text(json.dumps(results,indent=2))
    print(r.stdout.strip(),flush=True)
raise SystemExit(any(r['rc'] for r in results))
