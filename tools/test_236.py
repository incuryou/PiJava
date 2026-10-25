#!/usr/bin/env python3
"""0.2.36 VM/JIT/pixel/audio checks; commercial JARs are never required."""
import argparse,json,os,subprocess,time,sys,shutil
from pathlib import Path
import build
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=build.ROOT;o=build.BUILD/'checks236';o.mkdir(exist_ok=True);tag='san' if a.sanitize else 'opt'
flags=['-O1','-g','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
results=[]
def call(cmd,timeout=240,env=None):
 start=time.monotonic();q=subprocess.run(list(map(str,cmd)),cwd=r,capture_output=True,text=True,timeout=timeout,env=env)
 row=dict(command=list(map(str,cmd)),returncode=q.returncode,seconds=time.monotonic()-start,stdout=q.stdout,stderr=q.stderr,passed=q.returncode==0)
 results.append(row);(o/(tag+'-results.json')).write_text(json.dumps(dict(version=build.VERSION,sanitized=a.sanitize,results=results),indent=2))
 print(('PASS ' if row['passed'] else 'FAIL ')+str(cmd[-1])+' '+q.stdout[-450:]+q.stderr[-450:],flush=True)
 if q.returncode:raise RuntimeError('check failed; raw output saved')
c=build.BUILD/'test-classes';c.mkdir(exist_ok=True)
call(build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',r/'romfs/classes','-classpath',r/'romfs/classes','-d',c]+sorted((r/'tests/java').glob('*236Checks.java'))+[r/'tests/java/LifecycleChecks.java'])
obj=o/(tag+'-puff.o');call(['gcc']+flags+['-c',r/'src/puff.c','-o',obj])
for name,needs_vm in [('async_start236_checks',True),('host_freeze236_checks',True),('jit236_checks',False),('tone236_checks',False),('flag_spin236_checks',True),('compat236_vm',True),('audio_ffmpeg236_checks',False)]:
 exe=o/(name+'-'+tag);call(['g++']+flags+['-std=c++17','-fno-exceptions',r/'tests'/(name+'.cpp')]+([obj] if needs_vm else [])+['-ldl','-o',exe])
 if name=='compat236_vm':
  for enabled in ['0','1']:
   for cls in ['Compat236Checks','NokiaPixels236Checks','Timer236Checks']:
    call([exe,cls],env=dict(os.environ,J2ME_JIT=enabled,ASAN_OPTIONS='detect_leaks=0'))
 elif name=='audio_ffmpeg236_checks':
  samples=[]
  if shutil.which('ffmpeg'):
   for ext,codec in [('wav','pcm_s16le'),('mp3','libmp3lame'),('ogg','libvorbis'),('aac','aac')]:
    sample=o/('generated.'+ext);call(['ffmpeg','-hide_banner','-loglevel','error','-f','lavfi','-i','sine=frequency=440:duration=0.2','-ar','16000','-ac','1','-c:a',codec,'-y',sample]);samples.append(sample)
  call([exe]+samples,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 else:call([exe],timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('ALL 236 CHECKS PASS',flush=True)
