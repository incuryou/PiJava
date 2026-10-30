#!/usr/bin/env python3
"""Build a standalone NSP from the already built PiJava ELF and production ROMFS.

Run tools/build.py --switch --frozen-java first. This packages the application
itself; no external NRO forwarder or game JAR is included.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import tempfile

import build
from verify_nsp import TITLE_ID, command, sha, verify_package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--keyset', type=Path, required=True, help='private user-supplied packaging keyset; never copied into delivery')
    parser.add_argument('--hacbrewpack', type=Path, required=True, help='locally built hacBrewPack executable; see tools/nsp/README-CN.txt')
    parser.add_argument('--build-dir', type=Path, default=build.BUILD/'switch')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    dkp = Path(os.environ['DEVKITPRO']).resolve()
    inputs = args.build_dir.resolve()
    packer = args.hacbrewpack.resolve()
    output = args.output.resolve() if args.output else build.BUILD/('J2me-vm-'+build.VERSION+'.nsp')
    config = build.ROOT/'tools/nsp/npdm.json'
    for path in [inputs/'j2mevm.elf', inputs/'j2mevm.nacp', config, packer, args.keyset]:
        if not path.is_file():
            raise ValueError('Missing build input: ' + str(path))
    if not (inputs/'romfs-release/classes').is_dir():
        raise ValueError('Missing production ROMFS; build Switch first')
    nacp = (inputs/'j2mevm.nacp').read_bytes()
    if len(nacp) != 0x4000:
        raise ValueError('Invalid source NACP')
    title = nacp[:0x200].split(b'\0')[0].decode()
    author = nacp[0x200:0x300].split(b'\0')[0].decode()
    version = nacp[0x3060:0x3070].split(b'\0')[0].decode()
    if version != build.NACP_VERSION:
        raise ValueError('ELF/NACP build is not the current source version; rebuild Switch first')
    build.BUILD.mkdir(parents=True, exist_ok=True)
    stage = build.BUILD/'nsp'
    if stage.exists():
        shutil.rmtree(stage)
    for folder in ['exefs', 'control', 'nca', 'output', 'temp', 'backup']:
        (stage/folder).mkdir(parents=True, exist_ok=True)
    command([dkp/'tools/bin/elf2nso', inputs/'j2mevm.elf', stage/'exefs/main'], stage/'elf2nso.log')
    command([dkp/'tools/bin/npdmtool', config, stage/'exefs/main.npdm'], stage/'npdmtool.log')
    command([dkp/'tools/bin/nacptool', '--create', title, author, version, stage/'control/control.nacp',
             f'--titleid={TITLE_ID:016x}'], stage/'nacptool.log')
    for language in ['AmericanEnglish', 'SimplifiedChinese']:
        name = 'icon_'+language+'.dat'
        shutil.copyfile(build.ROOT/'assets/nsp/control'/name, stage/'control'/name)

    # Keep only the two keys this packager needs in a mode-0600 temporary file.
    # No title keys, console-unique keys or private keyset bytes enter reports.
    values = {}
    for line in args.keyset.read_text().splitlines():
        if '=' in line:
            key, value = map(str.strip, line.split('=', 1))
            if key in ['header_key', 'key_area_key_application_00']:
                values[key] = value
    for key, length in [('header_key', 64), ('key_area_key_application_00', 32)]:
        if not re.fullmatch('[0-9a-fA-F]{'+str(length)+'}', values.get(key, '')):
            raise ValueError('Missing or invalid required packaging key: ' + key)
    with tempfile.TemporaryDirectory(prefix='nsp-private-', dir=build.BUILD) as private:
        keyset = Path(private)/'keys.dat'
        keyset.write_text(''.join(k+' = '+v+'\n' for k, v in sorted(values.items())))
        keyset.chmod(0o600)
        command([packer, '--keyset', keyset, '--exefsdir', stage/'exefs',
                 '--romfsdir', inputs/'romfs-release', '--controldir', stage/'control',
                 '--ncadir', stage/'nca', '--nspdir', stage/'output', '--tempdir', stage/'temp',
                 '--backupdir', stage/'backup', '--nologo', '--nopatchnacplogo'], stage/'hacbrewpack.log')
        packages = list((stage/'output').glob('*.nsp'))
        if len(packages) != 1 or packages[0].stem != f'{TITLE_ID:016x}':
            raise ValueError('Packager did not produce the expected application NSP')
        result = verify_package(packages[0], inputs, stage/'control', config, dkp, keyset,
                                stage/'verify', version)
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(packages[0], output)
    if sha(output.read_bytes()) != result['sha256']:
        raise ValueError('Delivery copy differs from verified NSP')
    result.update(package=str(output), source_version=build.VERSION,
                  packer_sha256=sha(packer.read_bytes()), key_material_included=False,
                  standalone_application=True, external_nro_required=False)
    (stage/'build-result.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Built and content-verified', output)
    print('Title ID', result['title_id'], 'version', version, 'ROMFS files', result['romfs_files'])


if __name__ == '__main__':
    main()
