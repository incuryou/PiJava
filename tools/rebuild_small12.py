#!/usr/bin/env python3
"""Pack native 12px bitmaps without resampling or clipping any ink."""
import gzip,hashlib,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCES=[('wenquanyi_9pt.bdf.gz','aa7e18cfc3b351f4391108ee7f5393bcd16ee8e1e41d5db5e0f09e9a5bdaf31b'),('6x12.bdf.gz','2d3bd0bfd58025080e51b0847df4cfa97295e8487ccaeafc7644c1b84f49cabc')]
def read_bdf(path):
    text=gzip.decompress(path.read_bytes()).decode('utf8')
    for block in text.split('STARTCHAR ')[1:]:
        lines=block.splitlines();cp=int(next(l.split()[1] for l in lines if l.startswith('ENCODING ')))
        box=tuple(map(int,next(l for l in lines if l.startswith('BBX ')).split()[1:]));w,h,x,y=box
        at=next(i for i,l in enumerate(lines) if l.strip()=='BITMAP')+1
        rows=[int(l.strip(),16) for l in lines[at:at+h]]
        yield cp,w,h,x,y,rows
def build():
    glyphs={};skipped=[]
    for filename,digest in SOURCES:
        path=ROOT/'third_party/small12'/filename
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
        for cp,w,h,x,y,rows in read_bdf(path):
            if not 32<=cp<=65535 or (filename.startswith('6x12') and cp>=128) or (not filename.startswith('6x12') and cp<128):continue
            cell=6 if cp<128 else 12;top=10-y-h;packed=[0]*12;valid=True
            for sy,row in enumerate(rows):
                for sx in range(w):
                    if not row&(1<<(((w+7)//8)*8-1-sx)):continue
                    xx=x+sx;yy=top+sy
                    if not(0<=xx<cell and 0<=yy<12):valid=False;continue
                    packed[yy]|=0x8000>>xx
            if valid:glyphs[cp]=struct.pack('>12H',*packed)
            else:skipped.append(cp) # Existing 16px coverage fallback keeps rare outliers.
    assert all(cp in glyphs for cp in range(32,127))
    for c in '喝下牛奶可以增加点体力上限是中文':assert ord(c) in glyphs
    data=struct.pack('<I',len(glyphs))+b''.join(struct.pack('<H',cp)+glyphs[cp] for cp in sorted(glyphs))
    out=ROOT/'romfs/font/small12.bin';out.write_bytes(data)
    print(json.dumps({'glyphs':len(glyphs),'ascii':95,'out_of_cell_ink_fallback':len(skipped),'sha256':hashlib.sha256(data).hexdigest()},ensure_ascii=False))
if __name__=='__main__':build()
