#!/usr/bin/env python3
"""Actual async Canvas field-period Object.wait fixture, positive and isolation cases."""
import argparse,json,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--case',action='append')
p.add_argument('--skip-compile',action='store_true')
p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
classes=a.evidence/'classes';classes.mkdir(exist_ok=True);binary=a.evidence/'AsyncWaitChecks'
if not a.skip_compile:
    state_binary=a.evidence/'AsyncWaitStateChecks'
    subprocess.run(['g++','-std=c++17',str(build.ROOT/'tests/AsyncWaitStateChecks.cpp'),'-o',str(state_binary)],check=True)
    subprocess.run([str(state_binary)],check=True)
    subprocess.run([str(x) for x in build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/AsyncWaitProbe.java']],check=True)
    puff=a.evidence/'puff.o';subprocess.run(['gcc','-O1','-c',str(build.ROOT/'src/puff.c'),'-o',str(puff)],check=True)
    r=subprocess.run(['g++','-O1','-std=c++17','-fno-exceptions',str(build.ROOT/'tests/AsyncWaitChecks.cpp'),str(puff),'-o',str(binary)],capture_output=True,text=True)
    (a.evidence/'build.log').write_text(r.stdout+r.stderr);print('build_rc',r.returncode,flush=True)
    if r.returncode:raise SystemExit(r.returncode)
results=[]
for case in a.case or ['0/60','0/30','0/0']+[f'{mode}/60' for mode in range(1,20)]:
    mode,cap=map(int,case.split('/'));cmd=[str(binary),str(mode),str(cap),str(classes)]
    start=time.monotonic();r=subprocess.run(cmd,cwd=build.ROOT,capture_output=True,text=True,timeout=15)
    results.append(dict(case=case,argv=cmd,rc=r.returncode,seconds=time.monotonic()-start,stdout=r.stdout,stderr=r.stderr))
    (a.evidence/'results.json').write_text(json.dumps(results,indent=2));print(case,'rc',r.returncode,r.stdout.strip(),flush=True)
raise SystemExit(any(r['rc'] for r in results))
