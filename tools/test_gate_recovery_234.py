#!/usr/bin/env python3
"""End-to-end proof reuse through actual interpreted Java time/yield bytecode."""
import json,subprocess,time
import build
r=build.ROOT;b=build.BUILD;b.mkdir(exist_ok=True);c=b/'test-classes';c.mkdir(exist_ok=True)
commands=[build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',str(r/'romfs/classes'),'-classpath',str(r/'romfs/classes'),'-d',str(c),str(r/'tests/java/GateRecovery234Checks.java')],['gcc','-O2','-c',str(r/'src/puff.c'),'-o',str(b/'gate234-puff.o')],['g++','-O2','-std=c++17','-fno-exceptions',str(r/'tests/gate_recovery234_integration.cpp'),str(b/'gate234-puff.o'),'-o',str(b/'gate234-integration')]]
for cmd in commands:subprocess.run(cmd,cwd=r,check=True)
start=time.monotonic();p=subprocess.run([str(b/'gate234-integration')],cwd=r,capture_output=True,text=True,timeout=10)
out=dict(version=build.VERSION,passed=p.returncode==0,seconds=time.monotonic()-start,stdout=p.stdout,stderr=p.stderr,commands=commands,scope='real Java interpreted gate plus controlled host stall, not physical Switch')
(b/'gate234-integration.json').write_text(json.dumps(out,indent=2)+'\n');print(p.stdout+p.stderr);raise SystemExit(p.returncode)
