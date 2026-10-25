#!/usr/bin/env python3
"""Stage242 geometry differential checks; not a hardware benchmark."""
import argparse,json,os,subprocess,time
from pathlib import Path
import build

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--cases',type=int,default=128);ap.add_argument('--start',type=int,default=0);ap.add_argument('--legacy',action='store_true');ap.add_argument('--sanitize',action='store_true');ap.add_argument('--skip-compile',action='store_true');ap.add_argument('--out',type=Path);a=ap.parse_args()
 if a.cases<1 or a.start<0:ap.error('cases must be positive; start must be nonnegative')
 root=build.ROOT;build.BUILD.mkdir(exist_ok=True);out=a.out or build.BUILD/('stage242-'+('san' if a.sanitize else 'opt')+('-legacy' if a.legacy else '')+'.json');out.parent.mkdir(parents=True,exist_ok=True);results=[]
 def run(name,cmd,timeout=600):
  start=time.monotonic()
  try:
   r=subprocess.run(list(map(str,cmd)),cwd=root,text=True,capture_output=True,timeout=timeout,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));row=dict(name=name,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr)
  except subprocess.TimeoutExpired as e:row=dict(name=name,returncode='timeout',stdout=str(e.stdout),stderr=str(e.stderr))
  row.update(command=list(map(str,cmd)),elapsed_s=round(time.monotonic()-start,3));results.append(row);out.write_text(json.dumps(dict(version=build.VERSION,passed=name=='checks' and all(x['returncode']==0 for x in results),legacy=a.legacy,sanitize=a.sanitize,results=results),indent=2)+'\n');print(name,row['returncode'],flush=True);print(row['stdout'],end='',flush=True)
  if row['returncode']!=0:print(row['stderr'],flush=True);raise SystemExit(1)
 binary=build.BUILD/('m3g-stage242-'+('san' if a.sanitize else 'opt'))
 if not a.skip_compile:
  build.rebuild_java(build.java_compiler(None));cls=build.BUILD/'test-classes';cls.mkdir(exist_ok=True)
  run('compile-java',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',root/'romfs/classes','-classpath',root/'romfs/classes','-d',cls,root/'tests/java/M3GStage242Checks.java'])
  puff=build.BUILD/'stage242-puff.o';run('compile-puff',['gcc','-O2','-c',root/'src/puff.c','-o',puff])
  flags=['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
  run('compile-native',['g++']+flags+['-std=c++17','-fno-exceptions',root/'tests/m3g_stage242_checks.cpp',puff,'-ldl','-o',binary])
 run('checks',[binary,a.cases,'legacy' if a.legacy else 'current',a.start],1200)
 print('Results:',out,flush=True)
if __name__=='__main__':main()
