#!/usr/bin/env python3
"""Separate games, modes and renderers; never merge unlike workloads into FPS gains."""
import argparse,json,re,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('log',type=Path);p.add_argument('out',type=Path);a=p.parse_args()
sessions=[];cur=None;errors=[];version=None
for n,line in enumerate(a.log.read_text(errors='replace').splitlines(),1):
 if '==== BOOT' in line:version=line.split('BOOT ')[-1].split(' ')[0]
 m=re.search(r'(?:CONTINUE begin|LAUNCH begin) (.*)',line)
 if m:
  tail=m.group(1);jar=re.search(r'([^/]+\.jar)(?:\s|$)',tail)
  cur=dict(name=jar.group(1) if jar else tail,line=n,version=version,windows=[],errors=[]);sessions.append(cur)
 if any(t in line for t in ['EX uncaught','SAVE invalid state','SAVE apply failed','class not found','FATAL','GPU disabled']):
  e=dict(line=n,text=line);errors.append(e)
  if cur:cur['errors'].append(e)
 if cur and re.search(r'\] STAT ',line):
  vals=dict(re.findall(r'([\w.]+)=([^ ]+)',line));vals['line']=n;cur['windows'].append(vals)
for s in sessions:
 vals=s['windows'];s['summary']={}
 for key in ['canvas_fps','cap_budget_fps','canvas_paint_avg_ms','present_avg_ms','present_max_ms','worker_ms_sum','game_clock_scale']:
  v=[float(r[key]) for r in vals if key in r]
  if v:s['summary'][key]=dict(min=min(v),median=statistics.median(v),max=max(v))
 s['modes']=sorted(set(v.get('speed','unknown') for v in vals));s['renderers']=sorted(set(v.get('renderer','unknown') for v in vals))
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(dict(source=str(a.log),version=version,sessions=sessions,errors=errors,note='Within-session mixed scenes; no paired performance inference. CPU scopes overlap, no summation.'),ensure_ascii=False,indent=2))
for s in sessions:print(s['name'],len(s['windows']),s['modes'],s['summary'].get('canvas_fps'),len(s['errors']))
