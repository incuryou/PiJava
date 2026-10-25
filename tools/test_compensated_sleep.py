#!/usr/bin/env python3
"""Real Java cross-frame sleep error feedback under the production scheduler."""
import argparse,json,subprocess,os,hashlib,shutil
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--evidence',type=Path,required=True)
p.add_argument('--cases',default='0:60,5:60,0:30,0:0,5:0,2:60,3:60,4:60,6:60,8:60,9:60,10:60')
p.add_argument('--baseline',type=Path)
a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
source=a.baseline.resolve().parent if a.baseline else build.ROOT/'src'
(a.evidence/'source-sha256.json').write_text(json.dumps({str(x.relative_to(source)):hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted(source.rglob('*')) if x.is_file()},indent=2)+'\n')
fixtures=a.evidence/'fixtures';fixtures.mkdir(exist_ok=True)
for relative in ['tests/CompensatedSleepChecks.cpp','tests/java/CompensatedSleepProbe.java','tools/test_compensated_sleep.py']:
    shutil.copyfile(build.ROOT/relative,fixtures/Path(relative).name)
commands=[]
def execute(argv,timeout=180):
    argv=[str(x) for x in argv];commands.append(argv)
    (a.evidence/'commands.json').write_text(json.dumps(dict(affinity=sorted(os.sched_getaffinity(0)),commands=commands),indent=2)+'\n')
    return subprocess.run(argv,cwd=build.ROOT,capture_output=True,text=True,timeout=timeout)
classes=a.evidence/'classes';classes.mkdir(exist_ok=True)
r=execute(['taskset','-c','1']+build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/CompensatedSleepProbe.java'])
(a.evidence/'java-build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
puff=a.evidence/'puff.o';r=execute(['taskset','-c','1','gcc','-O0','-c',build.ROOT/'src/puff.c','-o',puff])
if r.returncode:raise SystemExit(r.returncode)
binary=a.evidence/'CompensatedSleepChecks'
command=['taskset','-c','1','g++','-O0','-std=c++17','-fno-exceptions']
if a.baseline:command+=['-DJ2ME_TEST_BASELINE=1','-DJ2ME_TEST_VM="'+str(a.baseline.resolve())+'"']
r=execute(command+[build.ROOT/'tests/CompensatedSleepChecks.cpp',puff,'-o',binary])
(a.evidence/'build.log').write_text(r.stdout+r.stderr);print('build_rc',r.returncode,flush=True)
if r.returncode:raise SystemExit(r.returncode)
results=[]
for pair in a.cases.split(','):
    mode,cap=map(int,pair.split(':'));r=execute(['taskset','-c','1,2',binary,mode,cap,classes],20)
    results.append(dict(mode=mode,cap=cap,rc=r.returncode,stdout=r.stdout,stderr=r.stderr))
    (a.evidence/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(r.stdout.strip(),flush=True)
raise SystemExit(any(r['rc'] for r in results))
