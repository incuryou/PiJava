#!/usr/bin/env python3
"""Run every supplied file unchanged in the desktop VM, with explicit limits.
Not a gameplay/certification/performance test. JARs and ROM assets are external.
"""
import argparse,concurrent.futures,hashlib,json,os,resource,subprocess,time
from pathlib import Path
import build

def classify(rc,timed,text,result,header):
    if header[:4] == bytes.fromhex('7a1a2010'):
        return 'other_platform_SIS'
    if timed:return 'runtime_timeout'
    if rc != 0:return 'runtime_error'
    if not result:return 'missing_result'
    if 'EX uncaught' in text or 'THREAD uncaught' in text:return 'uncaught_exception'
    if result.get('graceful_exit'):return 'graceful_exit'
    if not result.get('draws'):return 'no_draws'
    if not all(result.get(k) for k in ('snapshot_saved','snapshot_restored','snapshot_RGB_equal')):return 'snapshot_failure'
    return 'bounded_run_snapshot_equal'

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('inventory',type=Path);p.add_argument('--binary',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--ids');p.add_argument('--seconds',type=int,default=8);p.add_argument('--cap',type=int,choices=[0,30,60],default=0);p.add_argument('--jobs',type=int,default=4);p.add_argument('--pause',action='store_true');p.add_argument('--timeout',type=int,default=35);p.add_argument('--keys',default='1000:53,2000:-6,3000:53,4500:53,5500:56,6200:53');a=p.parse_args()
    rows=json.loads(a.inventory.read_text());a.binary=a.binary.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    if a.ids:rows=[r for r in rows if r['id'] in a.ids.split(',')]
    probe_hash=hashlib.sha256(a.binary.read_bytes()).hexdigest()
    def task(r):
        jar=Path(r['path']);header=jar.read_bytes()[:4];pre=a.out/r['id'];env=dict(os.environ)
        if a.pause:env['PROBE_PAUSE']='1'
        else:env.pop('PROBE_PAUSE',None)
        keys=a.keys
        cmd=[str(a.binary),str(jar),str(pre),str(a.seconds),str(a.cap),keys,str(a.seconds*1000-800),'-'];start=time.monotonic();rc=None;timed=False
        def limits():resource.setrlimit(resource.RLIMIT_AS,(1024**3,1024**3))
        with pre.with_suffix('.stdout').open('w') as f:
            try:rc=subprocess.run(cmd,cwd=build.ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=a.timeout,preexec_fn=limits).returncode
            except subprocess.TimeoutExpired:timed=True
        text=pre.with_suffix('.stdout').read_text(errors='replace');f=Path(str(pre)+'-result.json');result=json.loads(f.read_text()) if f.exists() else {}
        errors=list(dict.fromkeys(l for l in text.splitlines() if any(s in l for s in ('EX uncaught','THREAD uncaught','FATAL','EX missing method','class not found','unsupported','Unsupported','bad_alloc','unsupported_SIS'))))
        row=dict(id=r['id'],name=r['name'],sha256=r['sha256'],probe_sha256=probe_hash,returncode=rc,timeout=timed,status=classify(rc,timed,text,result,header),errors=errors,result=result,wall_s=time.monotonic()-start,command=cmd,cap=a.cap,jit_requested=env.get('J2ME_JIT','0'),pause_requested=a.pause,scope='bounded headless desktop; isolated RMS; fixed inputs; 1GiB address-space limit; no hardware or full gameplay claim; parallel runs NOT benchmarks')
        pre.with_suffix('.runner.json').write_text(json.dumps(row,ensure_ascii=False,indent=2));print(r['id'],row['status'],result.get('draws'),flush=True);return row
    out=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for row in pool.map(task,rows):
            out.append(row);(a.out/'summary.json').write_text(json.dumps(out,ensure_ascii=False,indent=2))
    from collections import Counter
    print(json.dumps(dict(tested=len(out),statuses=dict(Counter(r['status'] for r in out)),pause_pass=sum(bool(r['result'].get('pause_stable')) and bool(r['result'].get('resume_active')) for r in out)),indent=2),flush=True)
    return 0  # collection failures are data, not silently excluded
if __name__=='__main__':raise SystemExit(main())
