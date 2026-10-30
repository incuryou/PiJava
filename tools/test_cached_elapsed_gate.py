#!/usr/bin/env python3
"""Same-scheduler cached elapsed/sleep gate and safety fixtures."""
import argparse,json,subprocess,time,hashlib
from pathlib import Path
import build
p=argparse.ArgumentParser();p.add_argument('--skip-build',action='store_true');p.add_argument('--evidence',type=Path,default=build.BUILD/'cached-elapsed-gate');p.add_argument('--baseline',type=Path);p.add_argument('--slow',action='store_true');a=p.parse_args();a.evidence=a.evidence.resolve();a.evidence.mkdir(parents=True,exist_ok=True)
classes=a.evidence/'classes';classes.mkdir(exist_ok=True);binary=a.evidence/'checks';commands=[]
def hashes():
 return {str(p.relative_to(build.ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for d in ['src','java','romfs/classes'] for p in sorted((build.ROOT/d).rglob('*')) if p.is_file()}
before=hashes();(a.evidence/'source-before.json').write_text(json.dumps(before,indent=2))
def call(cmd):
 cmd=list(map(str,cmd));commands.append(cmd);return subprocess.run(cmd,cwd=build.ROOT,capture_output=True,text=True)
if not a.skip_build:
 macros=['-DJ2ME_TEST_VM="'+str(a.baseline.resolve())+'"','-DCACHED_ELAPSED_BASELINE=1'] if a.baseline else []
 if a.slow:macros+=['-DJ2ME_OPT_FAST_DISPATCH=0']
 for cmd in [build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/CachedElapsedGateFixture.java'],['taskset','-c','5','gcc','-O2','-c',build.ROOT/'src/puff.c','-o',a.evidence/'puff.o'],['taskset','-c','5','g++','-O2','-std=c++17','-fno-exceptions','-DREGRESSION_TIMER_TRIAL=1']+macros+[build.ROOT/'tests/cached_elapsed_gate_checks.cpp',a.evidence/'puff.o','-pthread','-ldl','-o',binary]]:
  r=call(cmd)
  if r.returncode:
   (a.evidence/'build-failure.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);raise SystemExit(r.returncode)
t=time.monotonic();r=call(['taskset','-c','5,6',binary,classes]);(a.evidence/'stdout.log').write_text(r.stdout);(a.evidence/'stderr.log').write_text(r.stderr)
after=hashes();(a.evidence/'source-after.json').write_text(json.dumps(after,indent=2))
(a.evidence/'result.json').write_text(json.dumps({'rc':r.returncode,'seconds':time.monotonic()-t,'commands':commands,'source_api_unchanged':before==after,'scope':'Linux shared scheduler, real Java bytecode, CPU5/6, O2; not physical Switch'},indent=2)+'\n');print(r.stdout);raise SystemExit(r.returncode)
