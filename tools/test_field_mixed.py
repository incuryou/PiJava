#!/usr/bin/env python3
"""Compile and exercise the two strictly recognized Java static-field gates."""
import argparse,json,subprocess,time
import build
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--skip-build',action='store_true')
args=parser.parse_args()
b=build.BUILD;b.mkdir(exist_ok=True);classes=b/'test-classes';classes.mkdir(exist_ok=True)
compiler=build.java_compiler(None)
build.run('Compile field gate Java fixture',compiler+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/MixedFpsChecks.java'])
puff=b/'field-mixed-puff.o';binary=b/'FieldMixedIntegration'
build.run('Compile decompressor',['gcc','-O2','-c',build.ROOT/'src/puff.c','-o',puff])
if not args.skip_build:build.run('Compile field gate integration',['g++','-O2','-std=c++17','-fno-exceptions',build.ROOT/'tests/field_mixed_integration_checks.cpp',puff,'-o',binary])
unit=b/'FieldMixedGateChecks'
build.run('Compile narrow field gate checks',['g++','-O2','-std=c++17',build.ROOT/'tests/field_time_gate_checks.cpp','-o',unit])
build.run('Run narrow field gate checks',[unit])
rows=[]
for cap in (0,30,60):
 for mode in (14,15,16):
    start=time.monotonic();p=subprocess.run([str(binary),str(classes),str(mode),str(cap),str(build.ROOT/'romfs/classes')],cwd=build.ROOT,capture_output=True,text=True,timeout=30)
    rows.append(dict(mode=mode,cap=cap,passed=p.returncode==0,rc=p.returncode,seconds=time.monotonic()-start,stdout=p.stdout,stderr=p.stderr));print(p.stdout,end='',flush=True)
(b/'field-mixed-results.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)
