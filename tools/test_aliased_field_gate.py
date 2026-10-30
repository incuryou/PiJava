#!/usr/bin/env python3
"""Run real Java alias/static clock gates; --baseline preserves a RED replay."""
import argparse,json,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',type=Path)
p.add_argument('--evidence',type=Path,default=build.BUILD/'aliased-field-evidence')
p.add_argument('--skip-build',action='store_true')
p.add_argument('--mode',type=int)
p.add_argument('--opt',default='-O0')
a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
classes=build.BUILD/'aliased-field-classes';classes.mkdir(parents=True,exist_ok=True)
binary=a.evidence/'AliasedFieldGateChecks'
if not a.skip_build:
 subprocess.run([str(x) for x in build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/AliasedFieldGateProbe.java']],check=True)
 puff=a.evidence/'puff.o';subprocess.run(['gcc',a.opt,'-c',str(build.ROOT/'src/puff.c'),'-o',str(puff)],check=True)
 cmd=['g++',a.opt,'-std=c++17','-fno-exceptions']
 if a.baseline:cmd+=['-DJ2ME_TEST_VM="'+str(a.baseline.resolve())+'"']
 cmd +=[str(build.ROOT/'tests/AliasedFieldGateChecks.cpp'),str(puff),'-pthread','-ldl','-o',str(binary)]
 r=subprocess.run(cmd,capture_output=True,text=True);(a.evidence/'build.log').write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
unit=a.evidence/'AliasedFieldTimeGateChecks'
if not a.skip_build:
 subprocess.run(['g++',a.opt,'-std=c++17',str(build.ROOT/'tests/aliased_field_time_gate_checks.cpp'),'-o',str(unit)],check=True)
 subprocess.run([str(unit)],check=True)
rows=[]
for mode,cap in ([(a.mode,60)] if a.mode is not None else [(0,60),(0,0),(0,30),(2,60),(4,60),(1,60),(3,60),(5,60),(6,60),(7,60),(8,60),(9,60)]):
 cmd=[str(binary),str(mode),str(cap),str(classes)];started=time.monotonic()
 r=subprocess.run(cmd,cwd=build.ROOT,capture_output=True,text=True,timeout=15)
 rows.append(dict(mode=mode,cap=cap,rc=r.returncode,seconds=time.monotonic()-started,stdout=r.stdout,stderr=r.stderr))
 (a.evidence/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print(r.stdout.strip(),r.stderr.strip(),flush=True)
raise SystemExit(any(r['rc'] for r in rows))
