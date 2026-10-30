"""Run native-deadline contracts in the actual VM; production frozen APIs stay unchanged."""
import subprocess
from pathlib import Path
import build

out=build.BUILD/'native-deadline-tests';out.mkdir(parents=True,exist_ok=True)
classes=build.BUILD/'test-classes';classes.mkdir(parents=True,exist_ok=True)
compiler=build.java_compiler(None)
build.run('Compiling deadline fixture',compiler+['-source','8','-target','8','-encoding','UTF-8',
    '-bootclasspath',build.ROOT/'romfs/classes','-classpath',build.ROOT/'romfs/classes',
    '-d',classes,build.ROOT/'tests/java/NativeDeadlineFixture.java'])
puff=out/'puff.o'
build.run('Compiling test decompressor',[build.executable('gcc'),'-O2','-c',build.ROOT/'src/puff.c','-o',puff])
for name,vm in [('native_deadline_gate_checks',False),('native_deadline_vm_checks',True),('runtime_stats_baseline_checks',True)]:
    binary=out/name
    build.run('Compiling '+name,[build.executable('g++'),'-O2','-std=c++17','-fno-exceptions','-pthread',
        '-I'+str(build.ROOT/'tests/mock_switch'),build.ROOT/'tests'/(name+'.cpp')]+([puff,'-ldl'] if vm else [])+['-o',binary])
    build.run('Running '+name,[binary])
