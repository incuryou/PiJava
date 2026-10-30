#!/usr/bin/env python3
"""P2.2: regenerate SOURCE-MANIFEST.json from the current tree (same inventory policy as package_source.py).
Usage: regen_source_manifest.py --version V --nro PATH ; then verify with verify_source_manifest.py --nro PATH.
Required check in every P2 step delivery (commander ruling, 2026-10-13)."""
import argparse, hashlib, json
from pathlib import Path
import package_source as ps
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
ap.add_argument('--version',required=True); ap.add_argument('--nro',type=Path,required=True)
a=ap.parse_args()
files=ps.inventory(a.root,source_policy=True)
m={'files':{k:ps.metadata(v) for k,v in sorted(files.items())},'inventory_policy':ps.POLICY,
   'nro_sha256':hashlib.sha256(a.nro.read_bytes()).hexdigest(),'version':a.version}
(a.root/ps.SELF_MANIFEST).write_text(json.dumps(m,indent=2,sort_keys=True)+'\n')
print('WROTE',len(m['files']),'files version',a.version,'nro',m['nro_sha256'])
