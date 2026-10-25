#!/usr/bin/env python3
"""Restore complete 16-row Unifont CJK glyphs; retain the existing repertoire."""
import gzip
import hashlib
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'third_party/unifont/unifont-16.0.04.hex.gz'
SOURCE_SHA256 = 'f9c8c7802453f47be02677176aeac2342ee96d354fad7a26cedcce48e68e1d9f'

def rebuild():
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == SOURCE_SHA256
    glyphs = {}
    with gzip.open(SOURCE, 'rt', encoding='ascii') as stream:
        for line in stream:
            code, bitmap = line.strip().split(':')
            glyphs[int(code, 16)] = bytes.fromhex(bitmap)
    target = ROOT / 'romfs/font/cjk16.bin'
    data = target.read_bytes()
    count = struct.unpack_from('<I', data)[0]
    assert len(data) == 4 + count * 34
    output = bytearray(data)
    changed = 0
    for i in range(count):
        offset = 4 + i * 34
        code = struct.unpack_from('<H', data, offset)[0]
        bitmap = glyphs.get(code)
        # Preserve non-CJK legacy glyphs and all codepoint/record identities.
        # Each upstream row belongs at its own y coordinate: no [2:] slice.
        if 0x2e80 <= code <= 0x9fff and bitmap is not None and len(bitmap) == 32:
            changed += data[offset + 2:offset + 34] != bitmap
            output[offset + 2:offset + 34] = bitmap
    target.write_bytes(output)
    print('CJK records:', count, 'changed:', changed,
          'sha256:', hashlib.sha256(output).hexdigest())

if __name__ == '__main__':
    rebuild()
