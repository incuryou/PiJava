#!/usr/bin/env python3
"""Production VM behavior of a bounded rechecked remainder-sleep frame gate."""
import argparse,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--evidence',type=Path,required=True)
p.add_argument('--baseline',type=Path,help='unchanged baseline j2mevm.cpp for behavioral RED')
p.add_argument('--cases',default='0:60,0:30,0:0,1:60,2:60,3:60,4:60,5:60,6:60,7:60,8:60,9:60,10:60')
a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
commands=[]
def execute(argv,timeout=180):
    argv=[str(x) for x in argv];commands.append(argv)
    (a.evidence/'commands.json').write_text(json.dumps(dict(affinity=sorted(os.sched_getaffinity(0)),commands=commands),indent=2)+'\n')
    return subprocess.run(argv,cwd=build.ROOT,capture_output=True,text=True,timeout=timeout)
source=a.baseline.resolve().parent if a.baseline else build.ROOT/'src'
manifest={str(x.relative_to(source)):hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted(source.rglob('*')) if x.is_file()}
(a.evidence/'source-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
fixtures=a.evidence/'fixtures';fixtures.mkdir(exist_ok=True)
for relative in ['tests/RecheckedDeadlineChecks.cpp','tests/java/RecheckedDeadlineProbe.java','tools/test_rechecked_deadline.py']:
    shutil.copyfile(build.ROOT/relative,fixtures/Path(relative).name)
classes=a.evidence/'classes';classes.mkdir(exist_ok=True)
r=execute(['taskset','-c','1']+build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/RecheckedDeadlineProbe.java'])
(a.evidence/'java-build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
puff=a.evidence/'puff.o';r=execute(['taskset','-c','1','gcc','-O1','-c',build.ROOT/'src/puff.c','-o',puff])
if r.returncode:raise SystemExit(r.returncode)
binary=a.evidence/'RecheckedDeadlineChecks'
command=['taskset','-c','1','g++','-O1','-std=c++17','-fno-exceptions']
if a.baseline:command+=['-DJ2ME_TEST_BASELINE=1','-DJ2ME_TEST_VM="'+str(a.baseline.resolve())+'"']
r=execute(command+[build.ROOT/'tests/RecheckedDeadlineChecks.cpp',puff,'-o',binary])
(a.evidence/'build.log').write_text(r.stdout+r.stderr);print('build_rc',r.returncode,flush=True)
if r.returncode:raise SystemExit(r.returncode)
results=[]
for pair in a.cases.split(','):
    mode,cap=map(int,pair.split(':'));start=time.monotonic();r=execute(['taskset','-c','1,2',binary,mode,cap,classes],timeout=20)
    results.append(dict(mode=mode,cap=cap,rc=r.returncode,seconds=time.monotonic()-start,stdout=r.stdout,stderr=r.stderr))
    (a.evidence/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(r.stdout.strip(),flush=True)
raise SystemExit(any(r['rc'] for r in results))
