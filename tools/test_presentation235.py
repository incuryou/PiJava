#!/usr/bin/env python3
"""Test optional presentation's lifetime/pixel contracts, not physical GPU execution."""
import argparse,subprocess,json,time
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=build.ROOT;b=build.BUILD;b.mkdir(exist_ok=True)
flags=['-O1','-g','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
exe=b/('presentation235-san' if a.sanitize else 'presentation235');cmd=['g++']+flags+['-std=c++17','-I'+str(r/'tests/mock_gpu'),'-I'+str(r/'tests/vendor'),str(r/'tests/presentation235_checks.cpp'),'-o',str(exe)]
subprocess.run(cmd,check=True);start=time.monotonic();q=subprocess.run([str(exe)],capture_output=True,text=True,timeout=120)
result=dict(passed=q.returncode==0,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,sanitized=a.sanitize,seconds=time.monotonic()-start,compile=cmd,physical_gpu_test=False)
(b/(exe.name+'.json')).write_text(json.dumps(result,indent=2)+'\n');print(q.stdout or q.stderr);raise SystemExit(q.returncode)
