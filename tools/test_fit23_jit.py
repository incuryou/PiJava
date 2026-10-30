#!/usr/bin/env python3
"""Fit23 finite CodeMemory capacity, bitmap, byte budget and publication checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parent.parent
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cxx', default='g++')
p.add_argument('--runner', default='', help='quoted emulator command, e.g. qemu-aarch64 -L SYSROOT')
p.add_argument('--sysroot', type=Path)
p.add_argument('--header', type=Path, default=ROOT/'src/native_jit236.h',
               help='override with unchanged Fit22 header to reproduce failing regressions')
p.add_argument('--sanitize', action='store_true')
p.add_argument('--output', type=Path, default=ROOT/'build/fit23-jit-checks')
a = p.parse_args()
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=True)
header = a.header.resolve()
inputs = [header, header.parent/'jit_code_pool.h', header.parent/'jit_budget.h',
          ROOT/'tests/runtime_fit23/jit_arena_checks.cpp',
          ROOT/'tests/runtime_fit23/jit_mock/switch.h', ROOT/'tests/runtime_fit13/jit_mock/switch.h']
evidence = dict(inputs=[dict(path=str(path), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                       for path in inputs], results=[])

def run(label, argv):
    command = list(map(str, argv))
    proc = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=300,
                          env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
    evidence['results'].append(dict(label=label, argv=command, signed_rc=proc.returncode,
                                    stdout=proc.stdout, stderr=proc.stderr))
    (a.output/'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print(label, 'rc', proc.returncode, flush=True)
    return proc.returncode == 0

flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
if a.sysroot:
    flags += ['--sysroot='+str(a.sysroot.resolve())]
exe = a.output/'jit-arena'
if not run('compile', [a.cxx]+flags+['-std=c++17', '-D__SWITCH__',
           '-I'+str(ROOT/'tests/runtime_fit23/jit_mock'), '-DJ2ME_TEST_JIT_HEADER="'+str(header)+'"',
           ROOT/'tests/runtime_fit23/jit_arena_checks.cpp', '-o', exe]):
    raise SystemExit(1)
ok = True
for scenario in ['capacity', 'publish', 'cross', 'slice', 'budget', 'writefail', 'execfail']:
    ok = run(scenario, shlex.split(a.runner)+[exe, scenario]) and ok
raise SystemExit(0 if ok else 1)
