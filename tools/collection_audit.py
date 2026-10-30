#!/usr/bin/env python3
"""Inventory supplied JARs unchanged; static references are requirements, not coverage."""
import argparse,collections,hashlib,json,sys,zipfile
from pathlib import Path
from audit_jar import Auditor
p=argparse.ArgumentParser();p.add_argument('collection',type=Path);p.add_argument('--out',type=Path,required=True);p.add_argument('--classes',type=Path,default=Path(__file__).resolve().parents[1]/'romfs/classes');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
rows=[];missing=collections.Counter();methods=collections.Counter();vendors=collections.Counter()
for n,jar in enumerate(sorted(a.collection.rglob('*.jar')),1):
 row={'id':f'jar{n:03}','name':jar.name,'path':str(jar.resolve()),'sha256':hashlib.sha256(jar.read_bytes()).hexdigest(),'bytes':jar.stat().st_size}
 try:
  audit=Auditor(jar,a.classes);report=audit.audit();game=audit.game
  refs=sorted({(o,n,d) for c in game.values() for tag,o,n,d in c.member_references if tag!=9})
  classes=sorted(set.union(*(c.class_references for c in game.values()))) if game else []
  external=sorted({c for c in classes if c not in game})
  vendor=sorted({c for c in external if c.startswith(('com/nokia/','com/samsung/','com/siemens/','com/motorola/','com/sonyericsson/','com/mascotcapsule/','com/vodafone/','com/j_phone/','com/nttdocomo/','mmpp/','com/skt/','com/sprintpcs/'))})
  tags=[]
  if any(c.startswith('javax/microedition/m3g/') for c in external):tags.append('M3G_JSR184')
  if any(c.startswith('com/mascotcapsule/') for c in external):tags.append('MascotCapsule')
  if any(c=='javax/microedition/lcdui/game/GameCanvas' for c in external):tags.append('GameCanvas')
  if any(n=='serviceRepaints' for o,n,d in refs):tags.append('Canvas_sync')
  if any(n=='repaint' for o,n,d in refs):tags.append('Canvas_async')
  if any(o=='java/lang/Thread' and n=='sleep' for o,n,d in refs):tags.append('sleep_pacing')
  if any(o=='java/lang/Thread' and n=='yield' for o,n,d in refs):tags.append('yield')
  if any(o=='java/lang/System' and n=='currentTimeMillis' for o,n,d in refs):tags.append('clock_reads')
  if 'java/util/Timer' in external:tags.append('Timer')
  if any(n=='getGraphics' and o=='javax/microedition/lcdui/Image' for o,n,d in refs):tags.append('offscreen_2D')
  if any(n=='drawRGB' for o,n,d in refs):tags.append('pixel_buffer_2D')
  if any(n=='wait' for o,n,d in refs):tags.append('monitor_wait')
  with zipfile.ZipFile(jar) as z:
   crc=z.testzip();manifest=z.read('META-INF/MANIFEST.MF').decode('utf8','replace') if 'META-INF/MANIFEST.MF' in z.namelist() else ''
   resources=collections.Counter(Path(s).suffix.lower() for s in z.namelist() if not s.endswith('.class'))
  row.update(tags=tags,vendor_classes=vendor,external_classes=external,game_classes=len(game),manifest=manifest,resource_extensions=dict(resources),summary=report['summary'],abi_complete=report['abi_complete'],zip_bad_crc=crc,missing_classes=[r['name'] for r in report['missing_classes']],unresolved_methods=report['unresolved_methods'],unresolved_fields=report['unresolved_fields'])
  (a.out/(row['id']+'-abi.json')).write_text(json.dumps(report,ensure_ascii=True,indent=2))
  missing.update(row['missing_classes']);methods.update(o['owner']+'.'+o['name']+o['descriptor'] for o in row['unresolved_methods']);vendors.update(vendor)
 except Exception as e:row['audit_error']=type(e).__name__+': '+str(e)
 rows.append(row)
(a.out/'inventory.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2))
(a.out/'requirements.json').write_text(json.dumps({'games':len(rows),'unique':len(set(r['sha256'] for r in rows)),'missing_classes':missing.most_common(),'unresolved_methods':methods.most_common(),'vendor_classes':vendors.most_common()},ensure_ascii=False,indent=2))
print('games',len(rows),'unique',len(set(r['sha256'] for r in rows)),'abi complete',sum(r.get('abi_complete',False) for r in rows));print('missing',missing.most_common(35));print('methods',methods.most_common(35));print('tags',collections.Counter(t for r in rows for t in r.get('tags',[])));print('vendors',vendors.most_common(25))
