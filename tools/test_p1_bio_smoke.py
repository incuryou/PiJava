#!/usr/bin/env python3
"""Queue advancement smoke with the unmodified exact user Bio JAR; no device verdict."""
import argparse,json,subprocess,time,hashlib,os
from pathlib import Path
import build
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--jar',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
a.jar=a.jar.resolve();a.out=a.out.resolve();a.out.parent.mkdir(parents=True,exist_ok=True)
expected='95f4e836247c5d0f00c7056c59cd80d57e0706e1a5159f4f39cb9656597f37bd';assert hashlib.sha256(a.jar.read_bytes()).hexdigest()==expected
rows=[]
def run(name,cmd,timeout=180):
 t=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=build.ROOT,capture_output=True,text=True,timeout=timeout)
 rows.append(dict(name=name,command=list(map(str,cmd)),returncode=p.returncode,elapsed_s=time.monotonic()-t,stdout=p.stdout,stderr=p.stderr))
 a.out.write_text(json.dumps(dict(build_id=build.VERSION,passed=all(x['returncode']==0 for x in rows) and len(rows)==4,
    jar_sha256=expected,jar_unchanged=hashlib.sha256(a.jar.read_bytes()).hexdigest()==expected,
    scope='exact Bio queue progress; shared production dispatch; simulated host I/O, desktop CPU',
    device_test='not_run',gameplay_reached=None,performance_comparable=None,results=rows),ensure_ascii=False,indent=2)+'\n')
 print(name,p.returncode,p.stdout,flush=True)
 if p.returncode:print(p.stderr);raise SystemExit(1)
puff=build.BUILD/'p1-bio-puff.o';binary=build.BUILD/'p1-bio-smoke'
run('puff',['gcc','-O2','-c',build.ROOT/'src/puff.c','-o',puff]);run('compile',['g++','-O2','-std=c++17','-fno-exceptions',build.ROOT/'tests/p1_bio_smoke.cpp',puff,'-ldl','-o',binary])
for profile in ['P1-R','P1-C']:
 os.environ['J2ME_RMS_DIR']=str(a.out.parent/'desktop-rms'/profile)
 run(profile,[binary,a.jar,build.ROOT/'profiles'/profile/'p1-profile.cfg',a.out.parent/(profile+'-bio-desktop.log')])
