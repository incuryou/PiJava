#!/usr/bin/env python3
"""Regress mixed sleep/clock pacing without changing the existing FPS policy."""
import argparse,json,subprocess,time
import build
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--skip-build',action='store_true',help='use the bundled Java API classes')
parser.add_argument('--quick',action='store_true',help='unit checks and four mixed cases at 60 FPS')
args=parser.parse_args()
b=build.BUILD;b.mkdir(exist_ok=True);classes=b/'test-classes';classes.mkdir(exist_ok=True)
compiler=build.java_compiler(None)
if not args.skip_build:build.rebuild_java(compiler)
build.run('Compile mixed Java fixture',compiler+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/MixedFpsChecks.java'])
puff=b/'mixed-fps-puff.o';binary=b/'MixedFpsIntegration'
build.run('Compile decompressor',['gcc','-O2','-c',build.ROOT/'src/puff.c','-o',puff])
build.run('Compile mixed FPS integration',['g++','-O2','-std=c++17','-fno-exceptions',build.ROOT/'tests/mixed_fps_integration_checks.cpp',puff,'-o',binary])
cases=[('unit','0')]+[(str(mode),str(cap)) for cap,modes in ([(60,[3,4,6,7,12,13])] if args.quick else [(0,[3,4,6,7]),(30,[3,4,6,7,8,9,10,11,12,13]),(60,[3,4,6,7,8,9,10,11,12,13])]) for mode in modes]
results=[]
for mode,cap in cases:
    start=time.monotonic();cmd=[str(binary),str(classes),mode,cap,str(build.ROOT/'romfs/classes')]
    p=subprocess.run(cmd,cwd=build.ROOT,capture_output=True,text=True,timeout=30)
    row=dict(mode=mode,cap=cap,passed=p.returncode==0,raw_rc=p.returncode,seconds=time.monotonic()-start,stdout=p.stdout,stderr=p.stderr)
    results.append(row);print(p.stdout,end='',flush=True)
output=b/'mixed-fps-results.json';output.write_text(json.dumps(dict(version=build.VERSION,quick=args.quick,results=results),indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
