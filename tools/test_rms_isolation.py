#!/usr/bin/env python3
"""Independent cross-JAR RMS VM regressions using only temporary test stores.

Rebuild romfs/classes first. --sanitize enables native ASan and UBSan checks.
JARs deliberately share manifest metadata and bytecode but differ in one resource.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
import build


def fixture_jar(path, classes, resource):
    manifest = (b'Manifest-Version: 1.0\r\nMIDlet-Name: Same metadata\r\n'
                b'MIDlet-Vendor: Same vendor\r\nMIDlet-Version: 1.0\r\n'
                b'MIDlet-1: Same metadata,,RmsIsolationChecks\r\n\r\n')
    entries = {'META-INF/MANIFEST.MF': manifest, 'variant.txt': resource}
    entries.update((str(p.relative_to(classes)).replace(os.sep, '/'), p.read_bytes())
                   for p in classes.rglob('*.class'))
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_STORED) as archive:
        for name, contents in sorted(entries.items()):
            entry = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_STORED
            archive.writestr(entry, contents)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--output', type=Path, default=build.BUILD / 'rms-isolation-results.json')
    args = parser.parse_args()
    if not (build.ROOT / 'romfs/classes/java/lang/Object.class').is_file():
        parser.error('romfs/classes is missing; rebuild the bundled Java APIs first')
    flags = (['-O1', '-g', '-fsanitize=address,undefined,float-cast-overflow',
              '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2'])
    results = []
    with tempfile.TemporaryDirectory(prefix='j2me-rms-isolation-') as temporary:
        directory = Path(temporary)
        classes = directory / 'classes'
        classes.mkdir()
        sources = sorted((build.ROOT / 'java/javax/microedition/rms').glob('*.java'))
        sources.append(build.ROOT / 'tests/java/RmsIsolationChecks.java')
        build.run('Compiling RMS APIs and isolation MIDlet', build.java_compiler(None) + [
            '-source', '8', '-target', '8', '-encoding', 'UTF-8',
            '-bootclasspath', build.ROOT / 'romfs/classes', '-classpath', build.ROOT / 'romfs/classes',
            '-d', classes] + sources)
        jars = [directory / 'first.jar', directory / 'second.jar', directory / 'renamed-first.jar']
        fixture_jar(jars[0], classes, b'game A')
        fixture_jar(jars[1], classes, b'game B')
        shutil.copyfile(jars[0], jars[2])
        hashes = dict((p.name, hashlib.sha256(p.read_bytes()).hexdigest()) for p in jars)
        puff = directory / 'puff.o'
        runner = directory / 'rms_isolation_checks'
        build.run('Compiling isolation test decompressor', [build.executable('gcc')] + flags + [
            '-c', build.ROOT / 'src/puff.c', '-o', puff])
        build.run('Compiling isolation interpreter harness', [build.executable('g++')] + flags + [
            '-std=c++17', '-fno-exceptions', '-I' + str(build.ROOT / 'src'),
            build.ROOT / 'tests/rms_isolation_checks.cpp', puff, '-Wl,--wrap=rename', '-o', runner])
        for mode in ['posix', 'nonreplace']:
            stores = directory / mode
            stores.mkdir()
            environment = dict(os.environ, J2ME_RMS_DIR=str(stores))
            before = time.monotonic()
            try:
                proc = subprocess.run([str(runner), str(classes), str(build.ROOT / 'romfs/classes'),
                                       str(stores), mode] + [str(p) for p in jars],
                                      cwd=build.ROOT, env=environment, text=True, capture_output=True, timeout=120)
                stages = [line for line in proc.stdout.splitlines() if line.startswith('PASS ')]
                item = dict(mode=mode, passed=proc.returncode == 0, returncode=proc.returncode,
                            seconds=round(time.monotonic() - before, 4), stages=stages,
                            stdout=proc.stdout, stderr=proc.stderr)
            except subprocess.TimeoutExpired as error:
                item = dict(mode=mode, passed=False, returncode=None,
                            seconds=round(time.monotonic() - before, 4), stages=[],
                            stdout=(error.stdout or b'').decode() if isinstance(error.stdout, bytes) else error.stdout or '',
                            stderr='VM harness timed out after 120 seconds')
            results.append(item)
            print('\n'.join(item['stages']) if item['passed'] else item['stdout'] + item['stderr'], flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(dict(version=build.VERSION, sanitized=args.sanitize,
                                          fixture_sha256=hashes, results=results), indent=2) + '\n')
    print('Results:', args.output)
    return 0 if all(item['passed'] for item in results) else 1


if __name__ == '__main__':
    sys.exit(main())
