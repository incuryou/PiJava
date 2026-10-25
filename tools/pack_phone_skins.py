#!/usr/bin/env python3
"""Pack approved complete-phone artwork into bounded split-JPEG textures.

Pillow is needed only to regenerate assets; ordinary NRO builds use the
committed compressed textures. Game artwork is never part of these textures.
"""
from pathlib import Path
import hashlib
import json
import struct
from io import BytesIO
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ROOT / 'assets/phone-skins'
OUTPUT = ROOT / 'romfs/skins'
APERTURES = {
    'phone-blue': (530, 37, 1137, 900),
    'phone-black': (548, 114, 1119, 811),
    'phone-ivory': (549, 121, 1110, 817),
}
# Crop only the outside bands. Rounded corners keep their original background;
# the housing's four extrema reach the output edges without filling corners.
OUTER_BOUNDS = {
    'phone-blue': (0, 0, 1672, 941),
    'phone-black': (25, 50, 1648, 892),
    'phone-ivory': (30, 45, 1645, 898),
}


def pack(name, aperture):
    source_path = SOURCES / (name + '.png')
    with Image.open(source_path) as image:
        source = image.convert('RGB')
    if source.size != (1672, 941):
        raise ValueError('Unexpected approved source dimensions: ' + name)
    # Nine independent regions align the blank aperture with the immutable
    # game viewport. This resizes only housing artwork, never a game image.
    left, top, right, bottom = aperture
    outer_left, outer_top, outer_right, outer_bottom = OUTER_BOUNDS[name]
    if not (0 <= outer_left < left < right < outer_right <= source.width and
            0 <= outer_top < top < bottom < outer_bottom <= source.height):
        raise ValueError('Invalid housing/aperture coordinates: ' + name)
    xs = (outer_left, left, right, outer_right)
    ys = (outer_top, top, bottom, outer_bottom)
    dx, dy = (0, 400, 880, 1280), (0, 40, 680, 720)
    frame = Image.new('RGB', (1280, 720))
    for row in range(3):
        for col in range(3):
            if row == col == 1:
                continue
            crop = source.crop((xs[col], ys[row], xs[col+1], ys[row+1]))
            crop = crop.resize((dx[col+1]-dx[col], dy[row+1]-dy[row]), Image.Resampling.LANCZOS)
            frame.paste(crop, (dx[col], dy[row]))
    OUTPUT.mkdir(exist_ok=True)
    assert frame.crop((400,40,880,680)).tobytes() == b'\0' * (480*640*3)
    halves = []
    for left in (0, 640):
        stream = BytesIO()
        frame.crop((left,0,left+640,720)).save(stream, format='JPEG', quality=80,
                                             subsampling=0, optimize=True, progressive=False)
        data = stream.getvalue()
        assert 0 < len(data) <= 512*1024 and data[-2:] == b'\xff\xd9'
        halves.append(data)
    # Two 640x720 halves reuse the Java decoder's existing <=1024 dimension
    # ceiling; it is not raised for a host decoration resource.
    header = struct.pack('<4sHHIII', b'J2SJ', 1280, 720, len(halves[0]), len(halves[1]), 0)
    path = OUTPUT / (name + '.skin')
    path.write_bytes(header + b''.join(halves))
    return dict(name=name, source_sha256=hashlib.sha256(source_path.read_bytes()).hexdigest(),
                source_aperture=aperture, source_outer_bounds=OUTER_BOUNDS[name],
                viewport=[400,40,480,640],
                bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                jpeg_half_bytes=[len(x) for x in halves], quality=80, subsampling='4:4:4',
                packed_game_aperture_all_zero=True)


if __name__ == '__main__':
    results = [pack(name, aperture) for name, aperture in APERTURES.items()]
    # These three retired resources are explicitly replaced by the new format.
    for name in APERTURES:
        (OUTPUT/(name+'.bz16')).unlink(missing_ok=True)
    (SOURCES/'packed-assets.json').write_text(json.dumps(dict(format='J2SJ split baseline JPEG', results=results), indent=2)+'\n')
    print(json.dumps(results, indent=2))
