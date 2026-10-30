#!/usr/bin/env python3
"""Optional Happy Farm startup regression using the original external JAR."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jar', type=Path, required=True)
parser.add_argument('--output', type=Path, default=build.BUILD/'farm-startup')
args = parser.parse_args()
output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
# Change the test VM's scheduling budget only. Original game bytes are untouched.
source = (build.ROOT/'src/j2mevm.cpp').read_text()
assert source.count('guard > 250000') == 1
vm = output/'vm-short-slices.cpp'
vm.write_text(source.replace('guard > 250000','guard > 1024'))
puff = output/'puff.o'; binary = output/'replay'
build.run('Compiling startup test decompressor',[build.executable('gcc'),'-O2','-c',build.ROOT/'src/puff.c','-o',puff])
build.run('Compiling startup test with short instruction slices',[build.executable('g++'),'-O0','-std=c++17','-fno-exceptions',
    '-I'+str(build.ROOT/'src'), '-DFARM_VM_SOURCE="'+str(vm)+'"',build.ROOT/'tests/farm_startup_replay.cpp',puff,'-o',binary])
results = []
for mode in ['baseline','compatible','baseline-paint','compatible-paint','after-ready-failure']:
    rms = output/(mode+'-rms'); rms.mkdir(exist_ok=True)
    env = dict(os.environ,J2ME_RMS_DIR=str(rms))
    log = output/(mode+'.log')
    proc = subprocess.run([str(binary),str(args.jar.resolve()),str(log),mode],cwd=build.ROOT,env=env,
                          stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=35)
    (output/(mode+'.stdout')).write_text(proc.stdout)
    lines = log.read_text()
    if mode.startswith('baseline') or mode=='after-ready-failure':
        trace = 'f.paint(Ljavax/microedition/lcdui/Graphics;)V (bci=937)' if mode=='baseline-paint' else 'f.keyPressed(I)V (bci=250)'
        assert proc.returncode != 0 and trace in lines, 'original race / later Java error must reproduce: '+mode+'\n'+lines[-1500:]
    else:
        assert proc.returncode == 0 and 'PASS HappyFarm startup:' in lines and 'EX uncaught' not in lines, lines[-2000:]
    results.append(dict(mode=mode,passed=True,exit_code=proc.returncode,log=log.name,
                        diagnostics=[line for line in lines.splitlines() if 'EX ' in line]))
    print(mode, 'PASS expected visible NPE' if mode.startswith('baseline') or mode=='after-ready-failure' else 'PASS guarded startup')
(output/'results.json').write_text(json.dumps(dict(sha256=hashlib.sha256(args.jar.read_bytes()).hexdigest(),
    scope='Startup-only desktop regression under short interpreter slices; not complete gameplay or Switch performance validation',
    results=results),indent=2)+'\n')
