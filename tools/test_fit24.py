#!/usr/bin/env python3
"""Run Fit23 regressions and Fit24 checks against one unchanged source tree.

Original game JARs are optional external inputs. Controlled host timing and QEMU
execution verify behavior; neither is a measurement of physical Switch FPS.
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
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workers", type=int, choices=(1, 2, 3, 4), default=3)
    parser.add_argument("--hero", type=Path)
    parser.add_argument("--hero-legacy", type=Path)
    parser.add_argument("--pet", type=Path)
    parser.add_argument("--immortal", type=Path)
    parser.add_argument("--cross-root", type=Path)
    args = parser.parse_args()
    if args.hero_legacy and not args.hero:
        parser.error("--hero-legacy requires --hero")
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    before = identity(root)
    (output / "source-identity.json").write_text(json.dumps(before, indent=2) + "\n")
    py = [sys.executable, "-B"]
    legacy = py + [root / "tools/test_fit23.py", "--output", output / "fit23", "--workers", args.workers]
    for name in ("hero", "hero_legacy", "pet"):
        path = getattr(args, name)
        if path:
            legacy += ["--" + name.replace("_", "-"), path.resolve()]
    graphics = py + [root / "tests/runtime_fit24/run_graphics_checks.py", "--source", root,
                     "--output", output / "graphics"]
    if args.pet:
        graphics += ["--pet-jar", args.pet.resolve()]
    if args.cross_root:
        graphics += ["--cross-root", args.cross_root.resolve()]
    long_ops = py + [root / "tests/runtime_fit24/run_long_ops_checks.py", "--source", root,
                     "--output", output / "long-ops"]
    if args.immortal:
        long_ops += ["--immortal-jar", args.immortal.resolve()]
    jobs = [
        ("fit23", legacy, 2400),
        ("pacing", py + [root / "tests/runtime_fit24/run_pacing_checks.py", "--source", root,
                          "--output", output / "pacing"], 600),
        ("hud", py + [root / "tests/runtime_fit24/run_present_fps_checks.py", "--source", root,
                       "--output", output / "hud"], 120),
        ("graphics", graphics, 900),
        ("long-ops", long_ops, 600),
    ]
    records = []
    for label, command, timeout in jobs:
        command = list(map(str, command))
        started = datetime.datetime.now(datetime.timezone.utc).isoformat()
        begin = time.monotonic()
        logfile = output / (label + ".log")
        print("START", label, flush=True)
        with logfile.open("w") as stream:
            try:
                result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=timeout, env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"))
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = 124
        records.append(dict(label=label, command=command, cwd=str(root), utc=started,
                            seconds=time.monotonic() - begin, returncode=code,
                            log=str(logfile), timeout_seconds=timeout))
        after = identity(root)
        changed = sorted(k for k in before.keys() | after.keys() if before.get(k) != after.get(k))
        summary = dict(passed=not changed and all(r["returncode"] == 0 for r in records)
                       and len(records) == len(jobs), source_unchanged=not changed,
                       changed_inputs=changed, results=records,
                       scope="host functional and controlled timing tests, optional QEMU AArch64; no physical Switch measurement")
        (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print("END", label, "rc", code, "seconds", round(records[-1]["seconds"], 2), flush=True)
        if code or changed:
            raise SystemExit("Failed " + label + "; inspect " + str(logfile))
        if label == "fit23":
            previous = json.loads((output / "fit23/summary.json").read_text())
            suite = json.loads((output / "fit23/full86/summary.json").read_text())
            if not previous["passed"] or len(previous["results"]) != 8 or len(suite["results"]) != 86:
                raise SystemExit("The complete previous regression set did not pass")
    print("PASS Fit24 regression groups=5 including_fit23_groups=8 existing_checks=86", flush=True)


if __name__ == "__main__":
    main()
