#!/usr/bin/env python3
"""Supplemental conservative static feature requirements; not execution coverage."""
import argparse,json
from pathlib import Path
from audit_jar import Auditor
p=argparse.ArgumentParser();p.add_argument('inventory',type=Path);p.add_argument('out',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[1];rows=[]
for r in json.loads(a.inventory.read_text()):
 row=dict(id=r['id'],name=r['name'],sha256=r['sha256'])
 try:
  auditor=Auditor(Path(r['path']),root/'romfs/classes');auditor.load_jar();g=auditor.game
  methods=[(c.name,m.name,m.descriptor) for c in g.values() for m in c.methods.values()]
  refs=set(c for v in g.values() for c in v.class_references)
  row['requirements']={
   'pointer_callbacks_declared':any(n.startswith('pointer') and d=='(II)V' for _,n,d in methods),
   'keyRepeated_declared':any(n=='keyRepeated' and d=='(I)V' for _,n,d in methods),
   'RMS_referenced':any(c.startswith('javax/microedition/rms/') for c in refs),
   'MMAPI_referenced':any(c.startswith('javax/microedition/media/') for c in refs),
   'Bluetooth_referenced':any(c.startswith('javax/bluetooth/') for c in refs),
   'Messaging_referenced':any(c.startswith('javax/wireless/messaging/') for c in refs),
   'FileConnection_referenced':any(c.startswith('javax/microedition/io/file/') for c in refs),
   'Connector_referenced':'javax/microedition/io/Connector' in refs,
  }
 except Exception as e:row['error']=str(e)
 rows.append(row)
a.out.write_text(json.dumps(dict(scope='Static declarations/references, NOT exercised branches or required gameplay paths',rows=rows),ensure_ascii=False,indent=2))
print({k:sum(r.get('requirements',{}).get(k,False) for r in rows) for k in rows[0]['requirements']})
