#!/usr/bin/env python3
"""Check complete CJK outlines against the pinned upstream font, not metrics."""
import gzip
from pathlib import Path
import struct
import sys

root=Path(__file__).resolve().parents[1]
font=Path(sys.argv[1]) if len(sys.argv)>1 else root/'romfs/font/cjk16.bin'
source={}
with gzip.open(root/'third_party/unifont/unifont-16.0.04.hex.gz','rt',encoding='ascii') as f:
    for line in f:
        code,bits=line.strip().split(':');source[int(code,16)]=bytes.fromhex(bits)
b=font.read_bytes();n=struct.unpack_from('<I',b)[0]
assert len(b)==4+n*34
seen=[];bad=[];checked=0
for i in range(n):
    at=4+i*34;cp=struct.unpack_from('<H',b,at)[0];seen.append(cp)
    if 0x2e80<=cp<=0x9fff and cp in source and len(source[cp])==32:
        checked+=1
        if b[at+2:at+34]!=source[cp]:bad.append(cp)
assert seen==sorted(set(seen))
if bad:
    print('FAIL incomplete/different CJK glyphs:',len(bad),'examples:',','.join('U+%04X'%c for c in bad[:15]));raise SystemExit(1)
print('PASS complete CJK glyphs:',checked,'records:',n)
