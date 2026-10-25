#!/usr/bin/env python3
"""Short desktop smoke runs; success is not a gameplay or speed guarantee."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parent.parent
out=ROOT/'build/smoke';out.mkdir(parents=True,exist_ok=True)
results=[]
for jar in sorted((ROOT/'romfs/jar').glob('*.jar')):
    begin=time.monotonic()
    try:
        result=subprocess.run([str(ROOT/'build/j2mevm'),'--cp',str(ROOT/'romfs/classes'),'--jar',str(jar),
            '--log',str(out/(jar.stem+'.log')),'-o',str(out/(jar.stem+'.png'))],cwd=ROOT,
            env=dict(os.environ,J2ME_FRAMES='8'),capture_output=True,text=True,timeout=8)
        item=dict(jar=jar.name,exit_code=result.returncode,seconds=round(time.monotonic()-begin,3),
                  passed=result.returncode==0,stderr=result.stderr)
    except subprocess.TimeoutExpired:
        item=dict(jar=jar.name,passed=False,error='8-second timeout')
    results.append(item)
    print(('PASS ' if item['passed'] else 'FAIL ')+jar.name,flush=True)
    if not item['passed']: print(item.get('stderr',item.get('error','')),flush=True)
(ROOT/'build/example-results.json').write_text(json.dumps(dict(frames=8,scope='launch and short run only',results=results),indent=2)+'\n')
sys.exit(0 if all(x['passed'] for x in results) else 1)
