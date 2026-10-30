#!/usr/bin/env python3
"""Check shared NRO/NSP icon assets without building an application package."""
from pathlib import Path
import hashlib
from PIL import Image

root=Path(__file__).resolve().parents[1]
master=root/'assets/app-logo.png'
assert master.is_file(),'uploaded logo must be the canonical icon source'
assert hashlib.sha256(master.read_bytes()).hexdigest()=='6eefbf56a26066ca13805007ca2df516780a96528663f3702296adeaa070f5ee'
with Image.open(master) as src:
    expected=src.convert('RGB').resize((256,256),Image.Resampling.LANCZOS)
with Image.open(root/'assets/icon.png') as image:
    assert image.mode=='RGB' and image.size==(256,256)
    assert image.tobytes()==expected.tobytes(),'preview icon must derive from uploaded logo'
with Image.open(root/'assets/icon.jpg') as image:
    assert image.format=='JPEG' and image.mode=='RGB' and image.size==(256,256)
    assert not image.info.get('progressive') and not image.getexif()
icon=(root/'assets/icon.jpg').read_bytes()
for language in ['AmericanEnglish','SimplifiedChinese']:
    assert (root/('assets/nsp/control/icon_'+language+'.dat')).read_bytes()==icon,'NRO and NSP icon bytes must agree'
assert "ROOT / \"assets\" / \"icon.jpg\"" in (root/'tools/build.py').read_text()
print('PASS supplied logo source identity, 256x256 RGB PNG/baseline JPEG, no EXIF, shared NRO/NSP icon bytes; no NRO/NSP built')
