#!/usr/bin/env python3
"""Real TimerTask -> Canvas helper residual sleeps, plus existing residual cases."""
import argparse,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--vm',type=Path,help='alternate j2mevm.cpp, such as unchanged source-v2')
p.add_argument('--evidence',type=Path,required=True)
p.add_argument('--helper-only',action='store_true')
p.add_argument('--cpu',type=int,help='pin this test and its child processes to one CPU')
a=p.parse_args()
if a.cpu is not None:os.sched_setaffinity(0,{a.cpu})
a.evidence.mkdir(parents=True,exist_ok=True)
source=a.vm.resolve().parent if a.vm else build.ROOT/'src'
manifest={str(x.relative_to(source)):hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted(source.rglob('*')) if x.is_file()}
(a.evidence/'source-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
fixtures=a.evidence/'fixtures';fixtures.mkdir(exist_ok=True)
for relative in ['tests/ResidualSleepChecks.cpp','tests/java/ResidualSleepProbe.java','tools/test_timer_residual_isolation.py']:
    shutil.copyfile(build.ROOT/relative,fixtures/Path(relative).name)
commands=[]
def execute(argv,timeout=180):
    argv=[str(x) for x in argv];commands.append(argv)
    (a.evidence/'commands.json').write_text(json.dumps(dict(affinity=sorted(os.sched_getaffinity(0)),commands=commands),indent=2)+'\n')
    return subprocess.run(argv,cwd=build.ROOT,capture_output=True,text=True,timeout=timeout)
classes=a.evidence/'classes';classes.mkdir(exist_ok=True)
r=execute(build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/ResidualSleepProbe.java'])
(a.evidence/'java-build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
puff=a.evidence/'puff.o';r=execute(['gcc','-O1','-c',build.ROOT/'src/puff.c','-o',puff])
if r.returncode:raise SystemExit(r.returncode)
binary=a.evidence/'ResidualSleepChecks'
command=['g++','-O1','-std=c++17','-fno-exceptions']
if a.vm:command+=['-DJ2ME_TEST_VM="'+str(a.vm.resolve())+'"']
command+=[build.ROOT/'tests/ResidualSleepChecks.cpp',puff,'-o',binary]
r=execute(command);(a.evidence/'build.log').write_text(r.stdout+r.stderr)
print('build_rc',r.returncode,'affinity',sorted(os.sched_getaffinity(0)),flush=True)
if r.returncode:raise SystemExit(r.returncode)
cases=[(4,c) for c in [0,30,60]]
if not a.helper_only:cases=[(0,60),(3,60),(0,0),(3,0),(0,30),(3,30),(1,60),(2,60)]+cases
results=[]
for mode,cap in cases:
    argv=[binary,str(mode),str(cap),classes];start=time.monotonic();r=execute(argv,timeout=20)
    results.append(dict(mode=mode,cap=cap,rc=r.returncode,seconds=time.monotonic()-start,stdout=r.stdout,stderr=r.stderr))
    (a.evidence/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(r.stdout.strip(),flush=True)
print('PASS' if all(r['rc']==0 for r in results) else 'FAIL',sum(r['rc']==0 for r in results),'/',len(results),flush=True)
raise SystemExit(any(r['rc'] for r in results))
