#!/usr/bin/env python3
"""Summarize final cumulative P1 records per session/launch; no gameplay verdict."""
import argparse,json,math,re
from pathlib import Path
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('log',type=Path);ap.add_argument('--out',type=Path,required=True)
a=ap.parse_args();launches={}
for number,line in enumerate(a.log.read_text(errors='replace').splitlines(),1):
    if 'P1STAT schema=1 ' not in line:continue
    row=dict(re.findall(r'([A-Za-z_][A-Za-z_0-9]*)=([^\s]+)',line))
    key=(row.get('session'),row.get('launch'));row['line']=number;launches[key]=row
rows=[]
for key,row in launches.items():
    try:
        counts=list(map(int,row['paint_hist'].split(',')));bounds=row['paint_hist_upper_us'].split(',')
        count=sum(counts);reported=int(row['paint_count']);assert count==reported and len(bounds)==len(counts)
        def quantile(q):
            target=math.ceil(count*q);running=0
            if count==0:return None
            for n,upper in zip(counts,bounds):
                running+=n
                if running>=target:return None if upper=='overflow' else int(upper)
        rows.append(dict(session=key[0],launch=key[1],source_line=row['line'],paint_count=count,
            paint_sum_us=int(row['paint_sum_us']),paint_max_us=int(row['paint_max_us']),
            paint_p95_upper_us=quantile(.95),paint_p99_upper_us=quantile(.99),
            paint_hist=counts,paint_hist_upper_us=bounds,overflow_count=counts[-1],
            presents=int(row['presents']),duplicate_presents=int(row['duplicate_presents']),
            dropped=int(row['dropped']),logic_fps=None,device_play_verified=None,raw=row))
    except (KeyError,ValueError,AssertionError) as exc:raise SystemExit(f'invalid P1STAT line {row["line"]}: {exc}')
a.out.parent.mkdir(parents=True,exist_ok=True)
a.out.write_text(json.dumps(dict(schema_version=1,scope='last launch-cumulative record; never merge repeated windows',
    source=str(a.log),launches=rows,device_verdict=None),ensure_ascii=False,indent=2)+'\n')
print(f'{len(rows)} launch records -> {a.out}')
