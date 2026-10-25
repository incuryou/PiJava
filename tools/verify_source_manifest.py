#!/usr/bin/env python3
"""Check every delivered source file; includes the restored unchanged font dependencies."""
import argparse
import hashlib
import json
from pathlib import Path

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    ap.add_argument('--nro',type=Path,help='also compare an explicitly supplied NRO with the release hash')
    a=ap.parse_args();manifest=json.loads((a.root/'SOURCE-MANIFEST.json').read_text())
    problems=[]
    for name,expected in manifest['files'].items():
        path=a.root/name
        if not path.is_file():problems.append('missing: '+name)
        elif path.stat().st_size!=expected['bytes'] or digest(path)!=expected['sha256']:
            problems.append('changed: '+name)
    if a.nro and (not a.nro.is_file() or digest(a.nro)!=manifest['nro_sha256']):
        problems.append('NRO does not match release hash')
    if problems:
        for problem in problems:print(problem)
        raise SystemExit(1)
    print('PASS',manifest['version'],'source files',len(manifest['files']),
          '(manifest itself excluded); NRO checked' if a.nro else '(manifest itself excluded)')
if __name__=='__main__':main()
