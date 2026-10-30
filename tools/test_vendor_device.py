#!/usr/bin/env python3
"""Exercise the vendor device APIs inside PiJava using the frozen classes."""
import subprocess
import build

build.BUILD.mkdir(exist_ok=True)
classes=build.BUILD/"vendor-device-classes"
classes.mkdir(exist_ok=True)
puff=build.BUILD/"vendor-device-puff.o"
runner=build.BUILD/"vendor_device_checks"
build.run("Compiling vendor device decompressor",[build.executable("gcc"),"-O2","-c",build.ROOT/"src/puff.c","-o",puff])
build.run("Compiling vendor device VM checks",[build.executable("g++"),"-O1","-std=c++17","-fno-exceptions",build.ROOT/"tests/vendor_device_checks.cpp",puff,"-ldl","-o",runner])
subprocess.run([str(runner)],cwd=build.ROOT,check=True,timeout=30)
build.run("Compiling vendor device Java consumer",build.java_compiler(None)+[
    "-source","8","-target","8","-bootclasspath",build.ROOT/"romfs/classes",
    "-classpath",build.ROOT/"romfs/classes","-d",classes,
    build.ROOT/"tests/java/VendorDeviceChecks.java"])
subprocess.run([str(runner),"fixture"],cwd=build.ROOT,check=True,timeout=30)
