#!/usr/bin/env python3
"""Pin reviewed source semantics and emitted class bytes for safe intrinsic binding."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent

def generate():
    pins=json.loads((ROOT/'tools/reviewed_library_sources.json').read_text())
    lines=['#pragma once','// Generated from reviewed sources plus the actual Java compiler output.',
           'struct ReviewedLibraryClass { const char* name; const char* sha256; unsigned char id; };',
           'static const ReviewedLibraryClass kReviewedLibraryClasses[] = {']
    for i,(name,source_hash) in enumerate(pins.items(),1):
        source=ROOT/'java'/(name+'.java'); compiled=ROOT/'romfs/classes'/(name+'.class')
        if hashlib.sha256(source.read_bytes()).hexdigest()!=source_hash:
            raise RuntimeError('Class-library intrinsic contract changed; review before rebinding: '+name)
        digest=hashlib.sha256(compiled.read_bytes()).hexdigest()
        lines.append('    {"%s","%s",%d},'%(name,digest,i))
    lines+=['};','']
    (ROOT/'src/runtime/reviewed_library_manifest.h').write_text('\n'.join(lines))
if __name__=='__main__':generate()
