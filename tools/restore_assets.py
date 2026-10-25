#!/usr/bin/env python3
"""Restore unchanged font dependencies from the user's original source archive.

No asset is downloaded. Only manifest-listed files with matching SHA-256 are
copied to fixed relative paths. A complete source directory is accepted too.
"""
import argparse,hashlib,json,os,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('original',type=Path);a=p.parse_args()
    entries=json.loads((ROOT/'assets-reuse.json').read_text())['files'];archive=None
    try:
        if not a.original.is_dir():archive=zipfile.ZipFile(a.original)
        staged=[]
        for e in entries:
            rel=e['path'];dest=ROOT/rel
            if dest.is_file() and hashlib.sha256(dest.read_bytes()).hexdigest()==e['sha256']:continue
            if archive:
                candidates=[x for x in archive.namelist() if x==rel or x.endswith('/'+rel)]
                candidates=[x for x in candidates if hashlib.sha256(archive.read(x)).hexdigest()==e['sha256']]
                if len(candidates)!=1:raise ValueError('missing or ambiguous matching original asset: '+rel)
                data=archive.read(candidates[0])
            else:data=(a.original/rel).read_bytes()
            if hashlib.sha256(data).hexdigest()!=e['sha256']:raise ValueError('original asset digest mismatch: '+rel)
            staged.append((dest,data))
        for dest,data in staged:
            dest.parent.mkdir(parents=True,exist_ok=True);tmp=dest.with_name(dest.name+'.restore-tmp')
            tmp.write_bytes(data);os.replace(tmp,dest)
        print('Original asset dependencies verified:',len(entries),'restored:',len(staged))
    finally:
        if archive:archive.close()
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,zipfile.BadZipFile) as e:print('Asset restore failed:',e,file=sys.stderr);raise SystemExit(1)
