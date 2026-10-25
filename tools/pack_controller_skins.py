#!/usr/bin/env python3
"""Pack only the three approved blank controller images and their label anchors."""
import argparse
import hashlib
from io import BytesIO
import json
from pathlib import Path
import struct
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
APERTURES={'ivory':(541,77,1128,837),'blue':(562,45,1111,864),'black':(552,78,1120,834)}
# Centers were measured on the approved 1672x941 drawings. Labels and small
# physical tags undergo exactly the same piecewise mapping as the housing.
CENTERS={
 'ivory':[("ZL",270,103),("L",245,174),("Minus",436,222),("Menu",253,362),("DUp",322,532),("DLeft",241,608),("DRight",398,608),("DDown",322,685),("ZR",1400,103),("R",1429,174),("Plus",1233,224),("X",1407,298),("Y",1309,390),("A",1507,388),("B",1405,482),("Zero",1324,644)],
 'blue':[("ZL",270,88),("L",244,166),("Minus",433,228),("Menu",271,365),("DUp",329,535),("DLeft",245,618),("DRight",414,619),("DDown",331,704),("ZR",1406,90),("R",1430,167),("Plus",1240,228),("X",1403,304),("Y",1298,391),("A",1507,389),("B",1404,483),("Zero",1323,654)],
 'black':[("ZL",269,101),("L",241,176),("Minus",430,229),("Menu",253,365),("DUp",321,532),("DLeft",239,607),("DRight",398,608),("DDown",322,685),("ZR",1405,107),("R",1431,177),("Plus",1235,229),("X",1407,300),("Y",1308,390),("A",1508,389),("B",1407,481),("Zero",1319,647)]}

def transform(value,source,target):
    for i in range(3):
        if value<=source[i+1]:return target[i]+(value-source[i])*(target[i+1]-target[i])/(source[i+1]-source[i])
    raise ValueError('anchor outside source')

def pack(name,preview):
    source_path=ROOT/'assets/phone-skins'/('controller-'+name+'.png')
    with Image.open(source_path) as opened:source=opened.convert('RGB')
    if source.size!=(1672,941):raise ValueError('approved geometry changed: '+name)
    l,t,r,b=APERTURES[name];xs=(0,l,r,1672);ys=(0,t,b,941);dx=(0,400,880,1280);dy=(0,40,680,720)
    frame=Image.new('RGB',(1280,720))
    for row in range(3):
        for col in range(3):
            if row==col==1:continue
            crop=source.crop((xs[col],ys[row],xs[col+1],ys[row+1])).resize((dx[col+1]-dx[col],dy[row+1]-dy[row]),Image.Resampling.LANCZOS)
            frame.paste(crop,(dx[col],dy[row]))
    assert frame.crop((400,40,880,680)).tobytes()==bytes(480*640*3)
    halves=[]
    for x in (0,640):
        out=BytesIO();frame.crop((x,0,x+640,720)).save(out,format='JPEG',quality=80,subsampling=0,optimize=True,progressive=False)
        data=out.getvalue();assert 0<len(data)<=512*1024 and data[-2:]==b'\xff\xd9';halves.append(data)
    output=ROOT/'romfs/skins'/('controller-'+name+'.skin')
    output.write_bytes(struct.pack('<4sHHIII',b'J2SJ',1280,720,len(halves[0]),len(halves[1]),0)+b''.join(halves))
    anchors=[]
    for key,x,y in CENTERS[name]:
        shoulder=key in ('ZL','L','ZR','R');stick=key in ('Menu','Zero');tiny=key in ('Plus','Minus')
        bw,bh=(130,42) if shoulder else (140,75) if stick else (65,50) if tiny else (85,60) if key.startswith('D') else (100,58)
        x0=round(transform(x-bw/2,xs,dx));x1=round(transform(x+bw/2,xs,dx));y0=round(transform(y-bh/2,ys,dy));y1=round(transform(y+bh/2,ys,dy))
        if not (x1<=400 or x0>=880):raise ValueError('anchor intersects viewport')
        tag='' if stick or key.startswith('D') else '+' if key=='Plus' else '-' if key=='Minus' else key
        tx=x+(80 if key in ('ZL','L') else -80 if shoulder else 0)
        ty=y if shoulder else y+(52 if tiny else 35)
        anchors.append(dict(input=key,source_center=[x,y],rect=[x0,y0,x1-x0,y1-y0],tag=tag,tag_center=[round(transform(tx,xs,dx)),round(transform(ty,ys,dy))]))
    if preview:
        preview.mkdir(parents=True,exist_ok=True);frame.save(preview/('controller-'+name+'-blank.png'))
    return dict(name=name,id={'ivory':4,'blue':5,'black':6}[name],source_sha256=hashlib.sha256(source_path.read_bytes()).hexdigest(),source_aperture=[l,t,r,b],viewport=[400,40,480,640],packed_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),bytes=output.stat().st_size,jpeg_half_bytes=list(map(len,halves)),anchors=anchors)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--preview',type=Path);a=p.parse_args();results=[pack(n,a.preview) for n in APERTURES]
    metadata=ROOT/'assets/phone-skins/controller-packed-assets.json';metadata.write_text(json.dumps(dict(format='J2SJ split baseline JPEG',results=results),indent=2)+'\n')
    lines=['#pragma once','#include "host_keymap.h"','#include <array>','namespace host_controller {','struct Anchor {int input,x,y,w,h,tag_x,tag_y;const char* tag;};','inline constexpr std::array<std::array<Anchor,16>,3> layouts = {{']
    for result in results:
        lines.append('    {{ // '+result['name'])
        for anchor in result['anchors']:
            key=anchor['input'];input_id='host_keymap::InputCount'+('+1' if key=='Zero' else '') if key in ('Menu','Zero') else 'host_keymap::'+key
            vals=anchor['rect']+anchor['tag_center'];lines.append('        {'+input_id+','+','.join(map(str,vals))+',"'+anchor['tag']+'"},')
        lines.append('    }},')
    lines+=['}};','}']
    (ROOT/'src/controller_skin_layout.h').write_text('\n'.join(lines)+'\n')
    print(json.dumps(results,indent=2))

if __name__=='__main__':main()
