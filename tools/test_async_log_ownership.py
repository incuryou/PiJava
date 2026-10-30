#!/usr/bin/env python3
"""Deterministic actual-p1_flush and drain ownership checks with blocked xsputn."""
import argparse,hashlib,json,os,shutil,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args();a.evidence=a.evidence.resolve()
if hasattr(os,'sched_setaffinity'):
    os.sched_setaffinity(0,set(sorted(os.sched_getaffinity(0))[-2:]))
a.evidence.mkdir(parents=True,exist_ok=True)
manifest={str(f.relative_to(build.ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted((build.ROOT/'src').rglob('*')) if f.is_file()}
(a.evidence/'source-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
for relative in ['tests/async_log_ownership_checks.cpp','tools/test_async_log_ownership.py']:
    shutil.copyfile(build.ROOT/relative,a.evidence/Path(relative).name)
commands=[]
def run(argv,timeout=180):
    argv=[str(v) for v in argv];commands.append(argv)
    (a.evidence/'commands.json').write_text(json.dumps(dict(affinity=sorted(os.sched_getaffinity(0)),commands=commands),indent=2)+'\n')
    return subprocess.run(argv,cwd=build.ROOT,capture_output=True,text=True,timeout=timeout)
puff=a.evidence/'puff.o';r=run(['gcc','-O1','-c',build.ROOT/'src/puff.c','-o',puff])
if r.returncode:raise SystemExit(r.returncode)
binary=a.evidence/'AsyncLogOwnershipChecks'
r=run(['g++','-O1','-std=c++17','-fno-exceptions',build.ROOT/'tests/async_log_ownership_checks.cpp',puff,'-pthread','-o',binary])
(a.evidence/'build.log').write_text(r.stdout+r.stderr);print('build_rc',r.returncode,flush=True)
if r.returncode:raise SystemExit(r.returncode)
start=time.monotonic();r=run([binary,a.evidence],timeout=15)
(a.evidence/'results.json').write_text(json.dumps(dict(affinity=sorted(os.sched_getaffinity(0)),rc=r.returncode,seconds=time.monotonic()-start,stdout=r.stdout,stderr=r.stderr),indent=2)+'\n')
print(r.stdout,flush=True);raise SystemExit(r.returncode)
