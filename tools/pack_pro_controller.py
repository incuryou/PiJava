#!/usr/bin/env python3
"""Regenerate the bounded editor RGBA and anchor header from approved inputs."""
from pathlib import Path
import hashlib
import json
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def main():
    source=ROOT/'assets/keymap/pro-controller.png';metadata=ROOT/'assets/keymap/pro-controller-layout.json'
    data=json.loads(metadata.read_text());digest=hashlib.sha256(source.read_bytes()).hexdigest()
    if digest!=data['source_sha256']:raise ValueError('source image changed; calibrate anchors before repacking')
    x,y,w,h=data['host_image']
    with Image.open(source) as opened:
        if list(opened.size)!=data['source_size']:raise ValueError('approved source dimensions changed')
        rgba=opened.convert('RGBA').resize((w,h),Image.Resampling.LANCZOS).tobytes()
    if (w,h)!=(460,318) or len(rgba)!=460*318*4:raise ValueError('bounded preview dimensions required')
    output=ROOT/'romfs/ui/pro-controller.rgba';output.write_bytes(rgba);data['rgba_sha256']=hashlib.sha256(rgba).hexdigest()
    lines=['#pragma once','#include "controller_text_fit.h"','namespace host_pro {',f'constexpr int image_x={x},image_y={y},image_w={w},image_h={h};','inline constexpr std::array<host_controller::Anchor,16> anchors={{']
    for a in data['anchors']:
        key='host_controller::Menu' if a['input']=='Menu' else 'host_keymap::'+a['input'];values=a['host_rect']+a['tag_center']
        lines.append('    {'+key+','+','.join(map(str,values))+',"'+a['tag']+'"},')
    lines+=['}};','inline constexpr std::array<host_controller::ButtonFace,16> button_faces={{']
    for face in data.get('image_faces',[]):
        key='host_controller::Menu' if face['input']=='Menu' else 'host_keymap::'+face['input']
        lines.append('    {'+key+','+','.join(map(str,face['center']+face['radii']))+',{'+','.join(map(str,face['phone_inset']))+'},{'+','.join(map(str,face['tag_inset']))+'},"'+face['physical']+'",'+str(face['horizontal']).lower()+'},')
    lines+=['}};','}'];(ROOT/'src/pro_controller_layout.h').write_text('\n'.join(lines)+'\n');metadata.write_text(json.dumps(data,indent=2)+'\n')
    print('Packed only pro-controller.rgba and its editor anchor header:',data['rgba_sha256'])
if __name__=='__main__':main()
