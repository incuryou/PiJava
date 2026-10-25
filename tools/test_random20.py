#!/usr/bin/env python3
"""Freeze a conservative non-3D random sample, then explicitly run that fixed list.

Preparation does not start a VM. Game bytes remain private external test input.
"""
import argparse,concurrent.futures,hashlib,io,json,os,random,re,subprocess,sys,time,zipfile
from pathlib import Path
from audit_jar import parse_class,Reader,modified_utf8
SEED=20261020193438
API3D=('javax/microedition/m3g','mascotcapsule','micro3d','superscape','swerve','com/jblend/graphics/j3d','com/j_phone/amuse')
TITLE3D=re.compile(r'(?i)(?:3[\s_-]*d|３Ｄ|三维|立体|浴火银河|galaxy\s*on\s*fire|deep\s*3d)')
PERSPECTIVE=re.compile(r'(?i)(?:赛车|飙车|狂飙|竞速|拉力|极品飞车|摩托|机车|滑雪|高尔夫|台球|保龄球|足球|篮球|网球|飞行|空战|银河|asphalt|racing|rally|skiing|golf|billiard|bowling|football|soccer|tennis|flight)')
GEOMETRY=re.compile(r'(?i)(?:vector[34]|quaternion|perspective|rasterizer|renderer3d|model3d|transform3d|vertexbuffer|indexbuffer|projectionmatrix|meshloader)')
MODELS=('.m3g','.m3d','.bac','.tra','.obj','.3ds','.msh','.mesh','.ase','.vtx','.mtl')
TRIG={'sin','cos','tan','asin','acos','atan','atan2'}
PIXEL={'drawRGB','fillTriangle','createRGBImage'}
RASTER=(b'\x89PNG\r\n\x1a\n',b'\xff\xd8\xff',b'GIF87a',b'GIF89a',b'BM')
def sha(data):return hashlib.sha256(data).hexdigest()
def dump(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
def manifest(data):
 text=data.decode('utf-8',errors='replace').replace('\r\n','\n').replace('\r','\n');rows=[]
 for line in text.splitlines():
  if line.startswith(' ')and rows:rows[-1]+=line[1:]
  else:rows.append(line)
 return dict(line.split(': ',1)for line in rows if ': 'in line)
def utf_constants(data):
 r=Reader(data);r.take(8);n=r.u2();out=[];i=1
 while i<n:
  tag=r.u1()
  if tag==1:out.append(modified_utf8(r.take(r.u2())))
  elif tag in(3,4):r.take(4)
  elif tag in(5,6):r.take(8);i+=1
  elif tag in(7,8,16,19,20):r.take(2)
  elif tag in(9,10,11,12,17,18):r.take(4)
  elif tag==15:r.take(3)
  else:raise ValueError('constant tag '+str(tag))
  i+=1
 return out

def inspect_jar(data,origins):
 row={'sha256':sha(data),'bytes':len(data),'origins':origins,'readable_jar':False,'excluded_reasons':[]}
 try:
  with zipfile.ZipFile(io.BytesIO(data))as z:
   bad=z.testzip()
   if bad:raise ValueError('CRC failure: '+bad)
   entries=z.infolist();mf=next((x for x in entries if x.filename.upper()=='META-INF/MANIFEST.MF'),None)
   attrs=manifest(z.read(mf))if mf else{};row['manifest']=attrs
   classes=[];errors=[];refs=set();members=set();strings=[];raster=[];models=[]
   for item in entries:
    if item.is_dir():continue
    name=item.filename
    if name.endswith('.class'):
     try:
      content=z.read(item);c=parse_class(content,name);classes.append(c.name);refs.update(c.class_references);members.update((o,n,d)for _,o,n,d in c.member_references);strings.extend(utf_constants(content))
     except Exception as e:errors.append({'entry':name,'error':str(e)})
    else:
     if name.lower().endswith(MODELS):models.append(name)
     with z.open(item)as f:magic=f.read(16)
     if any(magic.startswith(s)for s in RASTER):raster.append(name)
   row['readable_jar']=bool(classes);row['class_count']=len(classes);row['class_errors']=errors
   title=' | '.join([x['entry']for x in origins]+[attrs.get('MIDlet-Name',''),attrs.get('MIDlet-1','')]);row['title']=attrs.get('MIDlet-Name')or origins[0]['entry']
   api=sorted(r for r in refs if any(s in r.lower()for s in API3D))
   names=classes+[x.filename for x in entries]+strings
   geometry=sorted({s for s in names if GEOMETRY.search(s)})[:30]
   title3d=TITLE3D.search(title);perspective=PERSPECTIVE.search(title)
   trig=sorted(f'{o}.{n}{d}'for o,n,d in members if o in('java/lang/Math','java/lang/StrictMath')and n in TRIG)
   pixel=sorted(f'{o}.{n}{d}'for o,n,d in members if n in PIXEL and(o=='javax/microedition/lcdui/Graphics'or o=='javax/microedition/lcdui/Image'))
   sprites=sorted(r for r in refs if r in('javax/microedition/lcdui/game/Sprite','javax/microedition/lcdui/game/TiledLayer','javax/microedition/lcdui/game/LayerManager'))
   draw=sorted(f'{o}.{n}{d}'for o,n,d in members if o=='javax/microedition/lcdui/Graphics'and n in('drawImage','drawRegion'))
   image=sorted(f'{o}.{n}{d}'for o,n,d in members if o=='javax/microedition/lcdui/Image'and n=='createImage')
   row['screening_evidence']={'api3d':api,'title3d_match':title3d.group(0)if title3d else None,'perspective_title_match':perspective.group(0)if perspective else None,'model_assets':models,'geometry_names':geometry,'trig_calls':trig,'uncertain_pixel_calls':pixel,'sprite_tile_apis':sprites,'image_draw_calls':draw,'image_creation_calls':image,'raster_resource_count':len(raster),'raster_resources_sample':raster[:12]}
   reasons=row['excluded_reasons']
   if not classes:reasons.append('no_readable_classfiles')
   if errors:reasons.append('class_parse_errors_prevent_full_render_classification')
   if api:reasons.append('explicit_3d_api')
   if title3d:reasons.append('explicit_3d_title')
   if perspective:reasons.append('conservative_exclusion_perspective_genre_title')
   if models:reasons.append('possible_3d_model_assets')
   if geometry:reasons.append('possible_software_3d_geometry_names')
   if trig:reasons.append('conservative_exclusion_trigonometric_renderer_uncertainty')
   if pixel:reasons.append('conservative_exclusion_pixel_or_triangle_renderer_uncertainty')
   positive=bool(draw and image and raster)
   if not positive:reasons.append('insufficient_positive_2d_image_and_raster_evidence')
   row['positive_2d_basis']='sprite/tile APIs plus image drawing+raster resources'if sprites and positive else'image/region drawing+image creation+raster resources; no reviewed 3D/geometry/pixel/trig/title clues'if positive else None
 except Exception as e:row['excluded_reasons'].append('unreadable_jar_zip');row['read_error']=str(e)
 row['eligible_non3d_candidate']=row['readable_jar']and not row['excluded_reasons']
 return row

def prepare(args):
 out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);grouped={};archives=[]
 if(out/'selection.json').exists():raise SystemExit('Refusing to overwrite frozen selection.json; use validate')
 for path in args.archives:
  path=path.resolve();blob=path.read_bytes();archives.append({'path':str(path),'sha256':sha(blob),'bytes':len(blob)})
  with zipfile.ZipFile(io.BytesIO(blob))as z:
   for item in z.infolist():
    if item.is_dir()or not item.filename.lower().endswith('.jar'):continue
    data=z.read(item);digest=sha(data);origin={'archive':str(path),'entry':item.filename}
    if digest not in grouped:grouped[digest]={'bytes':data,'origins':[]}
    grouped[digest]['origins'].append(origin)
 rows=[inspect_jar(grouped[h]['bytes'],grouped[h]['origins'])for h in sorted(grouped)]
 candidates=[r for r in rows if r['eligible_non3d_candidate']]
 if len(candidates)<20:raise SystemExit(f'Only {len(candidates)} conservatively classified candidates; do not relax silently')
 selected=random.Random(SEED).sample(candidates,20)
 private=out/'private-inputs';private.mkdir(exist_ok=True)
 for index,row in enumerate(selected,1):
  path=private/(row['sha256']+'.jar');path.write_bytes(grouped[row['sha256']]['bytes']);row['sample_index']=index;row['private_jar_path']=str(path)
 policy={'version':1,'seed':SEED,'candidate_order':'ascending full SHA-256','sampling_algorithm':'Python random.Random(seed).sample(candidates,20), without replacement','deduplication':'full JAR bytes SHA-256 across both archives','non3d_policy':'Conservative static classification requires positive image/region draw+image creation+raster resource evidence, and excludes explicit 3D APIs/title/model/geometry clues plus uncertain pixel/triangle/trigonometry and perspective-genre titles. Exclusion for uncertainty is not a claim that the game is 3D.','limitation':'Static evidence does not prove all runtime scenes; formal tests must visually review each sampled game. If a missed 3D clue is discovered, report it and retain the frozen sample rather than silently replacing it.','selection_frozen_before_any_vm_test':True,'failures_must_not_be_replaced':True,'formal_cap':60,'duration_seconds':45,'native_followup':'Optional only to diagnose a retained failing60 run; it does not replace the60 result.','private_inputs_not_for_delivery':True}
 result={'policy':policy,'archives':archives,'unique_jar_bytes':len(rows),'readable_unique_jars':sum(r['readable_jar']for r in rows),'eligible_candidates':len(candidates),'excluded_unique_jars':sum(not r['eligible_non3d_candidate']for r in rows),'selected':selected}
 dump(out/'selection.json',result);dump(out/'candidate-screening.json',{'policy':policy,'rows':rows});dump(out/'excluded.json',[r for r in rows if not r['eligible_non3d_candidate']]);dump(out/'candidates-sha-sorted.json',candidates)
 (out/'selection.sha256').write_text(sha((out/'selection.json').read_bytes())+'  selection.json\n')
 print(json.dumps({'unique':len(rows),'readable':result['readable_unique_jars'],'candidates':len(candidates),'selected':[(r['sample_index'],r['title'],r['sha256'][:12])for r in selected]},ensure_ascii=False,indent=2))

def validate(args):
 out=args.output.resolve();selection=json.loads((out/'selection.json').read_text());candidates=json.loads((out/'candidates-sha-sorted.json').read_text())
 actual=[r['sha256']for r in selection['selected']];expected=[r['sha256']for r in random.Random(SEED).sample(candidates,20)]
 assert candidates==sorted(candidates,key=lambda r:r['sha256'])and len(set(actual))==20 and actual==expected
 for r in selection['selected']:assert sha(Path(r['private_jar_path']).read_bytes())==r['sha256']
 digest=sha((out/'selection.json').read_bytes());lock=out/'selection.sha256'
 if lock.exists():assert lock.read_text().split()[0]==digest,'Frozen selection changed'
 else:lock.write_text(digest+'  selection.json\n')
 reasons={}
 for r in json.loads((out/'excluded.json').read_text()):
  for reason in r['excluded_reasons']:reasons[reason]=reasons.get(reason,0)+1
 result={'seed':SEED,'selection_sha256':digest,'draw_reproduced':True,'all_selected_bytes_verified':True,'unique_inputs':selection['unique_jar_bytes'],'readable_unique_jars':selection['readable_unique_jars'],'candidates':len(candidates),'selected':len(actual),'exclusion_reason_counts_overlapping':reasons,'vm_started':False}
 dump(out/'selection-validation.json',result);print(json.dumps(result,ensure_ascii=False,indent=2));return selection

def tree_hash(root):return{str(p.relative_to(root)):sha(p.read_bytes())for p in sorted(root.rglob('*'))if p.is_file()}

def run_one(args,row):
 group='cap'+str(args.cap)+(('-'+args.label)if args.label else'')
 out=args.output.resolve()/group/('%02d-'%row['sample_index']+row['sha256'][:12]);out.mkdir(parents=True,exist_ok=False);(out/'rms').mkdir()
 tape=out/'input.tape';tape.write_text('');decisions=out/'decisions.jsonl';seq=0;sent=set();last_action=-10000;last_release=-10000;fallback=0;handled_manual=set()
 initial=args.output/'initial-controls'/('%02d.json'%row['sample_index'])
 if initial.exists()and args.label=='v2':(out/'control.json').write_bytes(initial.read_bytes())
 command=[str(args.binary.resolve()),row['private_jar_path'],str(args.classes.resolve()),str(out),str(args.cap),str(args.seconds),str(tape)]
 metadata={'sample_index':row['sample_index'],'title':row['title'],'jar_sha256':row['sha256'],'command':command,'cap':args.cap,'duration_seconds_after_launch':args.seconds,'binary_sha256':sha(args.binary.read_bytes()),'selection_sha256':sha((args.output/'selection.json').read_bytes()),'harness_sha256':sha((Path(__file__).resolve().parent.parent/'tests/random20_probe.cpp').read_bytes()),'source_sha256':tree_hash(Path(__file__).resolve().parent.parent/'src'),'api_sha256':tree_hash(args.classes),'parallel_workers':args.workers,'cpu_count':os.cpu_count(),'scope':'Functional headless production VM/shared scheduler. Parallel processes and PNG/check metrics add host load: no performance/FPS claim. CPU presenter host pixels, GPU composition reference, no physical Switch GPU/HID. Independent fresh RMS, no game private field writes, no forced size.','environment':{k:os.environ[k]for k in sorted(os.environ)if k.startswith('J2ME_')}}
 dump(out/'run-metadata.json',metadata)
 def emit(key,at,reason,hold=100):
  nonlocal seq,last_action,last_release
  at=max(at,last_release+150);seq+=2
  with tape.open('a')as f:f.write(f'{seq-1} {int(at)} {key} 0\n{seq} {int(at+hold)} {key} 1\n')
  with decisions.open('a')as f:f.write(json.dumps({'at_ms':at,'key':key,'hold_ms':hold,'reason':reason},ensure_ascii=False)+'\n')
  last_action=at;last_release=at+hold
 start=time.monotonic();timed=False;latest={};launched=False;cpu_seconds=0.;peak_rss_kib=0
 with(out/'console.log').open('w')as log:
  proc=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT)
  while proc.poll()is None:
   try:
    stat=Path(f'/proc/{proc.pid}/stat').read_text().rsplit(')',1)[1].split();cpu_seconds=(int(stat[11])+int(stat[12]))/os.sysconf('SC_CLK_TCK')
    for line in Path(f'/proc/{proc.pid}/status').read_text().splitlines():
     if line.startswith('VmRSS:'):peak_rss_kib=max(peak_rss_kib,int(line.split()[1]))
   except(FileNotFoundError,ProcessLookupError):pass
   if time.monotonic()-start>args.seconds+40:timed=True;proc.kill();break
   progress=out/'progress.jsonl'
   if progress.exists():
    lines=progress.read_text(errors='replace').splitlines()
    for line in reversed(lines):
     try:latest=json.loads(line);break
     except json.JSONDecodeError:continue
   if latest:
    launched=True;at=latest['elapsed_ms'];text=latest.get('text_sample','');lower=text.lower();control={}
    try:control=json.loads((out/'control.json').read_text())
    except(FileNotFoundError,json.JSONDecodeError):pass
    for index,action in enumerate(control.get('actions',[])):
     if index not in handled_manual:
      emit(int(action['key']),max(at+50,action.get('at_ms',at+50)),'human-reviewed normal navigation',int(action.get('hold_ms',100)));handled_manual.add(index)
    if not control.get('manual',False)and at-last_action>=1800:
     # Public text is a hint, not a private game state oracle. Stop automatic
     # confirmation at ambiguous exits/payment screens, leaving live review.
     barrier=any(s in lower for s in('购买','付费','支付','激活','发送短信','订购','buy now','purchase','activation','更多精彩','访问网站'))
     signature=sha(text.encode())
     if not barrier and('音乐'in text or'music'in lower)and any(s in lower for s in('开启','打开','是','否','yes','no','sound'))and('music',signature)not in sent:
      emit(-6,at+50,'public music yes/no prompt');sent.add(('music',signature))
     elif not barrier and any(s in text for s in('按5','按５','按任意键'))and('continue',signature)not in sent:
      emit(53,at+50,'public press5/anykey prompt');sent.add(('continue',signature))
     elif not barrier and'按0'in text and('zero',signature)not in sent:
      emit(48,at+50,'public press0 prompt');sent.add(('zero',signature))
     elif not barrier and any(s in lower for s in('开始游戏','新游戏','新的游戏','start game','new game'))and('start',signature)not in sent:
      emit(53,at+50,'public start/newgame label; one attempt per text');sent.add(('start',signature))
     elif not text and fallback<2 and at>2500+5000*fallback:
      emit(53,at+50,'bounded title advance; no available text; visual review required');fallback+=1
     elif control.get('scene')=='gameplay'and at>=20000 and('direction',int(at//5000))not in sent:
      emit((-3,-4,-1,-2)[int(at//5000)%4],at+50,'human-classified gameplay ordinary direction');sent.add(('direction',int(at//5000)))
   time.sleep(.25)
  proc.wait()
 result={}
 try:result=json.loads((out/'result.json').read_text())
 except(FileNotFoundError,json.JSONDecodeError):pass
 status='host_timeout'if timed else'vm_exception_pending'if result.get('remaining_exception')else'application_ended_early_review_input'if result.get('ended_early')else'bounded_run_completed_visual_review_pending'if proc.returncode==0 else'probe_error_review_logs'
 summary={'sample_index':row['sample_index'],'title':row['title'],'returncode':proc.returncode,'timeout':timed,'host_wall_seconds':time.monotonic()-start,'last_observed_process_cpu_seconds':cpu_seconds,'peak_observed_rss_kib':peak_rss_kib,'resource_samples_are_not_performance_results':True,'launched':launched,'status':status,'result':result,'output':str(out),'visual_review':'pending','scene_reached':'unclassified','clipping_or_centering':'pending human review; host pixel parity alone is insufficient','sample_replaced':False}
 dump(out/'summary.json',summary);dump(out/'human-review.json',{'sample_index':row['sample_index'],'scene_reached':'pending','gameplay_reached':None,'screenshots_reviewed':[],'centering':'pending','clipping':'pending','normal_input':'pending','exceptions_or_limitations':[],'notes':'Review raw and host pairs, public text and actual input callbacks. Startup/menu is not gameplay. A graceful game exit caused by ordinary navigation is not a VM crash.'})
 print(json.dumps({k:summary[k]for k in('sample_index','title','status','returncode','output')},ensure_ascii=False),flush=True);return summary

def run(args):
 if not args.frozen_fix:raise SystemExit('Formal run requires --frozen-fix after parent freezes production fix')
 if not 1<=args.workers<=4:raise SystemExit('workers must be1..4')
 selection=validate(args);indices=set(args.indices or range(1,21));rows=[r for r in selection['selected']if r['sample_index']in indices]
 if len(rows)!=len(indices):raise SystemExit('unknown sample index')
 if not args.binary.is_file()or not args.classes.is_dir():raise SystemExit('missing binary/API classes')
 with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers)as pool:results=list(pool.map(lambda r:run_one(args,r),rows))
 dump(args.output/('batch-cap%d-'%args.cap+'-'.join(str(r['sample_index'])for r in rows)+'.json'),{'results':results,'no_sample_replacement':True,'all_visual_reviews_required':True})

parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='command',required=True)
p=sub.add_parser('prepare');p.add_argument('--archives',nargs='+',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p=sub.add_parser('validate');p.add_argument('--output',type=Path,required=True)
p=sub.add_parser('run');p.add_argument('--output',type=Path,required=True);p.add_argument('--binary',type=Path,required=True);p.add_argument('--classes',type=Path,required=True);p.add_argument('--indices',type=int,nargs='+');p.add_argument('--workers',type=int,default=4);p.add_argument('--cap',type=int,choices=(0,60),default=60);p.add_argument('--label',default='');p.add_argument('--seconds',type=int,choices=(45,90),default=45);p.add_argument('--frozen-fix',action='store_true')
a=parser.parse_args()
if a.command=='prepare':prepare(a)
elif a.command=='validate':validate(a)
elif a.command=='run':run(a)
