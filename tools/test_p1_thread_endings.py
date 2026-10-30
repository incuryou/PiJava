#!/usr/bin/env python3
"""P1 thread end-reason and repeated-launch checks against the real VM."""
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
    out = args.out or build.BUILD / ('p1-thread-endings-san.json' if args.sanitize else 'p1-thread-endings-opt.json')
    out.parent.mkdir(parents=True, exist_ok=True)
    binary = build.BUILD / ('p1-thread-endings-san' if args.sanitize else 'p1-thread-endings-opt')
    results = []

    def invoke(name, command, timeout):
        command = list(map(str, command))
        started = time.monotonic()
        try:
            p = subprocess.run(command, cwd=build.ROOT, capture_output=True,
                               text=True, timeout=timeout,
                               env=dict(os.environ, TERM='dumb', ASAN_OPTIONS='detect_leaks=0'))
            row = dict(name=name, command=command, returncode=p.returncode,
                       stdout=p.stdout, stderr=p.stderr)
        except subprocess.TimeoutExpired as e:
            row = dict(name=name, command=command, returncode='timeout',
                       stdout=str(e.stdout), stderr=str(e.stderr))
        row['elapsed_s'] = round(time.monotonic() - started, 3)
        results.append(row)
        out.write_text(json.dumps(dict(version=build.VERSION, sanitize=args.sanitize,
            passed=name == 'checks' and all(r['returncode'] == 0 for r in results),
            results=results), indent=2) + '\n')
        print(name, row['returncode'], flush=True)
        print(row['stdout'], end='', flush=True)
        if row['returncode'] != 0:
            print(row['stderr'], flush=True)
            raise SystemExit(1)

    if not args.skip_compile:
        # Platform classes are rebuilt by the production build; test classes
        # stay under build/test-classes and never enter production ROMFS.
        classes = build.BUILD / 'test-classes'
        classes.mkdir(exist_ok=True)
        invoke('compile-java', build.java_compiler(None) + ['-source', '8', '-target', '8',
            '-bootclasspath', build.ROOT / 'romfs/classes', '-classpath', build.ROOT / 'romfs/classes',
            '-d', classes, build.ROOT / 'tests/java/P1ThreadEndingChecks.java'], 60)
        puff = build.BUILD / 'p1-thread-endings-puff.o'
        invoke('compile-puff', ['gcc', '-O2', '-c', build.ROOT / 'src/puff.c', '-o', puff], 60)
        flags = ['-O1', '-g1', '-fno-pie', '-no-pie', '-fsanitize=address,undefined,float-cast-overflow',
                 '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
        invoke('compile-native', ['g++'] + flags + ['-std=c++17', '-fno-exceptions',
            build.ROOT / 'tests/p1_thread_ending_checks.cpp', puff, '-ldl', '-o', binary], 300)
    invoke('checks', [binary], 60)
    print('Results:', out)


if __name__ == '__main__':
    main()
