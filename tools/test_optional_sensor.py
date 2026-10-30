#!/usr/bin/env python3
"""Independent sensor ABI and actual-VM no-device tests; never rebuild frozen APIs."""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
from pathlib import Path
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=build.ROOT.parent / 'checks/sensors')
parser.add_argument('--api', type=Path, default=build.ROOT / 'romfs/classes')
parser.add_argument('--phase', choices=['red', 'green'], default='green')
parser.add_argument('--case', choices=['all', 'declarations', 'discovery', 'fallback', 'validation', 'listeners', 'values'], default='all')
parser.add_argument('--skip-native-build', action='store_true')
args = parser.parse_args()
output = args.output.resolve()
classes = output / 'classes'
compile_api = output / 'compile-only-classes'
contracts = build.ROOT / 'tests/contracts/optional-sensor/java'
provenance = json.loads((contracts.parent / 'provenance.json').read_text())
for item in provenance['fixtures']:
    assert hashlib.sha256((contracts.parent / item['path']).read_bytes()).hexdigest() == item['sha256'], item['path']
for path in [classes, compile_api]:
    if path.exists(): shutil.rmtree(path)
    path.mkdir(parents=True)
build.run('Compile independent sensor contracts and fixture', build.java_compiler(None) + [
    '-source', '8', '-target', '8', '-encoding', 'UTF-8', '-bootclasspath', args.api,
    '-classpath', args.api, '-d', compile_api] + sorted(contracts.rglob('*.java')) +
    [build.ROOT / 'tests/java/OptionalSensorChecks.java'])
for path in compile_api.glob('OptionalSensorChecks*.class'):
    shutil.copyfile(path, classes / path.name)
assert not (classes / 'javax').exists(), 'compile contracts must never enter the runtime classpath'

def abi(library, name):
    proc = subprocess.run(['java', '-m', 'jdk.jdeps/com.sun.tools.javap.Main', '-protected',
        '-s', '-constants', '-classpath', str(library), name], capture_output=True, text=True)
    if proc.returncode: return None
    lines = proc.stdout.splitlines()
    signatures = []
    for index, line in enumerate(lines):
        line = line.strip()
        if not line.startswith(('public ', 'protected ')): continue
        # Fixed reference has extra public constructors in these static/factory
        # classes. They are deliberately excluded from the supported contract.
        if library == compile_api and name.endswith(('SensorManager', 'Unit')) and line.startswith('public ' + name + '('): continue
        desc = lines[index + 1].strip() if index + 1 < len(lines) and 'descriptor:' in lines[index + 1] else ''
        signatures.append(line + ' ' + desc)
    return sorted(signatures)

abi_results = []
for path in sorted(contracts.rglob('*.java')):
    name = '.'.join(path.relative_to(contracts).with_suffix('').parts)
    expected, actual = abi(compile_api, name), abi(args.api, name)
    frozen = args.api / path.relative_to(contracts).with_suffix('.class')
    major = struct.unpack('>H', frozen.read_bytes()[6:8])[0] if frozen.exists() else None
    abi_results.append(dict(name=name, matched=actual is not None and expected == actual,
                            major_version=major, expected=expected, actual=actual))
(output / (args.phase + '-abi.json')).write_text(json.dumps(abi_results, indent=2) + '\n')
puff, runner = output / 'puff.o', output / 'optional_sensor_checks'
if not args.skip_native_build:
    build.run('Compile test decompressor', ['gcc', '-O1', '-c', build.ROOT / 'src/puff.c', '-o', puff])
    build.run('Compile actual VM sensor harness', ['g++', '-O0', '-std=c++17', '-fno-exceptions',
        '-ffunction-sections', '-fdata-sections', build.ROOT / 'tests/optional_sensor_checks.cpp',
        puff, '-Wl,--gc-sections', '-o', runner])
results = []
cases = ['declarations', 'discovery', 'fallback', 'validation', 'listeners', 'values'] if args.case == 'all' else [args.case]
for case in cases:
    proc = subprocess.run([str(runner), str(classes), str(args.api.resolve()), case],
                          cwd=build.ROOT, capture_output=True, text=True, timeout=30)
    results.append(dict(case=case, exit=proc.returncode, stdout=proc.stdout, stderr=proc.stderr))
    print(case, proc.returncode, proc.stdout.strip() or proc.stderr.strip(), flush=True)
(output / (args.phase + '.json')).write_text(json.dumps(results, indent=2) + '\n')
if args.phase == 'green':
    passed = all(row['matched'] and row['major_version'] == 52 for row in abi_results)
    print(('PASS' if passed else 'FAIL') + ' OptionalSensor independent ABI and Java8 major52', flush=True)
    rebuilt = output / 'source-rebuild'
    if rebuilt.exists(): shutil.rmtree(rebuilt)
    rebuilt.mkdir()
    build.run('Verify sensor source/frozen coherence without changing runtime APIs', build.java_compiler(None) + [
        '-source', '8', '-target', '8', '-encoding', 'UTF-8', '-bootclasspath', args.api,
        '-classpath', args.api, '-sourcepath', build.ROOT / 'java', '-d', rebuilt] +
        sorted((build.ROOT / 'java/javax/microedition/sensor').glob('*.java')))
    coherence = []
    for path in sorted(rebuilt.rglob('*.class')):
        relative = path.relative_to(rebuilt)
        frozen = args.api / relative
        coherence.append(dict(path=str(relative), sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                              matches=frozen.exists() and path.read_bytes() == frozen.read_bytes()))
    expected_paths = sorted(str(path.relative_to(args.api)) for path in (args.api / 'javax/microedition/sensor').glob('*.class'))
    coherent = all(row['matches'] for row in coherence) and sorted(row['path'] for row in coherence) == expected_paths
    (output / 'source-coherence.json').write_text(json.dumps(coherence, indent=2) + '\n')
    print(('PASS' if coherent else 'FAIL') + ' OptionalSensor source/frozen class coherence', flush=True)
    passed = passed and coherent
else: passed = True
raise SystemExit(any(row['exit'] for row in results) or not passed)
