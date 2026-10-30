#!/usr/bin/env python3
"""Deterministic optimization regressions; baseline feature-off build supported."""
import argparse,json,os,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sanitize',action='store_true');p.add_argument('--rollback',action='store_true');p.add_argument('--frame-pool',action='store_true');args=p.parse_args()
if args.frame_pool and args.rollback:p.error('--frame-pool and --rollback are mutually exclusive')
root=build.ROOT;out=build.BUILD;out.mkdir(exist_ok=True)
flags=['-O1','-g','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
if args.frame_pool:flags+=['-DJ2ME_OPT_FRAME_POOL=1']
if args.rollback:
    flags+=['-DJ2ME_OPT_'+name+'=0' for name in ['FRAME_POOL','GC_SCRATCH','NATIVE_CLASS_CACHE','GFX_STATE','RGB_FILL','AUDIO_POLL','ZXX_HANDOFF','POLL_MARGIN','ASCII_SCAN','FAST_DISPATCH','FAST_PNG','IMAGE_CACHE','GATE_RECOVERY']]
label=('san' if args.sanitize else 'opt')+('-rollback' if args.rollback else '')+('-frame-pool' if args.frame_pool else '')
def run(cmd,**kwargs):return subprocess.run([str(x) for x in cmd],cwd=root,check=True,**kwargs)
classes=out/'test-classes';classes.mkdir(exist_ok=True)
run(build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',root/'romfs/classes','-classpath',root/'romfs/classes','-d',classes,root/'tests/java/OptimizationChecks.java'])
puff=out/('optimization-puff-'+label+'.o');run(['gcc']+flags+['-c',root/'src/puff.c','-o',puff])
results=[]
for name,source,vm in [('OptimizationCore','optimization_core_checks.cpp',False),('OptimizationVM','optimization_vm_checks.cpp',True),('AudioPollOwnership','switch_audio_mock.cpp',False)]:
    binary=out/(name+'-'+label);command=['g++']+flags+['-std=c++17','-fno-exceptions','-I'+str(root/'tests/mock_switch'),root/'tests'/source]+([puff] if vm else [])+['-o',binary]
    print('Compile',name,flush=True);run(command);before=time.monotonic()
    proc=subprocess.run([str(binary)],cwd=root,capture_output=True,text=True,timeout=90)
    results.append(dict(test=name,passed=proc.returncode==0,returncode=proc.returncode,seconds=time.monotonic()-before,stdout=proc.stdout,stderr=proc.stderr,compile=[str(x) for x in command]))
    print(proc.stdout or proc.stderr,flush=True)
    (out/('optimization-tests-'+label+'.json')).write_text(json.dumps(dict(version=build.VERSION,asan_options=os.environ.get('ASAN_OPTIONS',''),scope='host CPU / mock audio, not Switch hardware',results=results),indent=2)+'\n')
binary=out/('CpuSwitch-'+label)
command=['g++']+flags+['-std=c++17','-I'+str(root/'tests/mock_cpu_switch'),root/'tests/cpu_switch_checks.cpp','-o',binary]
run(command)
for mode in range(3):
    proc=subprocess.run([str(binary),str(mode)],cwd=root,capture_output=True,text=True,timeout=20)
    results.append(dict(test='CpuSwitch-'+str(mode),passed=proc.returncode==0,returncode=proc.returncode,stdout=proc.stdout,stderr=proc.stderr))
    print(proc.stdout or proc.stderr,flush=True)
(out/('optimization-tests-'+label+'.json')).write_text(json.dumps(dict(version=build.VERSION,asan_options=os.environ.get('ASAN_OPTIONS',''),scope='host CPU / mock audio/kernel, not Switch hardware',results=results),indent=2)+'\n')
raise SystemExit(0 if all(x['passed'] for x in results) else 1)
