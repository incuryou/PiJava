#!/usr/bin/env python3
"""Derive the two 2D cap centers from actual RGBA, then test the real renderer."""
from collections import deque
from datetime import datetime,timezone
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def cap(raw,seed,threshold):
    q=deque([seed]);seen={seed}
    while q:
        x,y=q.popleft()
        for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
            if (nx,ny) in seen or not(0<=nx<460 and 0<=ny<318):continue
            p=(ny*460+nx)*4
            if raw[p+3] and max(raw[p:p+3])>threshold:seen.add((nx,ny));q.append((nx,ny))
        if len(seen)>2000:raise ValueError('cap component escaped its black rim')
    return [min(x for x,y in seen)+748,min(y for x,y in seen)+151,max(x for x,y in seen)+748,max(y for x,y in seen)+151]
def main():
    p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--font',default='none');p.add_argument('--windows-adapters',action='store_true');a=p.parse_args();out=a.evidence.resolve();out.mkdir(parents=True,exist_ok=True)
    raw=(ROOT/'romfs/ui/pro-controller.rgba').read_bytes();geometry=[];centers=[]
    for name,seed in [('Minus',(172,93)),('Plus',(286,93))]:
        boxes=[cap(raw,seed,t) for t in (12,16,20)]
        if boxes[0]!=boxes[1] or boxes[0]!=boxes[2]:raise ValueError('cap bbox not stable across thresholds')
        x=(boxes[0][0]+boxes[0][2])/2;y=(boxes[0][1]+boxes[0][3])/2
        if x!=int(x) or y!=int(y):raise ValueError('unexpected half-pixel cap X')
        centers.extend([str(int(x)),str(int(y))]);geometry.append(dict(input=name,thresholds=[12,16,20],boxes=boxes,center_x=x,center_y=y))
    flags=['-O2','-std=c++17','-fno-exceptions']
    if a.windows_adapters:
        spec=out/'no-manifest.specs';spec.write_text('*endfile:\ncrtend.o%s\n');flags+=['-Dlstat=stat','-specs='+str(spec)]
    binary=out/('symbol-xalign.exe' if os.name=='nt' else 'symbol-xalign');rows=[];ok=True
    for phase,argv in [('compile',[shutil.which('g++'),*flags,ROOT/'tests/symbol_xalign_checks.cpp','-o',binary]),('run',[binary,a.font,*centers])]:
        start=datetime.now(timezone.utc).isoformat();r=subprocess.run([str(x) for x in argv],cwd=ROOT,capture_output=True,text=True,timeout=180);(out/(phase+'.stdout.txt')).write_text(r.stdout,encoding='utf-8');(out/(phase+'.stderr.txt')).write_text(r.stderr,encoding='utf-8');rows.append(dict(name=phase,argv=[str(x) for x in argv],raw_rc=r.returncode,utc_start=start,utc_end=datetime.now(timezone.utc).isoformat()));print(phase,r.returncode,r.stdout+r.stderr)
        if r.returncode:ok=False;break
    (out/'results.json').write_text(json.dumps(dict(passed=ok,pixel_geometry=geometry,rgba_sha256=hashlib.sha256(raw).hexdigest(),results=rows),indent=2)+'\n');return 0 if ok else 1
if __name__=='__main__':raise SystemExit(main())
