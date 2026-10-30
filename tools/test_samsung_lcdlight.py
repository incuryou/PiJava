#!/usr/bin/env python3
"""Check Samsung LCDLight in the real VM against the frozen Java API."""
import subprocess
import build

build.BUILD.mkdir(exist_ok=True)
classes=build.BUILD/"samsung-lcdlight-classes"
classes.mkdir(exist_ok=True)
puff=build.BUILD/"samsung-lcdlight-puff.o"
runner=build.BUILD/"samsung_lcdlight_checks"
build.run("Compiling LCDLight decompressor",[build.executable("gcc"),"-O2","-c",build.ROOT/"src/puff.c","-o",puff])
build.run("Compiling LCDLight VM checks",[build.executable("g++"),"-O2","-std=c++17","-fno-exceptions",build.ROOT/"tests/samsung_lcdlight_checks.cpp",puff,"-ldl","-o",runner])
# Check class availability before compiling the consumer, so a missing frozen
# API produces a direct regression failure rather than a javac diagnostic.
subprocess.run([str(runner)],cwd=build.ROOT,check=True,timeout=30)
build.run("Compiling LCDLight Java consumer",build.java_compiler(None)+[
    "-source","8","-target","8","-bootclasspath",build.ROOT/"romfs/classes",
    "-classpath",build.ROOT/"romfs/classes","-d",classes,
    build.ROOT/"tests/java/SamsungLcdLightChecks.java"])
subprocess.run([str(runner),"fixture"],cwd=build.ROOT,check=True,timeout=30)
