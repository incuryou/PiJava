#!/usr/bin/env python3
"""Version/session-aware J2ME logs: weighted totals, missing data remain unknown.

This parser does NOT infer gameplay scenes, logic FPS, physical presentation,
or CPU utilization from worker wall time. Menu pauses are excluded by summing
STAT window durations, rather than subtracting the first/last timestamps.
"""
import argparse, hashlib, json, re
from pathlib import Path

LINE = re.compile(r'^\[(\d+)\] (.*)$')
KV = re.compile(r'(?<!\S)([\w.]+)=([^\s]+)')
TYPES = {'STAT','WORKSTAT','SCHEDSTAT','POLLSTAT','AUDIOSTAT','CPUSTAT','FRAMESTAT','MEMSTAT','AUDIOPOLL'}

def values(text):
    result = {}
    for k,v in KV.findall(text):
        try: result[k] = float(v) if any(c in v for c in '.eE') else int(v)
        except ValueError: result[k] = v
    return result

def parse(path):
    sessions=[]; current=None; version='unknown'; pending={}; selected='unknown'
    for number,line in enumerate(path.read_text(encoding='utf-8',errors='replace').splitlines(),1):
        m=LINE.match(line)
        if not m: continue
        ts=int(m[1]);text=m[2]
        boot=re.search(r'==== BOOT (\S+) ====',text)
        if boot: version=boot[1];current=None;pending={}
        if text.startswith(('CONTINUE begin ','LAUNCH begin ')):
            gm=re.search(r'(?:path=)?(sdmc:/.*?\.jar)(?:\s|$)',text)
            if not gm: continue
            current={'version':version,'jar_path':gm[1],'game':gm[1].rsplit('/',1)[-1],
                     'start_line':number,'start_ms':ts,'launch_type':text.split()[0],
                     'mode_at_launch':values(text).get('speed',selected),'windows':[]}
            sessions.append(current);pending={}
            continue
        if text.startswith('SPEED '):
            selected=values(text).get('cap',selected)
            if current and not current['windows']:current['mode_at_launch']=selected
        kind=text.split(' ',1)[0]
        if kind not in TYPES or current is None: continue
        item={'line':number,'time_ms':ts,'values':values(text)}
        if kind=='STAT':
            # WORK/AUDIO precede STAT in historical logs. Only attach fresh values.
            extras={k:v for k,v in pending.items() if 0<=ts-v['time_ms']<=100}
            current['windows'].append({'stat':item,**extras});pending={}
        elif kind in ('POLLSTAT','CPUSTAT','FRAMESTAT','MEMSTAT','AUDIOPOLL') and current['windows'] and 0<=ts-current['windows'][-1]['stat']['time_ms']<=100:
            current['windows'][-1][kind]=item
        else:pending[kind]=item
    return sessions

def summarize(session):
    windows=session['windows']; ss=[w['stat']['values'] for w in windows]
    def total(field,kind='stat'):
        found=[w[kind]['values'][field] for w in windows if kind in w and isinstance(w[kind]['values'].get(field),(int,float))]
        return {'sum':sum(found) if found else None,'windows_with_field':len(found)}
    def weighted(field,weight):
        found=[s for s in ss if isinstance(s.get(field),(int,float)) and isinstance(s.get(weight),(int,float))]
        den=sum(s[weight] for s in found)
        return sum(s[field]*s[weight] for s in found)/den if den else None
    def rate(field):
        found=[s for s in ss if isinstance(s.get(field),(int,float)) and isinstance(s.get('window_ms'),(int,float))]
        den=sum(s['window_ms'] for s in found)
        return sum(s[field] for s in found)*1000/den if den else None
    out={k:v for k,v in session.items() if k!='windows'}
    out.update(windows=len(windows),observed_window_ms=total('window_ms'),canvas_draw_rate=rate('canvas_paints'),
               cap_budget_rate=rate('cap_budget_frames'),host_submit_rate=rate('presents'),
               paint_mean_ms=weighted('canvas_paint_avg_ms','canvas_paints'),present_mean_ms=weighted('present_avg_ms','presents'),
               modes=sorted({str(s.get('speed','unknown')) for s in ss}),clock_scales=sorted({s['game_clock_scale'] for s in ss if 'game_clock_scale' in s}),
               worker_wall_ms=total('worker_ms_sum'),worker_remaining_wall_ms=total('worker_remaining_wall_ms','WORKSTAT'),
               poll_wait_ms=total('worker_pollwait_ms','WORKSTAT'),gc_wall_ms=total('gc_ms','WORKSTAT'),
               audio_queue_empty_risk=total('queue_empty_risk','AUDIOSTAT'),
               actual_cpu_observed=any('CPUSTAT' in w for w in windows),
               scene='unlabeled; may include loading/menu/dialogue; not a matched-scene benchmark')
    for field in ('canvas_fps','game_clock_scale'):
        vs=[s[field] for s in ss if isinstance(s.get(field),(int,float))]
        out[field+'_range']=[min(vs),max(vs)] if vs else None
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('logs',nargs='+',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    a.out.mkdir(parents=True,exist_ok=True);all_rows=[];raw=[];manifest=[]
    for path in a.logs:
        data=parse(path)
        for i,s in enumerate(data):
            s.update(file=path.name,session=i+1);raw.append(s);all_rows.append(summarize(s))
        manifest.append({'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'sessions':len(data)})
    policy={'version':1,'scope':'Historical windows only; no matched-scene or hardware CPU inference',
            'unknown_fields':'null, never converted to zero','aggregation':'count / summed STAT window seconds; paint/submit weighted by count',
            'game_identity':'path only in historical logs; not assumed equal across names or versions',
            'post_restore':'first partial/loading windows retained and labeled, not silently discarded'}
    (a.out/'windows.json').write_text(json.dumps({'policy':policy,'sessions':raw},ensure_ascii=False,indent=2)+'\n')
    (a.out/'summary.json').write_text(json.dumps({'policy':policy,'inputs':manifest,'sessions':all_rows},ensure_ascii=False,indent=2)+'\n')
    lines=['# Historical log coverage','',policy['scope'],'','|Log|Version|Game / session|Windows|Draws/s|Paint ms|CPU measured|','|---|---|---|---:|---:|---:|---|']
    for s in all_rows:
        fmt=lambda v:'unknown' if v is None else f'{v:.3f}'
        lines.append(f"|{s['file']}|{s['version']}|{s['game']} / {s['session']}|{s['windows']}|{fmt(s['canvas_draw_rate'])}|{fmt(s['paint_mean_ms'])}|{s['actual_cpu_observed']}|")
    lines+=['','All rows include their logged loading/menu/scene transitions. Do not compare row averages as an optimization A/B test.','Poll/worker decomposition absent in earlier versions stays null. Array payload is not total RAM.','']
    (a.out/'README.md').write_text('\n'.join(lines))
    print(json.dumps({'logs':len(manifest),'sessions':len(all_rows),'windows':sum(s['windows'] for s in all_rows),'out':str(a.out)}))
if __name__=='__main__':main()
