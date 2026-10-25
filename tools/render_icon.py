#!/usr/bin/env python3
"""Derive NRO and NSP application icons from the supplied PNG; no package build."""
from pathlib import Path
from shutil import copyfile
from PIL import Image

assets=Path(__file__).resolve().parents[1]/'assets'
with Image.open(assets/'app-logo.png') as image:
    image=image.convert('RGBA')
    # JPEG has no alpha; flatten transparency on white without cropping art.
    background=Image.new('RGBA',image.size,(255,255,255,255))
    background.alpha_composite(image)
    icon=background.convert('RGB').resize((256,256),Image.Resampling.LANCZOS)
icon.save(assets/'icon.jpg','JPEG',quality=95,subsampling=0,progressive=False)
icon.save(assets/'icon.png','PNG')
control=assets/'nsp/control';control.mkdir(parents=True,exist_ok=True)
for language in ('AmericanEnglish','SimplifiedChinese'):
    copyfile(assets/'icon.jpg',control/('icon_'+language+'.dat'))
print('Updated shared NRO/NSP application icons only; no NRO/NSP built')
