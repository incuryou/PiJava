#!/usr/bin/env python3
"""Verify P1 BOOT resolution through the real production VM configurator."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--skip-compile', action='store_true')
    args = parser.parse_args()
    build.BUILD.mkdir(exist_ok=True)
    out = args.out or build.ROOT.parent.parent / 'p1-evidence/config-runtime.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    binary = build.BUILD / 'p1-config-runtime-checks'
    results = []

    def run(name, command, timeout=300):
        command = list(map(str, command))
        started = time.monotonic()
        try:
            process = subprocess.run(command, cwd=build.ROOT, text=True, capture_output=True,
                                     timeout=timeout, env=dict(os.environ, TERM='dumb'))
            row = dict(name=name, command=command, returncode=process.returncode,
                       stdout=process.stdout, stderr=process.stderr)
        except subprocess.TimeoutExpired as exc:
            row = dict(name=name, command=command, returncode='timeout', stdout=str(exc.stdout), stderr=str(exc.stderr))
        row['elapsed_s'] = round(time.monotonic() - started, 3)
        results.append(row)
        out.write_text(json.dumps(dict(version=build.VERSION, passed=name == 'checks' and
            all(r['returncode'] == 0 for r in results), scope='real desktop VM BOOT resolver; no Switch hardware validation',
            fixed_compile_features=dict(texture_native=1, span_native=1, row_native=1,
                enforcement='static_assert in include-VM test translation unit'), results=results), indent=2) + '\n')
        print(name, row['returncode'], flush=True)
        print(row['stdout'], end='', flush=True)
        if row['returncode'] != 0:
            print(row['stderr'], flush=True)
            raise SystemExit(1)

    if not args.skip_compile:
        puff = build.BUILD / 'p1-config-runtime-puff.o'
        run('compile-puff', ['gcc', '-O2', '-c', build.ROOT / 'src/puff.c', '-o', puff], 60)
        run('compile-native', ['g++', '-O2', '-std=c++17', '-fno-exceptions',
            build.ROOT / 'tests/p1_config_runtime_checks.cpp', puff, '-ldl', '-o', binary])
    run('checks', [binary, build.ROOT / 'profiles'], 60)
    print('Results:', out)


if __name__ == '__main__':
    main()
