#!/usr/bin/env python3
"""Optional asset-authoring tool (Pillow + CairoSVG), not needed to build NRO.

The canonical SVG uses paths only, so export does not depend on system fonts.
The committed baseline 256x256 RGB JPEG is embedded directly by build.py.
"""
from pathlib import Path
from io import BytesIO
import cairosvg
from PIL import Image

assets = Path(__file__).resolve().parents[1] / 'assets'
png = cairosvg.svg2png(url=str(assets / 'icon.svg'), output_width=1024, output_height=1024)
icon = Image.open(BytesIO(png)).convert('RGB').resize((256, 256), Image.Resampling.LANCZOS)
icon.save(assets / 'icon.jpg', 'JPEG', quality=95, subsampling=0, progressive=False)
icon.save(assets / 'icon.png', 'PNG')
print('Wrote 256x256 RGB baseline icon.jpg and lossless preview icon.png')
