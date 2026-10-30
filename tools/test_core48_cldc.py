#!/usr/bin/env python3
"""Exercise CLDC gap contracts on PiJava; never build Switch or replace API classes."""
import argparse
import json
import subprocess
from pathlib import Path
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--library', type=Path, default=build.ROOT / 'romfs/classes')
parser.add_argument('--case', choices=['all', 'buffered', 'exception', 'stack', 'filter'], default='all')
parser.add_argument('--skip-native-build', action='store_true')
parser.add_argument('--results', type=Path)
args = parser.parse_args()
output = build.BUILD / 'core48-cldc'
classes = output / 'classes'
classes.mkdir(parents=True, exist_ok=True)
build.run('Compiling independent Java 8 consumer contract', build.java_compiler(None) + [
    '--release', '8', '-encoding', 'UTF-8', '-Xlint:-unchecked', '-d', classes,
    build.ROOT / 'tests/java/Core48CldcChecks.java'])
runner = output / 'core48_cldc_checks'
puff = output / 'puff.o'
if not args.skip_native_build:
    build.run('Compiling scoped decompressor', ['gcc', '-O1', '-c', build.ROOT / 'src/puff.c', '-o', puff])
    build.run('Compiling scoped actual VM runner', ['g++', '-O0', '-std=c++17', '-fno-exceptions',
        '-ffunction-sections', '-fdata-sections', build.ROOT / 'tests/core48_cldc_checks.cpp',
        puff, '-Wl,--gc-sections', '-o', runner])
results = []
for case in ['buffered', 'exception', 'stack', 'filter'] if args.case == 'all' else [args.case]:
    command = [str(runner), str(classes), str(args.library.resolve()), case]
    print('COMMAND ' + json.dumps(command), flush=True)
    proc = subprocess.run(command, cwd=build.ROOT, capture_output=True, text=True, timeout=30)
    results.append(dict(case=case, command=command, exit=proc.returncode, stdout=proc.stdout, stderr=proc.stderr))
    print(proc.stdout + proc.stderr, end='', flush=True)
destination = args.results or output / 'results.json'
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(any(result['exit'] != 0 for result in results))
