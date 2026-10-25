#!/usr/bin/env python3
"""Compile and exercise complete-paint handoff/fairness using the real VM."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
import build

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--skip-compile', action='store_true')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    build.BUILD.mkdir(exist_ok=True)
    out = args.out or build.BUILD / ('paint-handoff241-san.json' if args.sanitize else 'paint-handoff241-opt.json')
    out.parent.mkdir(parents=True, exist_ok=True)
    binary = build.BUILD / ('paint-handoff241-san' if args.sanitize else 'paint-handoff241-opt')
    results = []
    def invoke(name, command, timeout):
        command = list(map(str, command))
        started = time.monotonic()
        try:
            p = subprocess.run(command, cwd=build.ROOT, capture_output=True, text=True, timeout=timeout,
                               env=dict(os.environ, TERM='dumb', ASAN_OPTIONS='detect_leaks=0'))
            result = dict(name=name, command=command, returncode=p.returncode, stdout=p.stdout, stderr=p.stderr)
        except subprocess.TimeoutExpired as e:
            result = dict(name=name, command=command, returncode='timeout', stdout=str(e.stdout), stderr=str(e.stderr))
        result['elapsed_s'] = round(time.monotonic()-started, 3)
        results.append(result)
        out.write_text(json.dumps(dict(version=build.VERSION, sanitizer=args.sanitize,
            leak_detection=False, passed=name=='checks' and all(r['returncode']==0 for r in results), results=results), indent=2)+'\n')
        print(name, result['returncode'], flush=True)
        print(result['stdout'], end='', flush=True)
        if result['returncode'] != 0:
            print(result['stderr'])
            raise SystemExit(1)
    if not args.skip_compile:
        build.rebuild_java(build.java_compiler(None))
        classes = build.BUILD / 'test-classes'
        classes.mkdir(exist_ok=True)
        invoke('compile-java', build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes',
            '-classpath',build.ROOT/'romfs/classes','-d',classes,build.ROOT/'tests/java/PaintHandoff241Checks.java'], 60)
        puff = build.BUILD/'stage241-puff.o'
        invoke('compile-puff', ['gcc','-O2','-c',build.ROOT/'src/puff.c','-o',puff], 60)
        flags = ['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow',
            '-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
        invoke('compile-native', ['g++']+flags+['-std=c++17','-fno-exceptions',build.ROOT/'tests/paint_handoff241_checks.cpp',puff,'-ldl','-o',binary],300)
    invoke('checks', [binary], 180)
    print('Results:', out)
if __name__=='__main__':
    main()
