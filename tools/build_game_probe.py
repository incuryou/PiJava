#!/usr/bin/env python3
import argparse,hashlib,json,subprocess
from pathlib import Path
import build
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=build.ROOT);p.add_argument('--name',default='optimized35');a=p.parse_args()
r=a.source.resolve();out=build.BUILD/'game-probe';out.mkdir(parents=True,exist_ok=True)
flags=['-O2','-std=c++17','-fno-exceptions']
for macro,path in [('PROBE_VM',r/'src/j2mevm.cpp'),('PROBE_ROM',r/'romfs/classes'),('PROBE_MENU',r/'src/host_menu.h'),('PROBE_SPEED',r/'src/host_speed.h'),('PROBE_SAVE',r/'src/instant_state.inc')]:flags+=['-D'+macro+'="'+str(path)+'"']
if (r/'src/optimization_config.h').exists():flags+=['-DPROBE_OPT=1']
if (r/'src/native_jit236.h').exists():flags+=['-DPROBE236=1']
subprocess.run(['gcc','-O2','-c',str(r/'src/puff.c'),'-o',str(out/(a.name+'.o'))],check=True)
command=['g++']+flags+[str(build.ROOT/'benchmarks/game_probe.cpp'),str(out/(a.name+'.o')),'-ldl','-o',str(out/a.name)]
subprocess.run(command,check=True)
(out/(a.name+'-build.json')).write_text(json.dumps({'source':str(r),'source_sha256':{str(f.relative_to(r/'src')):hashlib.sha256(f.read_bytes()).hexdigest() for f in (r/'src').rglob('*') if f.is_file()},'binary_sha256':hashlib.sha256((out/a.name).read_bytes()).hexdigest(),'command':command},indent=2)+'\n')
print('Built',out/a.name,flush=True)
