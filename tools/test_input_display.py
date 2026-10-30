#!/usr/bin/env python3
"""Focused cold-launch sizing, resolution/presentation, and J2S5 resume regressions.
Historical metadata/restore-fit modes assume SHA profiles or save migration and are not current policy.

Uses the shipped Java API unchanged; compiles only the owned ScreenChecks
fixture. Run from any directory. Results retain command, status and output.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
build.BUILD.mkdir(exist_ok=True)
classes = build.BUILD / 'test-classes'
classes.mkdir(exist_ok=True)
results = []

def execute(label, argv):
    argv = [str(x) for x in argv]
    p = subprocess.run(argv, cwd=build.ROOT, capture_output=True, text=True)
    results.append(dict(test=label, argv=argv, returncode=p.returncode,
                        stdout=p.stdout, stderr=p.stderr))
    print(label + ': ' + str(p.returncode), flush=True)
    if p.returncode:
        print(p.stderr[-3000:], flush=True)
    return p.returncode == 0

flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else ['-O1']
suffix = '-san' if args.sanitize else ''
java_ok = execute('Compile owned screen fixture', build.java_compiler(None) + [
    '-source', '8', '-target', '8', '-bootclasspath', build.ROOT / 'romfs/classes',
    '-classpath', build.ROOT / 'romfs/classes', '-d', classes,
    build.ROOT / 'tests/java/ScreenChecks.java'])
puff = build.BUILD / ('input-display-puff' + suffix + '.o')
puff_ok = execute('Compile decompressor', [build.executable('gcc')] + flags + [
    '-c', build.ROOT / 'src/puff.c', '-o', puff])
for name, source, link in [('AutoScreenChecks', 'auto_screen_checks.cpp', True),
                           ('ScreenChecks', 'screen_checks.cpp', True),
                           ('ScreenPresentChecks', 'screen_present_checks.cpp', False)]:
    if not java_ok or not puff_ok:
        break
    binary = build.BUILD / (name + suffix)
    if execute('Compile ' + name, [build.executable('g++')] + flags + [
            '-std=c++17', '-fno-exceptions', build.ROOT / 'tests' / source]
            + ([puff] if link else []) + ['-ldl', '-o', binary]):
        for mode in (['cold-autosize', 'restore'] if name == 'AutoScreenChecks' else [None]):
            execute(name + (':' + mode if mode else ''), [binary] + ([mode] if mode else []))
report = build.BUILD / ('input-display-results' + suffix + '.json')
report.write_text(json.dumps(dict(version=build.VERSION, sanitized=args.sanitize,
    asan_options=os.environ.get('ASAN_OPTIONS', ''), results=results), indent=2) + '\n')
print('Results: ' + str(report))
sys.exit(0 if results and all(r['returncode'] == 0 for r in results) else 1)
