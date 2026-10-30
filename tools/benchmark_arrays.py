#!/usr/bin/env python3
"""Measure compact payload and host-only synthetic copy; never infer Switch FPS."""
import json
import platform
import subprocess
import build
build.BUILD.mkdir(exist_ok=True)
puff=build.BUILD/'array-benchmark-puff.o'
runner=build.BUILD/'array_benchmark'
cc=build.executable('gcc');cxx=build.executable('g++')
build.run('Compiling decompressor', [cc,'-O2','-c',build.ROOT/'src/puff.c','-o',puff])
build.run('Compiling array storage benchmark', [cxx,'-O2','-std=c++17','-fno-exceptions',
    '-I'+str(build.ROOT/'src'),build.ROOT/'tests/array_benchmark.cpp',puff,'-o',runner])
result=subprocess.run([str(runner)],cwd=build.ROOT,capture_output=True,text=True,timeout=30,check=True)
data=json.loads(result.stdout)
data['host']=dict(system=platform.system(),machine=platform.machine(),compiler=subprocess.run([cxx,'--version'],capture_output=True,text=True,check=True).stdout.splitlines()[0])
p=build.BUILD/'array-storage-results.json';p.write_text(json.dumps(data,indent=2)+'\n')
print(p)
print('byte[1048576] payload:',data['arrays'][0]['legacy_payload_bytes'],'->',data['arrays'][0]['compact_payload_bytes'],'bytes')
print('Host copy samples:',data['byte_overlap_copy']['samples'])
