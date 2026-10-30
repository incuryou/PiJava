#!/usr/bin/env python3
"""Per-launch 3D diagnostics. Cumulative counters are differenced; draw completions
are not screen presentations, and neither is asserted to be game-logic FPS."""
import argparse,json,re
from pathlib import Path
LINE=re.compile(r'^\[(\d+)\]\s+(.*)$')
PAIR=re.compile(r'(\w+)=([^\s]+)')
CUMULATIVE={'M3GROW','M3GSPAN','M3GSPRITE','M3GPREP','M3GCOVERAGE','M3GSTAGE'}
IGNORE={'schema','enabled','texture_native','sample_period','bounded_pixels','max_pixels','max_sample_us','texture_only','max_span'}

def analyze(text):
    sessions=[];last={};current=None;boot=None;configuration={}
    for number,line in enumerate(text.splitlines(),1):
        m=LINE.match(line)
        if not m:continue
        timestamp=int(m[1]);body=m[2];kind=body.split(' ',1)[0]
        if body.startswith('==== BOOT'):
            boot=body;last={};current=None;configuration={}
        if kind in {'M3G_STAGE','M3G_STAGE241'}:configuration[kind]=dict(PAIR.findall(body))
        if body.startswith('LAUNCH begin ') or body.startswith('CONTINUE begin '):
            current={'boot':boot,'configuration':dict(configuration),'launch':body,'start_ms':timestamp,'line':number,'last_ms':timestamp,
                     'counters':{},'stats':[],'cpu':[],'hotspots':[],'warnings':[],'audio_gap_max_ms':0}
            sessions.append(current)
        pairs=dict(PAIR.findall(body))
        if kind in CUMULATIVE:
            for key,v in pairs.items():
                if key in IGNORE or not v.isdigit() or key.startswith('max_'):continue
                name=kind+'.'+key;value=int(v);prior=last.get(name,0)
                delta=value-prior if value>=prior else value
                if current:
                    current['counters'][name]=current['counters'].get(name,0)+delta
                    if value<prior:current['warnings'].append({'line':number,'counter_reset':name})
                last[name]=value
        if not current:continue
        current['last_ms']=timestamp
        if kind=='STAT':
            row={'timestamp_ms':timestamp,'line':number}
            for k in ['window_ms','canvas_fps','canvas_paints','canvas_paint_avg_ms','canvas_paint_max_ms','presents','present_avg_ms']:
                try:row[k]=float(pairs[k])
                except (KeyError,ValueError):pass
            if 'window_ms' in row and 'canvas_paints' in row:current['stats'].append(row)
        elif kind=='CPUSTAT':
            row={}
            for k in ['window_ms','host_thread_cpu_ms','paint_inclusive_cpu_ms','gc_cpu_ms']:
                try:row[k]=float(pairs[k])
                except (KeyError,ValueError):pass
            current['cpu'].append(row)
        elif kind=='AUDIOSTAT':
            try:current['audio_gap_max_ms']=max(current['audio_gap_max_ms'],float(pairs['pump_gap_max_ms']))
            except (KeyError,ValueError):pass
        elif kind in {'PAINTHOT','PAINTSLOW','PAINTHANDOFF','PAINTCHECK','VMFLOAT'}:
            current['hotspots'].append({'line':number,'timestamp_ms':timestamp,'record':body})
        if 'SAVE invalid' in body or 'SAVE apply failed' in body or body.startswith('EX '):
            current['warnings'].append({'line':number,'record':body})
        if body.startswith('LIFECYCLE destroyed') or 'back-to-menu' in body:current=None
    for session in sessions:
        rows=session['stats'];slow=[r for r in rows if r.get('canvas_paint_avg_ms',0)>=100]
        summary={}
        for group,selected in [('all',rows),('slow_paint_windows',slow)]:
            window=sum(r.get('window_ms',0) for r in selected);draws=sum(r.get('canvas_paints',0) for r in selected)
            summary[group]={'windows':len(selected),'window_ms':round(window,3),'completed_draws':draws,
                'presents':sum(r.get('presents',0) for r in selected),
                'paint_mean_ms':round(sum(r.get('canvas_paint_avg_ms',0)*r['canvas_paints'] for r in selected)/draws,3) if draws else None,
                'paint_max_ms':max((r.get('canvas_paint_max_ms',0) for r in selected),default=None),
                'draw_completions_per_s':round(1000*draws/window,3) if window else None,
                'presentations_per_s':round(1000*sum(r.get('presents',0) for r in selected)/window,3) if window else None}
        session['summary']=summary
    return {'scope':'uploaded log evidence; CPU scopes overlap; sampled native durations are not full-frame cost; logic FPS unavailable', 'sessions':sessions}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('logs',nargs='+',type=Path);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
    result={str(p):analyze(p.read_text(errors='replace')) for p in a.logs}
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    for path,r in result.items():
        print(path)
        for s in r['sessions']:print(s['launch'],json.dumps(s['summary'],ensure_ascii=False))
if __name__=='__main__':main()
