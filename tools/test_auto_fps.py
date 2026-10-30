#!/usr/bin/env python3
"""Check actual Java drawings and automatic FPS learning in this interpreter."""
import argparse
import json
import subprocess
import time
import build

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--full',action='store_true',help='run all 24 Native/30/60 timing cases (about 90 seconds)')
parser.add_argument('--skip-build',action='store_true',help='use the already rebuilt bundled Java library')
args=parser.parse_args()
build.BUILD.mkdir(exist_ok=True)
compiler=build.java_compiler(None)
if not args.skip_build:build.rebuild_java(compiler)
classes=build.BUILD/'test-classes'
classes.mkdir(exist_ok=True)
build.run('Compiling automatic FPS Java fixture',compiler+['-source','8','-target','8',
    '-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes',
    '-d',classes,build.ROOT/'tests/java/AutoFpsChecks.java'])
puff=build.BUILD/'auto-fps-puff.o'
build.run('Compiling decompressor',[build.executable('gcc'),'-O2','-c',build.ROOT/'src/puff.c','-o',puff])
results=[]
for name,source in [('AutoFpsUnit','auto_fps_checks.cpp'),('AutoFpsIntegration','auto_fps_integration_checks.cpp')]:
    binary=build.BUILD/name
    build.run('Compiling '+name,[build.executable('g++'),'-O2','-std=c++17','-fno-exceptions',
        build.ROOT/'tests'/source]+([puff] if name=='AutoFpsIntegration' else [])+['-o',binary])
    arguments=[] if args.full or name=='AutoFpsUnit' else ['60','5']
    before=time.monotonic()
    result=subprocess.run([str(binary)]+arguments,cwd=build.ROOT,capture_output=True,text=True,timeout=140)
    results.append(dict(test=name,passed=result.returncode==0,seconds=round(time.monotonic()-before,3),stdout=result.stdout,stderr=result.stderr))
    print(result.stdout,end='')
    if result.stderr:print(result.stderr,end='')
output=build.BUILD/'auto-fps-results.json'
output.write_text(json.dumps(dict(version=build.VERSION,full=args.full,scope='desktop interpreter; completed Java drawings',results=results),indent=2)+'\n')
print('Results:',output)
raise SystemExit(0 if all(item['passed'] for item in results) else 1)
