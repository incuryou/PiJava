#!/usr/bin/env python3
"""P1 bounded diagnostic fidelity, IDs, histogram and native toy overhead checks."""
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
    parser.add_argument('--skip-java', action='store_true', help='Reuse compiled test fixtures while building native checks')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    build.BUILD.mkdir(exist_ok=True)
    out = args.out or build.BUILD / ('p1-diagnostics-san.json' if args.sanitize else 'p1-diagnostics-opt.json')
    out.parent.mkdir(parents=True, exist_ok=True)
    binary = build.BUILD / ('p1-diagnostics-san' if args.sanitize else 'p1-diagnostics-opt')
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
        if not args.skip_java:
            invoke('compile-java', build.java_compiler(None) + ['-source', '8', '-target', '8',
                '-bootclasspath', build.ROOT / 'romfs/classes', '-classpath', build.ROOT / 'romfs/classes',
                '-d', classes, build.ROOT / 'tests/java/P1ThreadEndingChecks.java', build.ROOT / 'tests/java/P1HostChecks.java'], 60)
        puff = build.BUILD / 'p1-diagnostics-puff.o'
        invoke('compile-puff', ['gcc', '-O2', '-c', build.ROOT / 'src/puff.c', '-o', puff], 60)
        flags = ['-O1', '-g1', '-fno-pie', '-no-pie', '-fsanitize=address,undefined,float-cast-overflow',
                 '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
        invoke('compile-native', ['g++'] + flags + ['-std=c++17', '-fno-exceptions',
            build.ROOT / 'tests/p1_diagnostics_checks.cpp', puff, '-ldl', '-o', binary], 300)
    invoke('checks', [binary], 60)
    events = out.with_suffix('.events.log')
    events.write_text(results[-1]['stderr'])
    summary = out.with_suffix('.summary.json')
    invoke('histogram-parser', ['python', build.ROOT / 'tools/summarize_p1_log.py', events, '--out', summary], 30)
    data = json.loads(summary.read_text())
    row = next(r for r in data['launches'] if r['launch'] == '99')
    assert row['paint_count'] == 4 and row['paint_sum_us'] == 2050002 and row['paint_max_us'] == 2048001
    assert row['overflow_count'] == 1 and row['paint_p95_upper_us'] is None and row['paint_p99_upper_us'] is None
    # Corrupt the count: parser must reject, not silently manufacture quantiles.
    corrupt = out.with_suffix('.bad.log')
    corrupt.write_text('P1STAT schema=1 session=1 launch=1 paint_count=3 paint_sum_us=1 paint_max_us=1 paint_hist=1,1 paint_hist_upper_us=1,overflow presents=0 duplicate_presents=0 dropped=0\n')
    reject = subprocess.run(['python', str(build.ROOT / 'tools/summarize_p1_log.py'), str(corrupt), '--out', str(out.with_suffix('.bad.json'))], capture_output=True, text=True)
    assert reject.returncode != 0
    results.append(dict(name='histogram-count-rejection',returncode=0,stdout=reject.stdout,stderr=reject.stderr,elapsed_s=0))
    out.write_text(json.dumps(dict(version=build.VERSION, sanitize=args.sanitize, passed=True,
        scope='desktop_not_device; native toy overhead; no SD or physical rendering performance claim', results=results), indent=2) + '\n')
    print('PASS histogram parser overflow-null and inconsistent-count rejection')
    print('Results:', out)


if __name__ == '__main__':
    main()
