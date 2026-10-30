#!/usr/bin/env python3
"""Fit19 code-pool capacity, quarantine, retry and stale-ID regressions."""
import argparse
import datetime
import json
import os
import shlex
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cxx', default='g++')
p.add_argument('--runner', default='', help='quoted emulator command, e.g. qemu-aarch64 -L SYSROOT')
p.add_argument('--sysroot', type=Path)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--output', type=Path, default=ROOT / 'build/fit19-checks')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
records = []
runner = shlex.split(a.runner)
flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if a.sanitize else ['-O2']

if a.sysroot: flags += ['--sysroot='+str(a.sysroot.resolve())]

def run(label, argv):
    began = time.monotonic()
    proc = subprocess.run(list(map(str, argv)), cwd=ROOT, capture_output=True, text=True,
                          timeout=300, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
    records.append(dict(label=label, argv=list(map(str, argv)), signed_rc=proc.returncode,
                        utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                        seconds=time.monotonic()-began, stdout=proc.stdout, stderr=proc.stderr))
    (a.output / 'results.json').write_text(json.dumps(records, indent=2, ensure_ascii=False)+'\n')
    print(label, 'rc', proc.returncode, flush=True)
    if proc.returncode: raise SystemExit(1)

for name, source, mock, scenarios in [
    ('capacity', 'runtime_fit19/code_pool_capacity_checks.cpp', 'runtime_fit13/jit_mock', ['hot', 'pages']),
    ('resource', 'runtime_fit13/jit_resource_checks.cpp', 'runtime_fit13/jit_mock',
     ['pressure','reset','active','fresh','empty','retry','retryfail','nonresource','freshcap']),
    ('retry', 'runtime_fit15/jit_retry_checks.cpp', 'runtime_fit13/jit_mock', [None]),
    ('backoff', 'runtime_fit15/jit_retry_backoff_checks.cpp', 'runtime_fit13/jit_mock',
     ['backoff','nonresource','permanent','normal-reuse']),
    ('pool', 'jit_pool_checks.cpp', None, [None]),
    ('handles', 'jit_handle_pressure.cpp', 'switch_jit_stub', ['160']),
]:
    exe = a.output.resolve() / name
    includes = ['-D__SWITCH__', '-I'+str(ROOT/'tests'/mock)] if mock else []
    run('compile-'+name, [a.cxx]+flags+['-std=c++17']+includes+[ROOT/'tests'/source, '-o', exe])
    for scenario in scenarios:
        run(name+(('-'+scenario) if scenario else ''), runner+[exe]+([scenario] if scenario else []))
print('PASS Fit19 focused checks', flush=True)
