#!/usr/bin/env python3
"""Reference-equivalence checks for stage241, not a hardware or JSR conformance test."""
import argparse,json,os,subprocess,time
from pathlib import Path
import build

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sanitize',action='store_true')
    ap.add_argument('--legacy',action='store_true',help='Use build/legacy240-classes extracted from pristine 0.2.40 source')
    ap.add_argument('--skip-compile',action='store_true')
    ap.add_argument('--rows',type=int,default=4160)
    ap.add_argument('--resources',type=int,default=180)
    ap.add_argument('--scenes',type=int,default=64)
    ap.add_argument('--row-start',type=int,default=0)
    ap.add_argument('--resource-start',type=int,default=0)
    ap.add_argument('--scene-start',type=int,default=0)
    ap.add_argument('--out',type=Path)
    a=ap.parse_args()
    if not 0<=a.rows<=100000 or not 1<=a.resources<=360 or not 1<=a.scenes<=1024:ap.error('invalid test range')
    root=build.ROOT;build.BUILD.mkdir(exist_ok=True)
    out=a.out or build.BUILD/('stage241-'+('san' if a.sanitize else 'opt')+('-legacy' if a.legacy else '')+'.json')
    out.parent.mkdir(parents=True,exist_ok=True);results=[]
    def invoke(name,cmd,timeout):
        start=time.monotonic()
        try:
            p=subprocess.run(list(map(str,cmd)),cwd=root,text=True,capture_output=True,timeout=timeout,
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TERM='dumb'))
            row=dict(name=name,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr)
        except subprocess.TimeoutExpired as e:
            row=dict(name=name,returncode='timeout',stdout=str(e.stdout),stderr=str(e.stderr))
        row.update(command=list(map(str,cmd)),elapsed_s=round(time.monotonic()-start,3));results.append(row)
        out.write_text(json.dumps(dict(version=build.VERSION,passed=name=='checks' and all(r['returncode']==0 for r in results),
            sanitizer=a.sanitize,legacy=a.legacy,results=results,scope='desktop reference comparison; not real game or Switch speed'),indent=2)+'\n')
        print(name,row['returncode'],flush=True);print(row['stdout'],end='',flush=True)
        if row['returncode']!=0:print(row['stderr']);raise SystemExit(1)
    binary=build.BUILD/('m3g-stage241-'+('san' if a.sanitize else 'opt'))
    if not a.skip_compile:
        build.rebuild_java(build.java_compiler(None))
        cls=build.BUILD/'test-classes';cls.mkdir(exist_ok=True)
        invoke('compile-java',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',root/'romfs/classes',
            '-classpath',root/'romfs/classes','-d',cls,root/'tests/java/M3GStage241Checks.java'],60)
        puff=build.BUILD/'stage241-puff.o';invoke('compile-puff',['gcc','-O2','-c',root/'src/puff.c','-o',puff],60)
        flags=['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow',
            '-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
        invoke('compile-native',['g++']+flags+['-std=c++17','-fno-exceptions',root/'tests/m3g_stage241_checks.cpp',puff,'-ldl','-o',binary],300)
    invoke('checks',[binary,a.rows,a.resources,a.scenes,'legacy' if a.legacy else 'current',a.row_start,a.resource_start,a.scene_start],900)
    print('Results:',out)
if __name__=='__main__':main()
