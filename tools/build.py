#!/usr/bin/env python3
"""Rebuild Java APIs, matching bundled examples, and the desktop VM or Switch NRO.

Each bundled JAR whose name matches examples/<name> is recompiled independently.
Manifest and all non-class resources are preserved byte for byte. Bundled JARs
without matching sources are retained unchanged and listed in the build output.
Mix/Wide.class and Old/Jsr.class are explicit hand-written bytecode fixtures,
retained unchanged; all other classes in matching examples are replaced.
"""
import json
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
VERSION = "0.2.114-keyui7-gpuf5-fit30"
# NACP has only 15 usable UTF-8 bytes; use a compact chapter-save build identity.
NACP_VERSION = "0.2.114-k7-f30"
# These legacy raw-opcode regression fixtures deliberately have no Java source.
EXAMPLE_BYTECODE_FIXTURES = {"Mix.jar": ("Wide.class",), "Old.jar": ("Jsr.class",)}


class BuildError(Exception):
    pass


def executable(name):
    found = shutil.which(str(name))
    if not found:
        raise BuildError("required executable not found: " + str(name))
    return found


def run(label, arguments):
    print(label, flush=True)
    print("COMMAND " + json.dumps([str(arg) for arg in arguments]), flush=True)
    subprocess.run([str(arg) for arg in arguments], cwd=ROOT, check=True)


def java_compiler(override):
    if override:
        return [executable(override)]
    javac = shutil.which("javac")
    if javac:
        return [javac]
    java = executable("java")
    probe = subprocess.run([java, "-m", "jdk.compiler/com.sun.tools.javac.Main", "-version"],
                           cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if probe.returncode:
        raise BuildError("javac is missing and java has no jdk.compiler module; install a JDK")
    return [java, "-m", "jdk.compiler/com.sun.tools.javac.Main"]


def rebuild_java(compiler):
    sources = sorted((ROOT / "java").rglob("*.java"))
    if not sources:
        raise BuildError("no bundled Java sources found")
    # An empty bootclasspath prevents accidental linkage against the host JDK.
    # Compile everything at once so java.lang and the rest come from this tree.
    with tempfile.TemporaryDirectory(prefix="java-", dir=BUILD) as temporary:
        temporary = Path(temporary)
        boot = temporary / "empty-bootclasspath"
        classes = temporary / "classes"
        boot.mkdir()
        classes.mkdir()
        run("Rebuilding bundled Java library (Java 8 bytecode)", compiler + [
            "-source", "8", "-target", "8", "-encoding", "UTF-8",
            "-bootclasspath", boot, "-classpath", boot,
            "-sourcepath", ROOT / "java", "-d", classes,
        ] + sources)
        destination = ROOT / "romfs" / "classes"
        destination.parent.mkdir(parents=True, exist_ok=True)
        previous = temporary / "previous-classes"
        if destination.exists():
            destination.rename(previous)
        try:
            classes.rename(destination)
        except OSError:
            if previous.exists():
                previous.rename(destination)
            raise
    print("Rebuilt romfs/classes from all bundled Java sources", flush=True)


def rebuild_examples(compiler):
    examples = ROOT / "examples"
    library = ROOT / "romfs" / "classes"
    directories = {path.name.lower(): path for path in examples.iterdir() if path.is_dir()}
    jars = sorted((ROOT / "romfs" / "jar").glob("*.jar"))
    preserved = []
    with tempfile.TemporaryDirectory(prefix="examples-", dir=BUILD) as temporary:
        staging = Path(temporary)
        replacements = []
        for jar in jars:
            source_dir = directories.get(jar.stem.lower())
            sources = sorted(source_dir.rglob("*.java")) if source_dir else []
            if not sources:
                preserved.append(jar.name)
                continue
            classes = staging / jar.stem / "classes"
            classes.mkdir(parents=True)
            fixtures = EXAMPLE_BYTECODE_FIXTURES.get(jar.name, ())
            fixture_dir = staging / jar.stem / "fixtures"
            fixture_dir.mkdir()
            with zipfile.ZipFile(jar) as original:
                for name in fixtures:
                    if name not in original.namelist():
                        raise BuildError("missing intentional bytecode fixture: " + jar.name + "/" + name)
                    (fixture_dir / name).write_bytes(original.read(name))
            classpath = os.pathsep.join((str(library), str(fixture_dir)))
            run("Rebuilding example " + jar.stem, compiler + [
                "-source", "8", "-target", "8", "-encoding", "UTF-8",
                "-bootclasspath", library, "-classpath", classpath,
                "-sourcepath", source_dir, "-implicit:none", "-d", classes,
            ] + sources)
            # Replace the entire class set: an overlay would retain removed inner classes.
            entries = {}
            with zipfile.ZipFile(jar) as original:
                for entry in original.infolist():
                    if not entry.filename.endswith(".class") or entry.filename in fixtures:
                        entries[entry.filename] = original.read(entry)
            for compiled in sorted(classes.rglob("*.class")):
                entries[compiled.relative_to(classes).as_posix()] = compiled.read_bytes()
            rebuilt = staging / jar.name
            with zipfile.ZipFile(rebuilt, "w", compression=zipfile.ZIP_STORED) as archive:
                names = sorted(entries, key=lambda name: (name != "META-INF/MANIFEST.MF", name))
                for name in names:
                    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                    info.create_system = 3
                    info.external_attr = (0o40755 if name.endswith("/") else 0o100644) << 16
                    archive.writestr(info, entries[name])
            replacements.append((rebuilt, jar))
        # Compile every matching example before changing any bundled JAR.
        for rebuilt, destination in replacements:
            rebuilt.replace(destination)
    print("Rebuilt " + str(len(replacements)) + " bundled example JARs", flush=True)
    if preserved:
        print("Retained bundled JARs without matching source: " + ", ".join(preserved), flush=True)


def desktop(cc, cxx):
    objects = BUILD / "desktop"
    objects.mkdir(parents=True, exist_ok=True)
    run("Compiling desktop VM", [cxx, "-O2", "-DJ2ME_VERSION=\"" + VERSION + "\"", "-ffile-prefix-map=" + str(ROOT) + "=.", "-std=c++17", "-fno-exceptions",
        "-I" + str(ROOT / "src"), "-c", ROOT / "src" / "j2mevm.cpp", "-o", objects / "j2mevm.o"])
    run("Compiling DEFLATE helper", [cc, "-O2", "-ffile-prefix-map=" + str(ROOT) + "=.", "-std=c11", "-c", ROOT / "src" / "puff.c",
        "-o", objects / "puff.o"])
    output = BUILD / "j2mevm"
    run("Linking desktop VM", [cxx, objects / "j2mevm.o", objects / "puff.o", "-ldl", "-o", output])
    print("Built " + str(output), flush=True)


def switch_tools(no_nacp):
    configured = os.environ.get("DEVKITPRO")
    if not configured:
        raise BuildError("--switch requires DEVKITPRO pointing to your devkitPro installation")
    dkp = Path(configured).expanduser().resolve()
    required_files = [dkp / "libnx" / "switch.specs", dkp / "libnx" / "switch.ld",
                      dkp / "libnx" / "include" / "switch.h", dkp / "libnx" / "lib" / "libnx.a"]
    programs = {
        "cc": dkp / "devkitA64" / "bin" / "aarch64-none-elf-gcc",
        "cxx": dkp / "devkitA64" / "bin" / "aarch64-none-elf-g++",
        "romfs": dkp / "tools" / "bin" / "build_romfs",
        "nro": dkp / "tools" / "bin" / "elf2nro",
    }
    if not no_nacp:
        programs["nacp"] = dkp / "tools" / "bin" / "nacptool"
    missing = [str(path) for path in required_files if not path.is_file()]
    for name, path in list(programs.items()):
        if not path.is_file() or not os.access(path, os.X_OK):
            missing.append(str(path))
        else:
            programs[name] = str(path)
    if missing:
        raise BuildError("--switch requires devkitA64, libnx, and switch-tools; missing:\n  "
                         + "\n  ".join(missing))
    return dkp, programs


def verify_nro(path, nacp_expected, icon_path=None):
    with path.open("rb") as binary:
        header = binary.read(0x80)
        if len(header) != 0x80 or header[0x10:0x14] != b"NRO0":
            raise BuildError("elf2nro output has no valid NRO header")
        asset_start = struct.unpack_from("<I", header, 0x18)[0]
        binary.seek(asset_start)
        assets = binary.read(0x38)
        if len(assets) != 0x38 or assets[:4] != b"ASET":
            raise BuildError("NRO asset header is missing")
        nacp_offset, nacp_size, romfs_offset, romfs_size = struct.unpack_from("<QQQQ", assets, 0x18)
        size = path.stat().st_size
        if icon_path is not None:
            icon_offset, icon_size = struct.unpack_from("<QQ", assets, 8)
            icon = icon_path.read_bytes()
            if not icon or icon_size != len(icon) or icon_offset < 0x38 or asset_start + icon_offset + icon_size > size:
                raise BuildError("NRO does not contain the expected icon")
            binary.seek(asset_start + icon_offset)
            if binary.read(icon_size) != icon:
                raise BuildError("Embedded NRO icon differs from source asset")
        if romfs_size == 0 or romfs_offset < 0x38 or asset_start + romfs_offset + romfs_size > size:
            raise BuildError("NRO does not contain a valid nonempty embedded ROMFS")
        if nacp_expected and (nacp_size != 0x4000 or nacp_offset < 0x38
                              or asset_start + nacp_offset + nacp_size > size):
            raise BuildError("NRO does not contain a valid embedded NACP")


def stage_switch_romfs(destination):
    """Production Switch assets: classes, fonts, phone skins; no example JARs."""
    # Stage separately so desktop regression fixtures remain available.
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for directory in ("classes", "font", "skins", "ui", "shaders"):
        if directory == "shaders" and not (ROOT / "romfs" / directory).exists():
            continue
        shutil.copytree(ROOT / "romfs" / directory, destination / directory)
    return destination


def build_filter_shaders(dkp):
    """Offline only: ship DKSH modules, never a runtime GLSL compiler."""
    uam = executable(dkp / "tools/bin/uam")
    output = ROOT / "romfs/shaders"
    output.mkdir(parents=True, exist_ok=True)
    expanded = BUILD / "switch/shaders"
    expanded.mkdir(parents=True, exist_ok=True)
    math = (ROOT / "src/presentation/gpu_filter_math.inc").read_text()
    pins = ["#pragma once", "// Generated by tools/build.py from the exact UAM stage outputs.",
            "// Runtime accepts only these complete modules before deko3d parses DKSH.",
            "namespace j2me_present { namespace gpu_filter {",
            "struct ShaderAsset { unsigned bytes; const char* sha256; };"]
    for stage in ("vert", "frag"):
        source = (ROOT / ("shaders/filter." + stage)).read_text()
        source = source.replace('#include "gpu_filter_math.inc"', math)
        path = expanded / ("filter." + stage)
        path.write_text(source)
        module = output / ("filter." + stage + ".dksh")
        run("Compiling offline GPU filter " + stage, [uam, "-s", stage, "-o", module, path])
        data = module.read_bytes()
        pins.append('constexpr ShaderAsset ' + stage + '_asset = {' + str(len(data)) + ', "' + hashlib.sha256(data).hexdigest() + '"};')
    pins += ["}}", ""]
    (ROOT / "src/presentation/gpu_filter_assets.h").write_text("\n".join(pins))


def switch(dkp, tools, args):
    objects = BUILD / "switch"
    objects.mkdir(parents=True, exist_ok=True)
    arch = ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE"]
    common = ["-O2", "-g1", "-DJ2ME_VERSION=\"" + VERSION + "\"", "-ffile-prefix-map=" + str(ROOT) + "=.", "-fstack-usage", "-ffunction-sections", "-fdata-sections", "-D__SWITCH__"] + arch
    gpu = not args.switch_cpu_only
    if gpu:
        for required in [dkp / 'libnx/include/deko3d.h', dkp / 'libnx/lib/libdeko3d.a']:
            if not required.is_file():
                raise BuildError('GPU presentation build requires deko3d; install switch-dev/deko3d or pass --switch-cpu-only: ' + str(required))
        common.append('-DJ2ME_ENABLE_DEKO3D')
        build_filter_shaders(dkp)
    portlibs=dkp/"portlibs"/"switch"
    for lib in ("avformat","avcodec","avutil","swresample","dav1d","bz2","z"):
        if not (portlibs/"lib"/("lib"+lib+".a")).is_file():
            raise BuildError("Compressed audio backend requires devkitPro library: "+lib)
    includes = ["-I" + str(dkp / "libnx" / "include"), "-I" + str(ROOT / "src"), "-I"+str(portlibs/"include")]
    run("Compiling Switch VM", [tools["cxx"]] + common + includes + [
        "-std=gnu++17", "-fno-rtti", "-fno-exceptions", "-c", ROOT / "src" / "j2mevm.cpp",
        "-o", objects / "j2mevm.o"])
    run("Compiling Switch DEFLATE helper", [tools["cc"]] + common + [
        "-std=gnu11", "-c", ROOT / "src" / "puff.c", "-o", objects / "puff.o"])
    elf = objects / "j2mevm.elf"
    run("Linking Switch ELF", [tools["cxx"]] + arch + [
        "-specs=" + str(dkp / "libnx" / "switch.specs"), objects / "j2mevm.o", objects / "puff.o",
        "-L" + str(dkp / "libnx" / "lib")] + (["-ldeko3d"] if gpu else []) + ["-L"+str(portlibs/"lib"),"-Wl,--start-group","-lavformat","-lavcodec","-lswresample","-lavutil","-ldav1d","-lbz2","-lz","-Wl,--end-group","-lnx", "-o", elf])
    romfs = objects / "romfs.bin"
    romfs.unlink(missing_ok=True)
    release_assets = stage_switch_romfs(objects / "romfs-release")
    run("Packing production ROMFS without example JARs", [tools["romfs"], release_assets, romfs])
    if not romfs.is_file() or romfs.stat().st_size == 0:
        raise BuildError("build_romfs did not create a nonempty ROMFS image")
    output = BUILD / ("J2me-vm-" + VERSION + ".nro")
    icon = ROOT / "assets" / "icon.jpg"
    if not icon.is_file():
        raise BuildError("Missing app icon: " + str(icon))
    arguments = [tools["nro"], elf, output, "--romfs=" + str(romfs), "--icon=" + str(icon)]
    if not args.no_nacp:
        nacp = objects / "j2mevm.nacp"
        nacp.unlink(missing_ok=True)
        run("Creating NACP metadata", [tools["nacp"], "--create", args.app_title,
            args.app_author, NACP_VERSION, nacp])
        arguments.append("--nacp=" + str(nacp))
    output.unlink(missing_ok=True)
    run("Creating Switch NRO", arguments)
    verify_nro(output, not args.no_nacp, icon)
    print("Built " + str(output), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--switch", action="store_true", help="build Switch NRO instead of desktop VM; requires DEVKITPRO")
    parser.add_argument("--switch-cpu-only", action="store_true", help="omit optional deko3d presentation backend from Switch build")
    parser.add_argument("--frozen-java", action="store_true", help="preserve supplied romfs/classes and bundled JAR bytes; skip Java and example rebuilds")
    parser.add_argument("--javac", metavar="EXECUTABLE", help="Java compiler executable (default: javac, then java jdk.compiler)")
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"), metavar="EXECUTABLE", help="desktop C compiler")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"), metavar="EXECUTABLE", help="desktop C++ compiler")
    parser.add_argument("--no-nacp", action="store_true", help="omit optional Switch application metadata")
    parser.add_argument("--app-title", default="Pi Java模拟器", help="Switch NACP title")
    parser.add_argument("--app-author", default="乔治的蛋", help="Switch NACP author")
    args = parser.parse_args()
    try:
        if args.switch:
            dkp, tools = switch_tools(args.no_nacp)
            os.environ["DEVKITPRO"] = str(dkp)
            from check_switch_env import inspect_toolchain
            try:
                environment = inspect_toolchain(dkp, not args.switch_cpu_only, not args.no_nacp)
            except (ValueError, subprocess.SubprocessError) as error:
                raise BuildError(str(error)) from error
            BUILD.mkdir(parents=True, exist_ok=True)
            (BUILD / "switch-toolchain.json").write_text(json.dumps(environment, indent=2) + "\n")
        else:
            cc, cxx = executable(args.cc), executable(args.cxx)
        if any(not (ROOT / item['path']).is_file() for item in json.loads((ROOT/'assets-reuse.json').read_text())['files']):
            raise BuildError("Original font dependencies missing. Run: python3 tools/restore_assets.py /path/to/your-original-0.2.31-source.zip")
        BUILD.mkdir(parents=True, exist_ok=True)
        if not args.frozen_java:
            compiler = java_compiler(args.javac)
            rebuild_java(compiler)
            from generate_library_manifest import generate
            generate()
            if not args.switch:
                rebuild_examples(compiler)
        if args.switch:
            switch(dkp, tools, args)
        else:
            desktop(cc, cxx)
    except subprocess.CalledProcessError as error:
        print("Build failed: " + Path(error.cmd[0]).name + " exited with status " + str(error.returncode), file=sys.stderr)
        return 1
    except (BuildError, OSError) as error:
        print("Build failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
