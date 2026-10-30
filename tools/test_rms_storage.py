#!/usr/bin/env python3
"""RMS lifecycle, migration and failure regressions inside this VM, in temporary directories.

Rebuild romfs/classes before invoking this script. Java compilation uses the bundled
VM APIs, not host JDK APIs. --sanitize enables native memory/undefined-behavior checks.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--output', type=Path, default=build.BUILD / 'rms-storage-results.json')
    args = parser.parse_args()
    if not (build.ROOT / 'romfs/classes/java/lang/Object.class').is_file():
        parser.error('romfs/classes is missing; rebuild the bundled Java APIs first')
    flags = ['-O1', '-g', '-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
    results = []
    with tempfile.TemporaryDirectory(prefix='j2me-rms-checks-') as temporary:
        directory = Path(temporary)
        classes = directory / 'classes'
        classes.mkdir()
        sources = sorted((build.ROOT / 'java/javax/microedition/rms').glob('*.java'))
        sources.append(build.ROOT / 'tests/java/RmsLifecycleChecks.java')
        build.run('Compiling RMS APIs and Java checks', build.java_compiler(None) + [
            '-source', '8', '-target', '8', '-encoding', 'UTF-8',
            '-bootclasspath', build.ROOT / 'romfs/classes', '-classpath', build.ROOT / 'romfs/classes',
            '-d', classes] + sources)
        puff = directory / 'puff.o'
        runner = directory / 'rms_storage_checks'
        build.run('Compiling RMS test decompressor', [build.executable('gcc')] + flags + ['-c', build.ROOT / 'src/puff.c', '-o', puff])
        build.run('Compiling RMS interpreter harness', [build.executable('g++')] + flags + [
            '-std=c++17', '-fno-exceptions', '-I' + str(build.ROOT / 'src'),
            build.ROOT / 'tests/rms_storage_checks.cpp', puff, '-Wl,--wrap=rename', '-o', runner])
        for mode in ['posix', 'nonreplace']:
            stores = directory / mode
            stores.mkdir()
            environment = dict(os.environ, J2ME_RMS_DIR=str(stores))
            before = time.monotonic()
            proc = subprocess.run([str(runner), str(classes), str(build.ROOT / 'romfs/classes'), str(stores), mode],
                                  cwd=build.ROOT, env=environment, text=True, capture_output=True, timeout=60)
            stages = [line for line in proc.stdout.splitlines() if line.startswith('PASS ')]
            item = dict(mode=mode, passed=proc.returncode == 0, seconds=round(time.monotonic() - before, 4),
                        stages=stages, stdout=proc.stdout, stderr=proc.stderr)
            results.append(item)
            print('\n'.join(stages) if item['passed'] else proc.stdout + proc.stderr, flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(dict(version=build.VERSION, sanitized=args.sanitize, results=results), indent=2) + '\n')
    print('Results:', args.output)
    return 0 if all(item['passed'] for item in results) else 1


if __name__ == '__main__':
    sys.exit(main())
