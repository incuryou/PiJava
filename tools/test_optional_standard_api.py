#!/usr/bin/env python3
"""Exercise two standard API gaps on the actual VM without rebuilding its API."""
import argparse
import json
import subprocess
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--case', choices=['all', 'double', 'unsupported'], default='all')
parser.add_argument('--skip-native-build', action='store_true')
args = parser.parse_args()
output = build.BUILD / 'optional-standard-api'
classes = output / 'classes'
classes.mkdir(parents=True, exist_ok=True)
# The independent JDK compile contract permits RED tests against missing frozen
# API signatures. Only our interpreter runs this fixture, never the host JVM.
build.run('Compiling standard API contract fixture', build.java_compiler(None) + [
    '--release', '8', '-encoding', 'UTF-8',
    '-d', classes, build.ROOT / 'tests/java/OptionalStandardApiChecks.java'])
puff = output / 'puff.o'
runner = output / 'optional_standard_api_checks'
if not args.skip_native_build:
    build.run('Compiling test decompressor', ['gcc', '-O1', '-c', build.ROOT / 'src/puff.c', '-o', puff])
    build.run('Compiling actual VM standard API harness', ['g++', '-O0', '-std=c++17', '-fno-exceptions',
        '-ffunction-sections', '-fdata-sections', build.ROOT / 'tests/optional_standard_api_checks.cpp',
        puff, '-Wl,--gc-sections', '-o', runner])
results = []
for case in ['double', 'unsupported'] if args.case == 'all' else [args.case]:
    proc = subprocess.run([str(runner), str(classes), str(build.ROOT / 'romfs/classes'), case],
                          cwd=build.ROOT, capture_output=True, text=True, timeout=30)
    results.append(dict(case=case, exit=proc.returncode, stdout=proc.stdout, stderr=proc.stderr))
    print(proc.stdout.strip() or proc.stderr.strip(), flush=True)
(output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(any(result['exit'] != 0 for result in results))
