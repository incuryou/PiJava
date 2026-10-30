#!/usr/bin/env python3
"""Actual-VM optional file/HTTPS tests; no API rebuild and no host JVM execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=build.ROOT.parent / 'checks/files_network')
parser.add_argument('--api', type=Path, default=build.ROOT / 'romfs/classes')
parser.add_argument('--phase', choices=['red', 'green'], default='green')
parser.add_argument('--skip-native-build', action='store_true')
args = parser.parse_args()
output = args.output.resolve()
classes = output / 'classes'
compile_classes = output / 'compile-only-classes'
contracts = build.ROOT / 'tests/contracts/optional-files-network'
classes.mkdir(parents=True, exist_ok=True)
compile_classes.mkdir(parents=True, exist_ok=True)
# Independent mature signatures permit a fixture compiled before production has
# these types. The compile-only classes never enter the interpreter classpath.
provenance = json.loads((contracts / 'provenance.json').read_text())
for item in provenance['files']:
    assert hashlib.sha256((contracts / item['path']).read_bytes()).hexdigest() == item['sha256'], item['path']
build.run('Compile independent optional API fixture', build.java_compiler(None) + [
    '-source', '8', '-target', '8', '-bootclasspath', args.api, '-classpath', args.api,
    '-encoding', 'UTF-8', '-d', compile_classes] + sorted(contracts.rglob('*.java')) +
    [build.ROOT / 'tests/java/OptionalFilesNetworkChecks.java'])

def public_contract(classpath, name):
    proc = subprocess.run(['java', '-m', 'jdk.jdeps/com.sun.tools.javap.Main', '-public', '-s',
                           '-constants', '-classpath', str(classpath), name],
                          capture_output=True, text=True, check=False)
    if proc.returncode:
        return None
    lines = proc.stdout.splitlines()
    signatures = []
    for index, line in enumerate(lines):
        line = line.strip().replace(' native ', ' ').replace(' synchronized ', ' ')
        if not line.startswith('public '):
            continue
        # Registries are static-only contracts; no public constructor is needed.
        if name.endswith(('FileSystemRegistry', 'PushRegistry')) and line.startswith('public ' + name + '('):
            continue
        descriptor = lines[index + 1].strip() if index + 1 < len(lines) and 'descriptor:' in lines[index + 1] else ''
        signatures.append(line + ' ' + descriptor)
    return sorted(signatures)

contract_results = []
for path in sorted(contracts.rglob('*.java')):
    name = '.'.join(path.relative_to(contracts).with_suffix('').parts)
    expected = public_contract(compile_classes, name)
    actual = public_contract(args.api, name)
    contract_results.append(dict(name=name, matched=expected is not None and actual == expected,
                                 expected=expected, actual=actual))
(output / (args.phase + '-contracts.json')).write_text(json.dumps(dict(
    independent_reference=str(contracts), reference_sha256=provenance['upstream_sha256'],
    extracted_contracts=9, manually_declared_registry_contracts=2,
    results=contract_results), indent=2) + '\n')
if (classes / 'javax').exists():
    shutil.rmtree(classes / 'javax')
for path in compile_classes.glob('OptionalFilesNetworkChecks*.class'):
    shutil.copyfile(path, classes / path.name)
assert not (classes / 'javax').exists(), 'compile contracts must never enter the runtime classpath'
(classes / 'files-network.bin').write_bytes(bytes([1, 35]))
puff = output / 'puff.o'
runner = output / 'optional_files_network_checks'
if not args.skip_native_build:
    build.run('Compile test decompressor', ['gcc', '-O1', '-c', build.ROOT / 'src/puff.c', '-o', puff])
    build.run('Compile actual VM optional fallback harness', ['g++', '-O0', '-std=c++17', '-fno-exceptions',
        '-ffunction-sections', '-fdata-sections', build.ROOT / 'tests/optional_files_network_checks.cpp',
        puff, '-Wl,--gc-sections', '-o', runner])
results = []
for case in ['files', 'network', 'push', 'datagram', 'resourcesRms']:
    with tempfile.TemporaryDirectory(prefix='optional-files-network-rms-') as rms:
        env = dict(os.environ, J2ME_RMS_DIR=rms)
        proc = subprocess.run([str(runner), str(classes), str(args.api.resolve()), case],
                              cwd=build.ROOT, capture_output=True, text=True, timeout=30, env=env)
    row = dict(case=case, exit=proc.returncode, stdout=proc.stdout, stderr=proc.stderr)
    results.append(row)
    print(case, proc.returncode, proc.stdout.strip() or proc.stderr.strip(), flush=True)
(output / (args.phase + '.json')).write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(any(result['exit'] != 0 for result in results) or
                 (args.phase == 'green' and not all(row['matched'] for row in contract_results)))
