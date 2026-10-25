#!/usr/bin/env python3
"""Run user-supplied JARs unchanged; bounded smoke coverage, not gameplay FPS.

Each manifest item requires name,path; optional seconds,cap,keys,save_at_ms.
The runner records every failure and timeout. Screenshots must be manually
reviewed to label reached scenes. CPU numbers include capture/log overhead and
parallel sessions; they are explicitly NOT performance comparisons.
"""
import argparse,concurrent.futures,hashlib,json,os,subprocess,time
from pathlib import Path
import build

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('manifest',type=Path);p.add_argument('--out',type=Path,required=True);p.add_argument('--jobs',type=int,default=1);p.add_argument('--variants',nargs='+',default=['baseline31','optimized33']);a=p.parse_args()
    a.out.mkdir(parents=True,exist_ok=True);games=json.loads(a.manifest.read_text());tasks=[]
    for g in games:
        path=Path(g['path']).resolve()
        if not path.is_file():raise FileNotFoundError(path)
        g={**g,'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        for v in a.variants:tasks.append((g,v))
    def test(task):
        game,variant=task;prefix=a.out/(game['name']+'-'+variant);seconds=int(game.get('seconds',12))
        binary=build.BUILD/'game-probe'/variant
        cmd=[str(binary),game['path'],str(prefix),str(seconds),str(game.get('cap',60)),game.get('keys','2000:-6,6500:53'),str(game.get('save_at_ms',-1)),game.get('load_snapshot','-')]
        before=time.monotonic();rc=None;timed_out=False
        with prefix.with_suffix('.stdout').open('w') as log:
            try:rc=subprocess.run(cmd,cwd=build.ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=seconds+60).returncode
            except subprocess.TimeoutExpired:timed_out=True
        result_path=Path(str(prefix)+'-result.json');result=json.loads(result_path.read_text()) if result_path.exists() else None
        logtext=prefix.with_suffix('.stdout').read_text(errors='replace')
        row={'name':game['name'],'jar_sha256':game['sha256'],'variant':variant,'probe_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'returncode':rc,'timeout':timed_out,'seconds':time.monotonic()-before,'completed':rc==0 and result is not None and 'PROBE_COMPLETE normal=1' in logtext,'result':result,'command':cmd,'profile_enabled':os.environ.get('J2ME_PROFILE','0'),'parallel_jobs':a.jobs,'scope':'bounded desktop launch/input/save smoke; inspect scene images; NOT battle validation or FPS benchmark'}
        Path(str(prefix)+'-runner.json').write_text(json.dumps(row,ensure_ascii=False,indent=2)+'\n');print(game['name'],variant,'COMPLETED' if row['completed'] else 'FAILED',flush=True);return row
    rows=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,min(a.jobs,2))) as pool:
        for row in pool.map(test,tasks):
            rows.append(row);(a.out/'summary.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
    print('Completed',sum(r['completed'] for r in rows),'of',len(rows),flush=True)
    raise SystemExit(0 if all(r['completed'] for r in rows) else 1)
if __name__=='__main__':main()
