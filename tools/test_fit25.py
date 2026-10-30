#!/usr/bin/env python3
"""Run the complete prior regressions and Fit25 checks on unchanged inputs.

Original game JARs and the legacy save are optional external inputs. The O2
host and QEMU checks establish correctness, not physical Switch frame rates.
NSP content verification follows the separate Switch/NSP build.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from test_fit23 import identity


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--workers', type=int, choices=(1, 2, 3, 4), default=3)
    for name in ('hero', 'hero-legacy', 'pet', 'immortal', 'immortal-state', 'cross-root'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if args.hero_legacy and not args.hero:
        parser.error('--hero-legacy requires --hero')
    if args.immortal_state and not args.immortal:
        parser.error('--immortal-state requires --immortal')
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    before = identity(root)
    (output / 'source-identity.json').write_text(json.dumps(before, indent=2) + '\n')
    py = [sys.executable, '-B']
    fixture = root / 'tests/runtime_fit25'
    legacy = py + [root / 'tools/test_fit24.py', '--output', output / 'fit24', '--workers', args.workers]
    for name in ('hero', 'hero_legacy', 'pet', 'immortal', 'cross_root'):
        value = getattr(args, name)
        if value:
            legacy += ['--' + name.replace('_', '-'), value.resolve()]
    arches = ['--arch', 'both', '--cross-root', args.cross_root.resolve()] if args.cross_root else ['--arch', 'x64']
    interface = py + [fixture / 'run_interface_checks.py', '--source', root,
                      '--output', output / 'interface', '--opt', '2'] + arches
    jobs = [('fit24', legacy, 3600), ('interface', interface, 900)]
    skipped = []
    if args.pet:
        jobs.append(('populated-ui', py + [fixture / 'run_pet_ui_checks.py', '--source', root,
                    '--output', output / 'populated-ui', '--pet-jar', args.pet.resolve(), '--opt', '2'] + arches, 900))
    else:
        skipped.append('populated-ui: external Pet JAR not supplied')
    if args.immortal:
        deadline = py + [fixture / 'bounded_deadline/run_checks.py', '--source', root,
                         '--build', output / 'deadline-build', '--output', output / 'deadline',
                         '--jar', args.immortal.resolve(), '--opt', '2']
        if args.cross_root:
            deadline += ['--cross-root', args.cross_root.resolve()]
        jobs.append(('deadline', deadline, 900))
    else:
        skipped.append('deadline: external original JAR not supplied')
    if args.immortal_state:
        jobs.append(('original-world', py + [fixture / 'run_original_world_checks.py', '--source', root,
                     '--output', output / 'original-world', '--jar', args.immortal.resolve(),
                     '--state', args.immortal_state.resolve()], 600))
    else:
        skipped.append('original-world: external natural scene snapshot not supplied')
    jobs.append(('nsp-structure', py + [fixture / 'nsp_checks.py'], 60))
    records = []
    for label, command, timeout in jobs:
        command = list(map(str, command))
        started = datetime.datetime.now(datetime.timezone.utc).isoformat()
        begin = time.monotonic()
        logfile = output / (label + '.log')
        print('START', label, flush=True)
        with logfile.open('w') as stream:
            try:
                result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=timeout, env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = 124
        records.append(dict(label=label, command=command, cwd=str(root), utc=started,
                            seconds=time.monotonic()-begin, returncode=code, log=str(logfile),
                            timeout_seconds=timeout))
        after = identity(root)
        changed = sorted(k for k in before.keys() | after.keys() if before.get(k) != after.get(k))
        summary = dict(passed=not changed and all(r['returncode'] == 0 for r in records)
                       and len(records) == len(jobs), source_unchanged=not changed,
                       changed_inputs=changed, results=records, skipped=skipped,
                       scope='host regressions and O2 checks; optional AArch64 QEMU correctness; no physical Switch measurement')
        (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print('END', label, 'rc', code, 'seconds', round(records[-1]['seconds'], 2), flush=True)
        if code or changed:
            raise SystemExit('Failed ' + label + '; inspect ' + str(logfile))
        if label == 'fit24':
            previous = json.loads((output / 'fit24/summary.json').read_text())
            if not previous['passed'] or not previous['source_unchanged'] or len(previous['results']) != 5:
                raise SystemExit('The complete prior regression set did not pass')
    print('PASS Fit25 regression groups=' + str(len(jobs)) + ' skipped=' + str(len(skipped)), flush=True)


if __name__ == '__main__':
    main()
