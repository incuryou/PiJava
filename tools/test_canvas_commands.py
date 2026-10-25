#!/usr/bin/env python3
"""Exercise Canvas commands through the shared host dispatch_key path."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import build

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--evidence',type=Path,required=True)
p.add_argument('--skip-build',action='store_true')
p.add_argument('--cpu',default='0')
args=p.parse_args()
args.evidence.mkdir(parents=True,exist_ok=True)
classes=args.evidence/'classes';classes.mkdir(exist_ok=True)
results=[]
def execute(label,argv,expected_code=0,expected_marker=''):
    argv=[str(a) for a in argv]
    proc=subprocess.run(argv,cwd=build.ROOT,capture_output=True,text=True)
    passed=proc.returncode==expected_code and expected_marker in proc.stdout+proc.stderr
    results.append(dict(test=label,argv=argv,returncode=proc.returncode,expected_code=expected_code,passed=passed,stdout=proc.stdout,stderr=proc.stderr))
    (args.evidence/(label+'.log')).write_text(proc.stdout+proc.stderr)
    print(label,proc.returncode,proc.stdout[-2000:],flush=True)
    return passed
binary=args.evidence/'canvas-command-checks';puff=args.evidence/'puff.o'
if not args.skip_build:
    commands=[('java',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/CanvasCommandChecks.java']),
              ('puff',['taskset','-c',args.cpu,'gcc','-O1','-c',build.ROOT/'src/puff.c','-o',puff]),
              ('native',['taskset','-c',args.cpu,'g++','-O1','-std=c++17','-fno-exceptions',build.ROOT/'tests/canvas_command_checks.cpp',puff,'-ldl','-pthread','-o',binary])]
    for label,cmd in commands:
        if not execute(label,cmd):break
if args.skip_build or all(r['passed'] for r in results):
    execute('checks',['taskset','-c',args.cpu,binary,classes])
    execute('throwing-listener',['taskset','-c',args.cpu,binary,classes,'throw'],1,'uncaught java/lang/RuntimeException command callback')
manifest={str(f.relative_to(build.ROOT/'romfs/classes')):hashlib.sha256(f.read_bytes()).hexdigest() for f in (build.ROOT/'romfs/classes').rglob('*.class')}
(args.evidence/'results.json').write_text(json.dumps(dict(results=results,java_api=manifest),indent=2)+'\n')
sys.exit(0 if results and all(r['passed'] for r in results) else 1)
