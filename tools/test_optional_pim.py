#!/usr/bin/env python3
"""Test frozen unavailable PIM contracts on the actual VM without rebuilding APIs."""
import argparse
import json
import subprocess
import struct
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--case', choices=['all', 'declarations', 'denied', 'fallback', 'exceptions'], default='all')
parser.add_argument('--skip-native-build', action='store_true')
parser.add_argument('--library', type=build.Path, default=build.ROOT / 'romfs/classes')
args = parser.parse_args()
output = build.BUILD / 'optional-pim'
classes = output / 'classes'
compile_api = output / 'compile-api'
classes.mkdir(parents=True, exist_ok=True)
compile_api.mkdir(parents=True, exist_ok=True)
# Independent, fixed J2ME-Loader declarations compile the game-style fixture.
# These API classes never enter the VM runtime classpath or frozen library.
build.run('Compiling independent PIM API fixture', build.java_compiler(None) + [
    '--release', '8', '-encoding', 'UTF-8', '-d', compile_api] +
    sorted((build.ROOT / 'tests/fixtures/optional-pim-api').rglob('*.java')))
build.run('Compiling PIM fallback fixture', build.java_compiler(None) + [
    '--release', '8', '-encoding', 'UTF-8', '-classpath', compile_api,
    '-d', classes, build.ROOT / 'tests/java/OptionalPimChecks.java'])
puff = output / 'puff.o'
runner = output / 'optional_pim_checks'
if not args.skip_native_build:
    build.run('Compiling test decompressor', ['gcc', '-O1', '-c', build.ROOT / 'src/puff.c', '-o', puff])
    build.run('Compiling actual VM PIM harness', ['g++', '-O0', '-std=c++17', '-fno-exceptions',
        '-ffunction-sections', '-fdata-sections', build.ROOT / 'tests/optional_pim_checks.cpp',
        puff, '-Wl,--gc-sections', '-o', runner])
results = []
if args.case in ['all', 'declarations']:
    names = sorted(path.stem for path in (build.ROOT / 'tests/fixtures/optional-pim-api/javax/microedition/pim').glob('*.java'))
    abi = []
    for name in names:
        qualified = 'javax.microedition.pim.' + name
        snapshots = []
        for library in [compile_api, args.library]:
            proc = subprocess.run([build.executable('java'), '-m', 'jdk.jdeps/com.sun.tools.javap.Main',
                '-protected', '-constants', '-s', '-classpath', str(library), qualified],
                cwd=build.ROOT, capture_output=True, text=True, timeout=30)
            snapshots.append(proc.stdout)
            if proc.returncode:
                raise build.BuildError(proc.stderr)
        bytecode = (args.library / 'javax/microedition/pim' / (name + '.class')).read_bytes()
        version = struct.unpack('>H', bytecode[6:8])[0]
        abi.append(dict(class_name=qualified, public_abi_matches=snapshots[0] == snapshots[1], major_version=version))
    (output / 'public-abi.json').write_text(json.dumps(abi, indent=2) + '\n')
    passed = all(item['public_abi_matches'] and item['major_version'] == 52 for item in abi)
    results.append(dict(case='public-abi', exit=0 if passed else 1, classes=abi))
    print(('PASS' if passed else 'FAIL') + ' OptionalPim complete public/protected ABI and constants match independent fixture; Java 8 bytecode', flush=True)
for case in ['declarations', 'denied', 'fallback', 'exceptions'] if args.case == 'all' else [args.case]:
    proc = subprocess.run([str(runner), str(classes), str(args.library), case],
                          cwd=build.ROOT, capture_output=True, text=True, timeout=30)
    results.append(dict(case=case, exit=proc.returncode, stdout=proc.stdout, stderr=proc.stderr))
    print(proc.stdout.strip() or proc.stderr.strip(), flush=True)
(output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(any(result['exit'] != 0 for result in results))
