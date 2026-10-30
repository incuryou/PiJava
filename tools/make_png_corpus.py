#!/usr/bin/env python3
"""Build decoder differential cases from user-provided JARs + deterministic PNG fixtures."""
import argparse,hashlib,json,random,struct,zlib,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('jars',nargs='*',type=Path);a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True);valid=a.out/'valid';fuzz=a.out/'mutated';valid.mkdir(exist_ok=True);fuzz.mkdir(exist_ok=True);manifest=[]
def save(folder,name,data,source):
 (folder/name).write_bytes(data);manifest.append(dict(name=folder.name+'/'+name,sha256=hashlib.sha256(data).hexdigest(),source=source))
for index,jar in enumerate(a.jars):
 with zipfile.ZipFile(jar) as z:
  for n in z.namelist():
   if n.lower().endswith('.png'):
    b=z.read(n);save(valid,f'jar{index}-{len(manifest):04d}.png',b,dict(jar=jar.name,member=n))
rng=random.Random(0x234)
def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
def png(w,h,depth,color,raw,level=6,interlace=0,palette=b'',trans=b''):
 b=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,depth,color,0,0,interlace))
 if palette:b+=chunk(b'PLTE',palette)
 if trans:b+=chunk(b'tRNS',trans)
 return b+chunk(b'IDAT',zlib.compress(raw,level))+chunk(b'IEND',b'')
def paeth(a,b,c):
 p=a+b-c;aa,bb,cc=abs(p-a),abs(p-b),abs(p-c)
 return a if aa<=bb and aa<=cc else b if bb<=cc else c
for color,ch in [(0,1),(2,3),(4,2),(6,4)]:
 for depth in [8,16]:
  for filt in range(5):
   w,h=19,13;bpp=ch*depth//8;prev=bytes(w*bpp);raw=bytearray()
   for y in range(h):
    row=bytes(rng.randrange(256) for _ in prev);raw.append(filt)
    for x,v in enumerate(row):
     left=row[x-bpp] if x>=bpp else 0;above=prev[x];ul=prev[x-bpp] if x>=bpp else 0
     pred=[0,left,above,(left+above)//2,paeth(left,above,ul)][filt];raw.append((v-pred)&255)
    prev=row
   for level in [0,1,6,9]:
    name=f'generated-c{color}-d{depth}-f{filt}-z{level}.png';save(valid,name,png(w,h,depth,color,raw,level),dict(generated=True))
for depth in [1,2,4,8]:
 for color in [0,3]:
  w,h=23,9;raw=b''.join(bytes([0])+bytes(rng.randrange(256) for _ in range((w*depth+7)//8)) for _ in range(h))
  palette=bytes(rng.randrange(256) for _ in range((1<<depth)*3)) if color==3 else b''
  trans=bytes(rng.randrange(256) for _ in range(1<<depth)) if color==3 else b''
  save(valid,f'generated-packed-c{color}-d{depth}.png',png(w,h,depth,color,raw,palette=palette,trans=trans),dict(generated=True))
# All 7 Adam7 passes including small passes with zero rows/columns.
for w,h in [(1,1),(2,3),(23,17),(65,33)]:
 pixels=[[(rng.randrange(256),rng.randrange(256),rng.randrange(256),rng.randrange(256)) for x in range(w)] for y in range(h)]
 raw=bytearray()
 for x0,y0,dx,dy in [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]:
  if x0>=w:continue
  for y in range(y0,h,dy):
   raw.append(0)
   for x in range(x0,w,dx):raw.extend(pixels[y][x])
 save(valid,f'generated-adam7-{w}x{h}.png',png(w,h,8,6,raw,interlace=1),dict(generated=True))
seeds=[f.read_bytes() for f in sorted(valid.glob('generated-*.png'))]
for i in range(800):
 data=bytearray(rng.choice(seeds));kind=i%4
 if kind==0:data=data[:rng.randrange(len(data)+1)]
 elif kind==1:
  for _ in range(1+rng.randrange(4)):
   pos=rng.randrange(len(data));data[pos]^=1<<rng.randrange(8)
 elif kind==2:
  # Corrupt the deflate payload while repairing PNG chunk CRC, so the
  # decompressor itself, not just the chunk validator, is exercised.
  pos=data.find(b'IDAT');n=struct.unpack('>I',data[pos-4:pos])[0];at=pos+4+rng.randrange(n)
  data[at]^=1<<rng.randrange(8);data[pos+4+n:pos+8+n]=struct.pack('>I',zlib.crc32(data[pos:pos+4+n])&0xffffffff)
 else:
  pos=data.find(b'IDAT');n=struct.unpack('>I',data[pos-4:pos])[0];data[pos+n]^=0x55
  data[pos+4+n:pos+8+n]=struct.pack('>I',zlib.crc32(data[pos:pos+4+n])&0xffffffff)
 save(fuzz,f'mutated-{i:04d}.png',bytes(data),dict(generated=True,mutation=kind))
(a.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Corpus:',len(list(valid.iterdir())),'valid/assets;',len(list(fuzz.iterdir())),'mutated cases')
