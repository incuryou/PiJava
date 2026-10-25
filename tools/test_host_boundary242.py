#!/usr/bin/env python3
"""Exercise Switch's outer event/work/present order without physical HID/GPU."""
import argparse,json,os,subprocess,time
from pathlib import Path
import build

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sanitize',action='store_true')
    ap.add_argument('--out',type=Path)
    a=ap.parse_args();root=build.ROOT;build.BUILD.mkdir(exist_ok=True)
    out=a.out or build.BUILD/('host-boundary242-'+('san' if a.sanitize else 'opt')+'.json')
    out.parent.mkdir(parents=True,exist_ok=True);rows=[]
    def run(name,cmd,timeout=300):
        t=time.monotonic()
        p=subprocess.run(list(map(str,cmd)),cwd=root,text=True,capture_output=True,timeout=timeout,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
        rows.append(dict(name=name,command=list(map(str,cmd)),returncode=p.returncode,elapsed_s=time.monotonic()-t,stdout=p.stdout,stderr=p.stderr))
        out.write_text(json.dumps(dict(version=build.VERSION,passed=name=='checks' and all(r['returncode']==0 for r in rows),sanitize=a.sanitize,scope='actual production helpers in a simulated outer Switch loop; no physical GPU/HID',results=rows),indent=2)+'\n')
        print(name,p.returncode,flush=True);print(p.stdout,flush=True)
        if p.returncode:print(p.stderr,flush=True);raise SystemExit(1)
    cls=build.BUILD/'test-classes';cls.mkdir(exist_ok=True)
    run('java',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',root/'romfs/classes','-classpath',root/'romfs/classes','-d',cls,root/'tests/java/PaintHandoff241Checks.java'])
    puff=build.BUILD/'boundary242-puff.o';run('puff',['gcc','-O2','-c',root/'src/puff.c','-o',puff])
    flags=['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
    binary=build.BUILD/('host-boundary242-'+('san' if a.sanitize else 'opt'))
    run('compile',['g++']+flags+['-std=c++17','-fno-exceptions',root/'tests/host_boundary242_checks.cpp',puff,'-ldl','-o',binary],600)
    run('checks',[binary],600)
if __name__=='__main__':main()
