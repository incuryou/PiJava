#!/usr/bin/env python3
"""Chapter RMS import checks. Uses disposable fixtures; never reads user saves."""
import argparse,json,subprocess,tempfile,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build/chapter-save')
p.add_argument('--sanitize',action='store_true')
p.add_argument('--vm',action='store_true',help='compile and run the real Java RMS integration')
p.add_argument('--ui',action='store_true')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
assert (ROOT/'src/chapter_save.h').exists(), 'chapter-save import is not implemented'
rows=[]
def run(name,argv,**kw):
 start=time.monotonic();r=subprocess.run([str(x) for x in argv],capture_output=True,text=True,**kw)
 (out/(name+'.log')).write_text(r.stdout+r.stderr)
 rows.append(dict(name=name,argv=[str(x) for x in argv],rc=r.returncode,seconds=round(time.monotonic()-start,3)))
 (out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
 print(name,'PASS' if r.returncode==0 else 'FAIL',flush=True)
 if r.returncode: print((r.stdout+r.stderr)[-4000:]);raise SystemExit(r.returncode)
flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if a.sanitize else ['-O2']
binary=out/'checks'
run('compile',['g++','-std=c++17',*flags,ROOT/'tests/chapter_save_checks.cpp','-Wl,--wrap=rename','-Wl,--wrap=fwrite','-o',binary])
for mode in ['posix','nonreplace']:
 with tempfile.TemporaryDirectory(prefix='chapter-rms-') as d:run(mode,[binary,Path(d)/'rms',mode])

if a.ui:
 assert (ROOT/'src/chapter_save_ui.h').exists(), 'chapter-save UI is not implemented'
 ui=out/'ui'
 run('ui-compile',['g++','-std=c++17',*flags,ROOT/'tests/chapter_save_ui_checks.cpp','-o',ui])
 with tempfile.TemporaryDirectory(prefix='chapter-ui-') as d:run('ui',[ui,d])
if a.vm:
 import build
 from test_rms_isolation import fixture_jar
 with tempfile.TemporaryDirectory(prefix='chapter-vm-') as d:
  d=Path(d);classes=d/'classes';classes.mkdir()
  run('java',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',ROOT/'romfs/classes','-classpath',ROOT/'romfs/classes','-d',classes,ROOT/'tests/java/RmsIsolationChecks.java'])
  jars=[d/'chapter-a.jar',d/'chapter-b.jar']
  for path,resource in zip(jars,[b'chapter A',b'chapter B']):fixture_jar(path,classes,resource)
  puff=out/'puff.o';vm=out/'vm'
  run('puff',['gcc',*flags,'-c',ROOT/'src/puff.c','-o',puff])
  run('vm-compile',['g++','-std=c++17','-fno-exceptions',*flags,ROOT/'tests/chapter_save_vm_checks.cpp',puff,'-o',vm])
  run('vm',[vm,d/'rms',classes,ROOT/'romfs/classes',*jars])
