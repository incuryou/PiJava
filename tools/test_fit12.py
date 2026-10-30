#!/usr/bin/env python3
"""Four Fit12 actual-VM regressions. RED is evidence, never a successful test run."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--api', type=Path, default=build.ROOT / 'romfs/classes')
parser.add_argument('--output', type=Path, default=build.ROOT / 'build/fit12')
parser.add_argument('--phase', choices=['red', 'green'], required=True)
parser.add_argument('--case', choices=['all', 'scroll', 'digits', 'registry', 'sensor'], default='all')
parser.add_argument('--skip-native-build', action='store_true')
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
classes = output / 'classes'
classes.mkdir(exist_ok=True)
api = args.api.resolve()
build.run('Compile Fit12 fixture only (frozen API remains untouched)', build.java_compiler(None) + [
    '-source', '8', '-target', '8', '-encoding', 'UTF-8', '-bootclasspath', api,
    '-classpath', api, '-d', classes, build.ROOT / 'tests/fit12/Fit12Checks.java'])
runner, puff = output / 'fit12_checks', output / 'puff.o'
if not args.skip_native_build:
    build.run('Compile test decompressor', ['gcc', '-O1', '-c', build.ROOT / 'src/puff.c', '-o', puff])
    build.run('Compile Fit12 actual VM harness', ['g++', '-O0', '-std=c++17', '-fno-exceptions',
        '-ffunction-sections', '-fdata-sections', build.ROOT / 'tests/fit12/fit12_checks.cpp',
        puff, '-Wl,--gc-sections', '-o', runner])
if not runner.is_file():
    raise SystemExit('Missing native runner; first invocation must build it')
expected_red = {
    'scroll': {'tall_bottom_reached', 'tall_bottom_pixels',
               'enter_tall_from_below_bottom', 'enter_tall_from_below_pixels'},
    'digits': {'all_digit_presses', 'all_digit_releases', 'digit_press_once', 'digit_release_once',
               'digits_with_default_command_no_activation', 'digits_with_default_command_raw_press',
               'digits_with_default_command_raw_release'},
    'registry': {'added', 'removed'},
    'sensor': {'literal_whitespace_rejected'},
}
results = []
cases = list(expected_red) if args.case == 'all' else [args.case]
for case in cases:
    proc = subprocess.run([str(runner), str(classes), str(api), case], cwd=build.ROOT,
                          capture_output=True, text=True, timeout=45)
    details = None
    for line in proc.stdout.splitlines():
        if line.startswith('FIT12_RESULT '): details = json.loads(line[len('FIT12_RESULT '):])
    failed = {a['id'] for a in details['assertions'] if not a['passed']} if details else set()
    known_red = proc.returncode == 1 and failed == expected_red[case]
    results.append(dict(case=case, returncode=proc.returncode, actual_vm=details,
                        known_expected_red=known_red, stdout=proc.stdout, stderr=proc.stderr))
    print(case, 'rc=' + str(proc.returncode), 'known_expected_red=' + str(known_red), flush=True)
    print(proc.stdout + proc.stderr, flush=True)
report = dict(phase=args.phase, api=str(api), runner_sha256=hashlib.sha256(runner.read_bytes()).hexdigest(),
              cases=results, passed=all(r['returncode'] == 0 and r['actual_vm'] is not None for r in results),
              known_expected_red=args.phase == 'red' and all(r['known_expected_red'] for r in results))
destination = output / (args.phase + ('' if args.case == 'all' else '-' + args.case) + '.json')
destination.write_text(json.dumps(report, indent=2) + '\n')
print('REPORT ' + str(destination), flush=True)
# Keep actual failures nonzero even when all are the expected RED evidence.
raise SystemExit(0 if report['passed'] else 1)
