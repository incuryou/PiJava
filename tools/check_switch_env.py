#!/usr/bin/env python3
"""Check and record the Switch compiler, SDK libraries and FFmpeg header ABI.

This is an environment check, not physical Switch validation. It performs no
installation, download or network access. The source retains its FFmpeg 7.1
adapter; incompatible header ABIs are rejected instead of silently disabling
compressed audio after a successful link.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = ('avformat', 'avcodec', 'avutil', 'swresample', 'dav1d', 'bz2', 'z')
ABI = {'avcodec': 61, 'avformat': 61, 'avutil': 59}


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def inspect_toolchain(dkp, gpu=True, nacp=True):
    dkp = Path(dkp).resolve()
    a64 = Path(os.environ.get('DEVKITA64', str(dkp / 'devkitA64'))).resolve()
    paths = [str(a64 / 'bin/aarch64-none-elf-gcc'),
             str(a64 / 'bin/aarch64-none-elf-g++'),
             'tools/bin/build_romfs', 'tools/bin/elf2nro',
             'libnx/include/switch.h', 'libnx/switch.specs',
             'libnx/switch.ld', 'libnx/lib/libnx.a']
    if nacp:
        paths.append('tools/bin/nacptool')
    if gpu:
        paths += ['libnx/include/deko3d.h', 'libnx/lib/libdeko3d.a']
    paths += ['portlibs/switch/lib/lib' + name + '.a' for name in LIBRARIES]
    missing = [str(dkp / name) for name in paths if not (dkp / name).is_file()]
    if missing:
        raise ValueError('Missing Switch dependencies:\n  ' + '\n  '.join(missing))
    compiler = a64 / 'bin/aarch64-none-elf-g++'
    target = subprocess.check_output([compiler, '-dumpmachine'], text=True, timeout=15).strip()
    if target != 'aarch64-none-elf':
        raise ValueError('Unexpected compiler target: ' + target)
    version = subprocess.check_output([compiler, '--version'], text=True, timeout=15).splitlines()[0]
    headers = {}
    for name, expected in ABI.items():
        relative = 'portlibs/switch/include/lib' + name + '/'
        relative += 'version.h' if name == 'avutil' else 'version_major.h'
        path = dkp / relative
        if not path.is_file():
            raise ValueError('Missing FFmpeg header: ' + str(path))
        match = re.search(r'^#define\s+LIB' + name.upper() + r'_VERSION_MAJOR\s+(\d+)',
                          path.read_text(), flags=re.M)
        if not match or int(match[1]) != expected:
            raise ValueError('FFmpeg header ABI mismatch: ' + name + '; adapter requires ' + str(expected))
        paths.append(relative)
        headers[name] = int(match[1])
    return {'schema': 1, 'passed': True, 'devkitpro': str(dkp), 'devkita64': str(a64),
            'compiler': version, 'target': target, 'gpu_presentation': gpu,
            'ffmpeg_header_abi': headers,
            'files': {name: {'bytes': (dkp / name).stat().st_size,
                             'sha256': digest(dkp / name)} for name in sorted(paths)},
            'scope': 'dependency presence, compiler execution, header ABI and hashes; not Switch execution'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--devkitpro', type=Path, default=os.environ.get('DEVKITPRO'))
    parser.add_argument('--cpu-only', action='store_true')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/switch-toolchain.json')
    args = parser.parse_args()
    try:
        if not args.devkitpro:
            raise ValueError('Set DEVKITPRO or supply --devkitpro')
        result = inspect_toolchain(args.devkitpro, not args.cpu_only)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        result = {'schema': 1, 'passed': False, 'error': str(error)}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key != 'files'}, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
