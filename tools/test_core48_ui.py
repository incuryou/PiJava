#!/usr/bin/env python3
"""Focused MIDP2 UI contracts on the actual VM; never builds NRO/NSP or rebuilds API."""
import argparse,json,subprocess
import build
p=argparse.ArgumentParser();p.add_argument('--skip-native-build',action='store_true');p.add_argument('--library',default=str(build.ROOT/'romfs/classes'));p.add_argument('--baseline-library');a=p.parse_args()
out=build.BUILD/'core48-ui';classes=out/'classes';contract=out/'contract';classes.mkdir(parents=True,exist_ok=True);contract.mkdir(exist_ok=True)
stubs=sorted((build.ROOT/'tests/fixtures/core48-ui-api').rglob('*.java'))
build.run('Compile independent MIDP2 signature contract',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes','-d',contract]+stubs)
build.run('Compile fixture (contract classes excluded from runtime)',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',str(contract)+':'+str(build.ROOT/'romfs/classes'),'-classpath',str(contract)+':'+str(build.ROOT/'romfs/classes'),'-d',classes,build.ROOT/'tests/java/Core48UiChecks.java',build.ROOT/'tests/java/Core48UiLegacyChecks.java'])
runner=out/'core48_ui_checks';puff=out/'puff.o'
if not a.skip_native_build:
 build.run('Compile test decompressor',['gcc','-O0','-c',build.ROOT/'src/puff.c','-o',puff])
 build.run('Compile actual VM UI harness',['g++','-O0','-std=c++17','-fno-exceptions','-ffunction-sections','-fdata-sections',build.ROOT/'tests/core48_ui_checks.cpp',puff,'-Wl,--gc-sections','-o',runner])
results=[]
for mode in ('focus','strings','custom','lifecycle','edge'):
 r=subprocess.run([str(runner),str(classes),a.library,mode],cwd=build.ROOT,text=True,capture_output=True,timeout=45);results.append(dict(case=mode,exit=r.returncode,stdout=r.stdout,stderr=r.stderr));print(r.stdout+r.stderr,flush=True)
if a.baseline_library:
 for kind in ('string','form'):
  state=out/('fit8-'+kind+'.state');hashes=[]
  for library,op in ((a.baseline_library,'write'),(a.library,'read')):
   mode='legacy-'+kind+'-'+op
   r=subprocess.run([str(runner),str(classes),library,mode,str(state)],cwd=build.ROOT,text=True,capture_output=True,timeout=45);results.append(dict(case=mode,exit=r.returncode,stdout=r.stdout,stderr=r.stderr));print(r.stdout+r.stderr,flush=True)
   if r.returncode==0:hashes.append(r.stdout.strip().split('pixels=')[-1])
  if len(hashes)!=2 or hashes[0]!=hashes[1]:results.append(dict(case='legacy-'+kind+'-pixel-equality',exit=1,stdout='',stderr='baseline/candidate default Form pixels differ'))
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n');raise SystemExit(any(r['exit'] for r in results))
