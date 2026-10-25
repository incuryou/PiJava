#!/usr/bin/env python3
"""Targeted 0.2.17 collection/character/key/RMS regressions in the custom VM."""
import subprocess
import tempfile
import build

build.BUILD.mkdir(exist_ok=True)
build.rebuild_java(build.java_compiler(None))
classes = build.BUILD / "compat217-classes"
classes.mkdir(exist_ok=True)
build.run("Compiling compatibility fixtures", build.java_compiler(None) + [
    "-source", "8", "-target", "8", "-encoding", "UTF-8",
    "-bootclasspath", build.ROOT / "romfs/classes", "-classpath", build.ROOT / "romfs/classes",
    "-d", classes, build.ROOT / "tests/java/Compat217Checks.java"])
puff = build.BUILD / "compat217-puff.o"
runner = build.BUILD / "compat217_checks"
build.run("Compiling compatibility decompressor", [build.executable("gcc"), "-O2", "-c", build.ROOT / "src/puff.c", "-o", puff])
build.run("Compiling compatibility runner", [build.executable("g++"), "-O2", "-std=c++17", "-fno-exceptions",
    build.ROOT / "tests/compat217_checks.cpp", puff, "-Wl,--wrap=statvfs", "-o", runner])
with tempfile.TemporaryDirectory(prefix="compat217-rms-") as directory:
    subprocess.run([str(runner), str(classes), str(build.ROOT / "romfs/classes"), directory],
                   cwd=build.ROOT, check=True, timeout=30)
