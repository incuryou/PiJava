#!/usr/bin/env python3
"""0.2.34 bounded interpreter/cache/gate regressions, on this custom VM."""
import argparse,json,os,subprocess,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sanitize',action='store_true');p.add_argument('--rollback',action='store_true');a=p.parse_args()
r=build.ROOT;o=build.BUILD;o.mkdir(exist_ok=True);label=('san' if a.sanitize else 'opt')+('-rollback' if a.rollback else '')
flags=['-O1','-g','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
if a.rollback:flags+=['-DJ2ME_OPT_'+n+'=0' for n in ['ASCII_SCAN','FAST_DISPATCH','FAST_PNG','IMAGE_CACHE','GATE_RECOVERY']]
def run(cmd,**kw):return subprocess.run([str(x) for x in cmd],cwd=r,check=True,**kw)
c=o/'test-classes';c.mkdir(exist_ok=True)
run(build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',r/'romfs/classes','-classpath',r/'romfs/classes','-d',c,r/'tests/java/Optimization234Checks.java'])
obj=o/('234-puff-'+label+'.o');run(['gcc']+flags+['-c',r/'src/puff.c','-o',obj]);results=[]
for name,vm in [('optimization234_core',False),('optimization234_vm',True)]:
    exe=o/(name+'-'+label);cmd=['g++']+flags+['-std=c++17','-fno-exceptions',r/'tests'/(name+'.cpp')]+([obj] if vm else [])+['-o',exe]
    print('Compile',name,label,flush=True);run(cmd);start=time.monotonic()
    q=subprocess.run([str(exe)],cwd=r,capture_output=True,text=True,timeout=120)
    results.append(dict(test=name,returncode=q.returncode,passed=q.returncode==0,seconds=time.monotonic()-start,stdout=q.stdout,stderr=q.stderr,compile=[str(x) for x in cmd]))
    print(q.stdout or q.stderr,flush=True)
    (o/('tests-234-'+label+'.json')).write_text(json.dumps(dict(version=build.VERSION,sanitized=a.sanitize,rollback=a.rollback,results=results),indent=2)+'\n')
raise SystemExit(0 if all(x['passed'] for x in results) else 1)
