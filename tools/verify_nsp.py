#!/usr/bin/env python3
"""Verify a standalone PiJava homebrew NSP against its ELF and frozen assets.

Homebrew packages do not have Nintendo's fixed-key/ACID signatures. Those two
known unsigned fields are reported separately; content hashes and the Program
NCA's NPDM signature must verify. This is not a physical-console launch test.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

TITLE_ID = 0x05004A324D450000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def identity(directory):
    return {p.relative_to(directory).as_posix(): sha(p.read_bytes())
            for p in sorted(directory.rglob('*')) if p.is_file()}


def read_pfs0(package):
    data = package.read_bytes()
    if len(data) < 16 or data[:4] != b'PFS0':
        raise ValueError('NSP is not a PFS0 container')
    count, strings_size, reserved = struct.unpack_from('<III', data, 4)
    if count != 3 or reserved != 0:
        raise ValueError('Expected exactly Program, Control and Meta NCA entries')
    strings_start = 16 + 24 * count
    payload_start = strings_start + strings_size
    if payload_start > len(data):
        raise ValueError('Truncated PFS0 table')
    strings = data[strings_start:payload_start]
    result, names, ranges = [], set(), []
    for i in range(count):
        offset, size, name_offset, pad = struct.unpack_from('<QQII', data, 16 + 24 * i)
        if name_offset >= strings_size or pad != 0:
            raise ValueError('Invalid PFS0 name offset')
        end = strings.find(b'\0', name_offset)
        if end < 0:
            raise ValueError('Unterminated PFS0 name')
        name = strings[name_offset:end].decode('ascii')
        if not re.fullmatch(r'[0-9a-f]{32}(?:\.cnmt)?\.nca', name) or name in names:
            raise ValueError('Unsafe, duplicate or non-NCA entry name')
        begin, finish = payload_start + offset, payload_start + offset + size
        if size == 0 or begin < payload_start or finish > len(data):
            raise ValueError('Truncated or invalid NCA extent')
        digest = sha(data[begin:finish])
        if not name.startswith(digest[:32]):
            raise ValueError('NCA filename/content hash mismatch')
        names.add(name)
        ranges.append((begin, finish))
        result.append(dict(name=name, offset=begin, bytes=size, sha256=digest))
    ranges.sort()
    if any(b[0] < a[1] for a, b in zip(ranges, ranges[1:])):
        raise ValueError('Overlapping NCA extents')
    if ranges[0][0] != payload_start or ranges[-1][1] != len(data):
        raise ValueError('Unexpected content outside NCA extents')
    if sum('.cnmt.nca' in n for n in names) != 1:
        raise ValueError('Expected one metadata NCA')
    return result


def sanitized(text):
    return re.sub(r'(?im)^.*(?:key area key 2|decrypted key|master key).*$','[key material suppressed]', text)


def command(arguments, logfile):
    result = subprocess.run(list(map(str, arguments)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    text = sanitized(result.stdout.decode(errors='replace'))
    logfile.write_text(text)
    if result.returncode:
        raise ValueError('Tool failed: ' + Path(arguments[0]).name + '; see ' + str(logfile))
    return text


def verify_package(package, build_dir, control_dir, npdm_config, dkp, keyset, output, version):
    """Check structure, all NCA content integrity, CNMT and exact build identity."""
    output.mkdir(parents=True, exist_ok=True)
    entries = read_pfs0(package)
    payload = package.read_bytes()
    types, unsigned = {}, []
    hactool = dkp / 'tools/bin/hactool'
    for entry in entries:
        nca = output / entry['name']
        nca.write_bytes(payload[entry['offset']:entry['offset'] + entry['bytes']])
        destination = output / (entry['name'] + '-contents')
        destination.mkdir(exist_ok=True)
        info = command([hactool, '--suppresskeys', '--disablekeywarns', '-k', keyset, '-y',
                        '--exefsdir=' + str(destination / 'exefs'),
                        '--romfsdir=' + str(destination / 'romfs'),
                        '--section0dir=' + str(destination / 'section0'), nca],
                       destination / 'hactool.log')
        content_type = re.search(r'^Content Type:\s+(\w+)', info, re.M)
        title = re.search(r'^Title ID:\s+([0-9a-fA-F]+)', info, re.M)
        if not content_type or not title or int(title[1], 16) != TITLE_ID:
            raise ValueError('Invalid NCA type or application Title ID')
        kind = content_type[1]
        if kind not in ('Program', 'Control', 'Meta') or kind in types:
            raise ValueError('Unexpected or duplicate NCA content type')
        if not re.search(r'^Magic:\s+NCA3$', info, re.M):
            raise ValueError('Invalid NCA magic')
        # hacBrewPack homebrew has no Nintendo signature. Never ignore content
        # hash failures, or the distinct Program NCA NPDM signature failure.
        for line in info.splitlines():
            if '(FAIL)' in line:
                if re.match(r'^\s*(?:Fixed-Key Signature|Signature) \(FAIL\):\s+0+$', line):
                    unsigned.append(dict(content=kind, field=line.split(':', 1)[0].strip()))
                else:
                    raise ValueError('NCA integrity failed: ' + line.strip())
        if 'Superblock Hash (GOOD)' not in info:
            raise ValueError('NCA section hashes were not verified')
        if kind in ('Program', 'Control') and not re.search(r'Level 5 \(GOOD\)', info):
            raise ValueError('ROMFS IVFC integrity was not verified')
        if kind in ('Program', 'Meta') and 'Hash Table (GOOD)' not in info:
            raise ValueError('PFS0 integrity was not verified')
        if kind == 'Program' and 'NPDM Signature (GOOD)' not in info:
            raise ValueError('Program NPDM signature did not verify')
        types[kind] = dict(entry=entry, directory=destination)
    if set(types) != {'Program', 'Control', 'Meta'}:
        raise ValueError('Incomplete installable application package')

    program, control, meta = (types[k]['directory'] for k in ('Program', 'Control', 'Meta'))
    expected_main = output / 'expected-main'
    command([dkp / 'tools/bin/elf2nso', build_dir / 'j2mevm.elf', expected_main], output / 'elf2nso.log')
    actual_main = program / 'exefs/main'
    if actual_main.read_bytes()[:4] != b'NSO0' or actual_main.read_bytes() != expected_main.read_bytes():
        raise ValueError('NSP program differs from the supplied build ELF')
    if set(identity(program / 'exefs')) != {'main', 'main.npdm'}:
        raise ValueError('Unexpected ExeFS files')
    expected_npdm = output / 'expected.npdm'
    command([dkp / 'tools/bin/npdmtool', npdm_config, expected_npdm], output / 'npdmtool.log')
    before = expected_npdm.read_bytes()
    after = (program / 'exefs/main.npdm').read_bytes()
    if len(before) != len(after) or before[:4] != b'META' or after[:4] != b'META':
        raise ValueError('Invalid NPDM header')
    acid = struct.unpack_from('<I', before, 0x78)[0]
    first, last = acid + 0x100, acid + 0x200
    if last > len(before) or before[:first] != after[:first] or before[last:] != after[last:]:
        raise ValueError('NPDM permissions/configuration differ from the source configuration')
    aci = struct.unpack_from('<I', after, 0x70)[0]
    if after[aci:aci+4] != b'ACI0' or struct.unpack_from('<Q', after, aci + 0x10)[0] != TITLE_ID:
        raise ValueError('NPDM application Title ID mismatch')

    expected_assets = identity(build_dir / 'romfs-release')
    if not expected_assets or identity(program / 'romfs') != expected_assets:
        raise ValueError('NSP ROMFS differs from the frozen production assets')
    controls = identity(control / 'romfs')
    if controls != identity(control_dir) or set(controls) != {'control.nacp', 'icon_AmericanEnglish.dat', 'icon_SimplifiedChinese.dat'}:
        raise ValueError('NSP control metadata or icons differ from the packaging inputs')
    nacp = (control / 'romfs/control.nacp').read_bytes()
    original_nacp = (build_dir / 'j2mevm.nacp').read_bytes()
    if len(nacp) != 0x4000 or nacp[:0x3000] != original_nacp[:0x3000]:
        raise ValueError('Application title or publisher mismatch')
    actual_version = nacp[0x3060:0x3070].split(b'\0')[0].decode()
    if actual_version != version:
        raise ValueError('Application version mismatch')
    if struct.unpack_from('<Q', nacp, 0x3038)[0] != TITLE_ID or struct.unpack_from('<Q', nacp, 0x3078)[0] != TITLE_ID:
        raise ValueError('NACP application/save owner ID mismatch')

    cnmt_files = list((meta / 'section0').glob('*.cnmt'))
    if len(cnmt_files) != 1:
        raise ValueError('Missing application content metadata')
    cnmt = cnmt_files[0].read_bytes()
    if len(cnmt) != 192 or struct.unpack_from('<Q', cnmt)[0] != TITLE_ID or cnmt[12] != 0x80:
        raise ValueError('Invalid application CNMT')
    extended, count, meta_count = struct.unpack_from('<HHH', cnmt, 14)
    if extended != 16 or count != 2 or meta_count != 0:
        raise ValueError('Unexpected CNMT content entries')
    for i, kind in enumerate(('Program', 'Control')):
        at = 32 + extended + 56 * i
        item = types[kind]['entry']
        if (cnmt[at:at+32].hex() != item['sha256'] or
                cnmt[at+32:at+48].hex() != item['name'][:32] or
                int.from_bytes(cnmt[at+48:at+54], 'little') != item['bytes'] or
                cnmt[at+54] != (1 if kind == 'Program' else 3) or cnmt[at+55] != 0):
            raise ValueError('CNMT content hash/size/type mismatch')
    result = dict(passed=True, package=str(package), bytes=len(payload), sha256=sha(payload),
                  title_id=f'{TITLE_ID:016x}', nacp_version=actual_version,
                  cnmt_version=struct.unpack_from('<I', cnmt, 8)[0],
                  entries=entries, program_matches_elf=True,
                  program_sha256=sha(actual_main.read_bytes()), elf_sha256=sha((build_dir/'j2mevm.elf').read_bytes()),
                  romfs_files=len(expected_assets), romfs_unchanged=True, icons_and_nacp_verified=True,
                  npdm_matches_configuration=True, all_content_integrity_verified=True,
                  unsigned_homebrew_fields=unsigned, physical_switch_tested=False,
                  tool_sha256={p.name:sha(p.read_bytes()) for p in [hactool, dkp/'tools/bin/elf2nso', dkp/'tools/bin/npdmtool']})
    (output / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main():
    import build
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--keyset', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, default=build.BUILD/'switch')
    parser.add_argument('--control-dir', type=Path, default=build.BUILD/'nsp/control')
    parser.add_argument('--npdm-config', type=Path, default=build.ROOT/'tools/nsp/npdm.json')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', default=build.NACP_VERSION)
    args = parser.parse_args()
    dkp = Path(os.environ['DEVKITPRO']).resolve()
    result = verify_package(args.package.resolve(), args.build_dir.resolve(), args.control_dir.resolve(),
                            args.npdm_config.resolve(), dkp, args.keyset.resolve(), args.output.resolve(), args.version)
    print('PASS NSP', result['title_id'], result['nacp_version'], 'ROMFS files=' + str(result['romfs_files']))


if __name__ == '__main__':
    main()
