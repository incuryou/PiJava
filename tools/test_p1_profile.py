#!/usr/bin/env python3
"""Compile the standalone P1 parser and validate shipped copy-ready profiles."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="j2me-p1-parser-") as temporary:
    binary = Path(temporary) / "p1_profile_checks"
    subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-O2", "-Wall", "-Wextra",
                    "-Werror", "-fno-exceptions", str(root / "tests/p1_profile_checks.cpp"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(root / "profiles")], check=True)
