#!/usr/bin/env python3
"""Rebuild and verify the stage239 bounded scanline span against the bundled Java path.
Run from the project root. Requires the original assets listed in assets-reuse.json.
This is a differential regression test, not a JSR-184 conformance test.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import build


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sanitize',action='store_true')
    ap.add_argument('--skip-build',action='store_true')
    ap.add_argument('--cases',type=int,default=12032)
    ap.add_argument('--frames',type=int,default=256)
    ap.add_argument('--start-seed',type=int,default=0)
    ap.add_argument('--out',type=Path)
    args=ap.parse_args()
    if args.start_seed<0 or args.start_seed+args.cases>1000000:
        ap.error('seed range must fit between 0 and 999999')
    if args.frames<2 or args.frames>4096:
        ap.error('--frames must be between 2 and 4096 (seed 1 exercises the span path)')
    if args.cases<1 or args.cases>1000000:
        ap.error('--cases must be between 1 and 1000000')
    root=build.ROOT
    out=args.out or build.BUILD/('m3g-stage239-'+('san' if args.sanitize else 'opt')+'.json')
    out.parent.mkdir(parents=True,exist_ok=True)
    results=[]
    def invoke(name,command,timeout):
        command=list(map(str,command));start=time.monotonic()
        try:
            p=subprocess.run(command,cwd=root,text=True,capture_output=True,timeout=timeout,
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TERM='dumb'))
            row=dict(name=name,command=command,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr)
        except subprocess.TimeoutExpired as ex:
            row=dict(name=name,command=command,returncode='timeout',stdout=str(ex.stdout),stderr=str(ex.stderr))
        row['elapsed_s']=round(time.monotonic()-start,3);results.append(row)
        out.write_text(json.dumps(dict(version=build.VERSION,passed=(name=='differential-and-full-images' and all(x['returncode']==0 for x in results)),
            sanitizer=args.sanitize,results=results,scope='desktop differential checks; not Switch execution'),indent=2)+'\n')
        print(name,row['returncode']);print(row['stdout'],end='')
        if row['returncode']!=0:
            print(row['stderr']);raise SystemExit(1)
    compiler=build.java_compiler(None)
    if not args.skip_build:
        build.rebuild_java(compiler)
    classes=build.BUILD/'test-classes';classes.mkdir(exist_ok=True)
    invoke('compile-java',compiler+['-source','8','-target','8','-encoding','UTF-8',
        '-bootclasspath',root/'romfs/classes','-classpath',root/'romfs/classes','-d',classes,
        root/'tests/java/M3GTexture238Checks.java',root/'tests/java/M3GTexture238FrameChecks.java',root/'tests/java/M3GSpan239Checks.java',root/'tests/java/M3GSpan239FrameChecks.java'],60)
    puff=build.BUILD/'m3g-stage239-puff.o'
    invoke('compile-puff',['gcc','-O2','-c',root/'src/puff.c','-o',puff],60)
    flags=['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow',
        '-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
    binary=build.BUILD/('m3g-stage239-'+('san' if args.sanitize else 'opt'))
    invoke('compile-native',['g++']+flags+['-std=c++17','-fno-exceptions',root/'tests/m3g_span239_checks.cpp',puff,'-ldl','-o',binary],300)
    invoke('differential-and-full-images',[binary,str(args.cases),str(args.frames),str(args.start_seed)],300)
    print('Results:',out)

if __name__=='__main__':
    main()
