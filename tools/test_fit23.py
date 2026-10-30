#!/usr/bin/env python3
"""Run the complete Fit23 host regression set against one unchanged source tree.

The external original-game inputs are optional and are never bundled. This is
functional verification; no result here represents physical Switch frame rate.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def identity(root):
    return {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for base in ("src", "java", "romfs", "tests", "tools", "third_party")
        for path in sorted((root / base).rglob("*"))
        if path.is_file() and "__pycache__" not in path.parts
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workers", type=int, choices=(1, 2, 3, 4), default=3)
    parser.add_argument("--hero", type=Path)
    parser.add_argument("--hero-legacy", type=Path)
    parser.add_argument("--pet", type=Path)
    args = parser.parse_args()
    if args.hero_legacy and not args.hero:
        parser.error("--hero-legacy requires --hero")
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    before = identity(root)
    (output / "source-identity.json").write_text(json.dumps(before, indent=2) + "\n")
    py = [sys.executable, "-B"]
    state = py + [root / "tests/runtime_fit23/run_state_threads_checks.py", "--output", output / "state"]
    if args.hero:
        state += ["--hero", args.hero.resolve()]
    if args.hero_legacy:
        state += ["--hero-legacy", args.hero_legacy.resolve()]
    vector = py + [root / "tests/runtime_fit23/run_vector_checks.py", "--output", output / "vector"]
    if args.pet:
        vector += ["--jar", args.pet.resolve()]
    jobs = [
        ("full86", py + [root / "tests/runtime_fit21/run_suite.py", "--source", root,
                         "--output", output / "full86", "--workers", args.workers], 1800),
        ("jit-arena", py + [root / "tools/test_fit23_jit.py", "--output", output / "jit-arena"], 300),
        ("jit-legacy", py + [root / "tools/test_fit19.py", "--output", output / "jit-legacy"], 300),
        ("state", state, 600),
        ("vector", vector, 300),
        ("rms-directory", py + [root / "tests/runtime_fit23/run_rms_directory_checks.py",
                                "--output", output / "rms-directory"], 300),
        ("rms-storage", py + [root / "tools/test_rms_storage.py", "--output", output / "rms-storage.json"], 300),
        ("rms-isolation", py + [root / "tools/test_rms_isolation.py", "--output", output / "rms-isolation.json"], 300),
    ]
    records = []
    for label, argv, timeout in jobs:
        argv = list(map(str, argv))
        started = datetime.datetime.now(datetime.timezone.utc).isoformat()
        begin = time.monotonic()
        logfile = output / (label + ".log")
        print("START", label, flush=True)
        with logfile.open("w") as stream:
            try:
                result = subprocess.run(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                        timeout=timeout, env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"))
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = 124
        records.append(dict(label=label, command=argv, cwd=str(root), utc=started,
                            seconds=time.monotonic() - begin, returncode=code,
                            log=str(logfile), timeout_seconds=timeout))
        after = identity(root)
        changed = sorted(k for k in before.keys() | after.keys() if before.get(k) != after.get(k))
        summary = dict(passed=not changed and all(r["returncode"] == 0 for r in records)
                       and len(records) == len(jobs), source_unchanged=not changed,
                       changed_inputs=changed, results=records,
                       scope="host functional tests; no physical Switch measurement")
        (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print("END", label, "rc", code, "seconds", round(records[-1]["seconds"], 2), flush=True)
        if code or changed:
            raise SystemExit("Failed " + label + "; inspect " + str(logfile))
        if label == "full86":
            suite = json.loads((output / "full86/summary.json").read_text())
            if len(suite["results"]) != 86 or not suite["source_unchanged"] or not all(r["passed"] for r in suite["results"]):
                raise SystemExit("Existing 86-test suite did not pass unchanged")
    print("PASS Fit23 regression groups=8 existing_checks=86", flush=True)


if __name__ == "__main__":
    main()
