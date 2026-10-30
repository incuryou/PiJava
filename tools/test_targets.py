#!/usr/bin/env python3
"""Bounded desktop replay of the six supplied targets; no Switch FPS claims.

Game JARs are external inputs and are never copied into the source distribution.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jar-dir', type=Path, required=True)
parser.add_argument('--output', type=Path, default=build.BUILD / 'target-replay')
args = parser.parse_args()
args.jar_dir = args.jar_dir.resolve()
args.output = args.output.resolve()
args.output.mkdir(parents=True, exist_ok=True)
binary = build.BUILD / 'game_replay'
puff = build.BUILD / 'replay-puff.o'
build.run('Compiling replay decompressor', [build.executable('gcc'), '-O2', '-c', build.ROOT / 'src/puff.c', '-o', puff])
build.run('Compiling desktop replay', [build.executable('g++'), '-O2', '-std=c++17', '-fno-exceptions',
    build.ROOT / 'tests/game_replay.cpp', puff, '-o', binary])
targets = [
    ('petking6_30FPS.jar', 'petking-30', 120, 'petking'),
    ('宠物王国6(去模拟器限制)-1.jar', 'petking-unrestricted', 120, 'petking'),
    ('robot_60.jar', 'robot-60', 40, 'robot'),
    ('装甲特战队.jar', 'robot-original', 40, 'robot'),
    ('开心农场_60帧_档位变速_v3.17.jar', 'farm', 40, None),
    ('火焰外传烈炎佣兵_60FPS_原速逻辑_防抖稳定版.jar', 'fire', 40, None),
]

def replay(target):
    filename, slug, seconds, mode = target
    jar = args.jar_dir / filename
    result = dict(jar=filename, seconds=seconds, mode=mode or 'observe')
    if not jar.is_file():
        return dict(result, passed=False, error='input file missing')
    result['sha256'] = hashlib.sha256(jar.read_bytes()).hexdigest()
    log, png = args.output / (slug + '.log'), args.output / (slug + '.png')
    command = [str(binary), str(jar), str(log), str(png), str(seconds)] + ([mode] if mode else [])
    # Isolate each replay's stores: normal/modified editions run concurrently.
    rms_dir = args.output / (slug + '-rms')
    rms_dir.mkdir(exist_ok=True)
    env = dict(os.environ, J2ME_RMS_DIR=str(rms_dir))
    with (args.output / (slug + '.stdout')).open('w') as out:
        try:
            proc = subprocess.run(command, cwd=build.ROOT, stdout=out, stderr=subprocess.STDOUT, timeout=seconds+120, env=env)
            result['exit_code'] = proc.returncode
        except subprocess.TimeoutExpired:
            result['exit_code'] = None
            result['error'] = 'host watchdog timeout'
    lines = log.read_text().splitlines() if log.exists() else []
    result['log'], result['screenshot'] = log.name, png.name
    result['diagnostics'] = [line for line in lines if 'EX ' in line or 'THREAD uncaught' in line]
    result['replay'] = [line for line in lines if 'REPLAY ' in line]
    result['passed'] = result['exit_code'] == 0 and not result['diagnostics'] and any('bounded timeout completed normally' in line for line in lines)
    print(filename, 'PASS bounded replay' if result['passed'] else 'FAIL', flush=True)
    return result

# Separate host processes; parallel replay timing must not be used as a benchmark.
with ThreadPoolExecutor(max_workers=3) as pool:
    results = list(pool.map(replay, targets))
report = dict(version=build.VERSION, scope='Desktop bounded replay in three independent host processes; not complete gameplay or Switch performance validation', results=results)
(args.output / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
raise SystemExit(0 if all(row['passed'] for row in results) else 1)
