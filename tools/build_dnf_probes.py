#!/usr/bin/env python3
import argparse,hashlib,json,subprocess
from pathlib import Path
import build
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--image-only',action='store_true');p.add_argument('--optimized-only',action='store_true');a=p.parse_args()
r=build.ROOT;o=build.BUILD/'dnf-probes';o.mkdir(exist_ok=True,parents=True)
manifest={}
for label,source in ([('optimized35',r)] if a.optimized_only else [('baseline34',a.baseline.resolve()),('optimized35',r)]):
    flags=['-O2','-std=c++17','-fno-exceptions']
    if a.sanitize:flags=['-O1','-g','-std=c++17','-fno-exceptions','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-sanitize-recover=all'];label+='-san'
    obj=o/(label+'.o');cflags=[f for f in flags if f not in ['-std=c++17','-fno-exceptions']];subprocess.run(['gcc']+cflags+['-c',str(source/'src/puff.c'),'-o',str(obj)],check=True)
    for name in ['dnf_text_probe']:
        macros=[]
        for macro,path in [('PROBE_VM',source/'src/j2mevm.cpp'),('PROBE_ROM',source/'romfs/classes'),('PROBE_MENU',source/'src/host_menu.h'),('PROBE_SPEED',source/'src/host_speed.h'),('PROBE_SAVE',source/'src/instant_state.inc')]:macros+=['-D'+macro+'="'+str(path)+'"']
        exe=o/(name+'-'+label);cmd=['g++']+flags+macros+[str(r/'benchmarks'/(name+'.cpp')),str(obj),'-o',str(exe)]
        print('Compile',exe.name,flush=True);subprocess.run(cmd,check=True)
        manifest[exe.name]={'command':cmd,'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'source':str(source),'source_hashes':{str(f.relative_to(source/'src')):hashlib.sha256(f.read_bytes()).hexdigest() for f in (source/'src').rglob('*') if f.is_file()}}
    (o/('build-san.json' if a.sanitize else 'build.json')).write_text(json.dumps(manifest,indent=2)+'\n')
