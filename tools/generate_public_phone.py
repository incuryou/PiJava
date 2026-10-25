#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rebuild the original, unbranded phone schematic. Requires Pillow.

Run from any directory; --check verifies the committed assets without writing.
The historical raw filename is retained for the existing runtime loader.
All geometry and 3x5 digit glyphs below are project-original; no fonts,
photographs, manufacturer artwork, or external image inputs are used.
"""

import argparse
import hashlib
import io
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT = 227, 518
RAW_SIZE = WIDTH * HEIGHT * 4
NAV = (
    ("SoftL", 47, 266, 28, 16), ("Up", 103, 267, 21, 12),
    ("SoftR", 151, 266, 28, 16), ("Left", 91, 282, 13, 23),
    ("Fire", 104, 282, 20, 21), ("Right", 124, 282, 13, 23),
    ("Down", 103, 304, 21, 12),
)
KEYS = tuple((label, 34 + col * 52, 365 + row * 26, 49, 23)
             for row, labels in enumerate(("123", "456", "789", "*0#"))
             for col, label in enumerate(labels))
GLYPHS = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "010", "010", "010"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
    "*": ("000", "101", "010", "101", "000"),
    "#": ("101", "111", "101", "111", "101"),
}


def render():
    """Use one sequence of native shapes for both raster and vector outputs."""
    image = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" '
           f'height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
           '<!-- SPDX-License-Identifier: GPL-3.0-or-later -->',
           '<title>Original unbranded phone keymap schematic</title>']

    def rect(x, y, w, h, fill, radius=0):
        draw.rounded_rectangle((x, y, x + w - 1, y + h - 1),
                               radius=radius, fill=fill)
        svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" '
                   f'rx="{radius}" fill="{fill}"/>')

    def polygon(points, fill):
        draw.polygon(points, fill=fill)
        coords = " ".join(f"{x},{y}" for x, y in points)
        svg.append(f'<polygon points="{coords}" fill="{fill}"/>')

    def glyph(label, cx, cy):
        for row, bits in enumerate(GLYPHS[label]):
            for col, bit in enumerate(bits):
                if bit == "1":
                    rect(cx - 3 + col * 2, cy - 5 + row * 2,
                         2, 2, "#f5f7f8")

    # Neutral silver body, with no recognizable brand-specific styling.
    rect(10, 17, 207, 497, "#59636b", 31)
    rect(12, 19, 203, 493, "#b6bdc2", 29)
    rect(17, 24, 193, 483, "#dbe0e3", 25)
    rect(22, 29, 183, 472, "#cbd2d7", 21)
    rect(86, 33, 55, 5, "#525e68", 2)
    for x in range(91, 138, 6):
        rect(x, 34, 2, 2, "#263640")
    # An abstract LCD: status marks and four simple app tiles.
    rect(26, 46, 175, 204, "#52606b", 9)
    rect(30, 50, 167, 196, "#182e3d", 5)
    rect(40, 61, 5, 3, "#a4c1cc")
    rect(47, 58, 5, 6, "#a4c1cc")
    rect(54, 55, 5, 9, "#a4c1cc")
    rect(172, 56, 13, 8, "#a4c1cc", 1)
    rect(185, 58, 2, 4, "#a4c1cc")
    rect(40, 74, 147, 1, "#385361")
    for x, y, color in ((70, 111, "#559ba6"), (119, 111, "#7d9eaa"),
                        (70, 155, "#7d9eaa"), (119, 155, "#559ba6")):
        rect(x, y, 37, 32, color, 5)
        rect(x + 10, y + 9, 17, 3, "#d8e7eb", 1)
        rect(x + 10, y + 17, 12, 3, "#d8e7eb", 1)
    rect(75, 219, 77, 3, "#6b8b99", 1)
    # Each visible navigation key occupies its unchanged runtime hotspot.
    for label, x, y, w, h in NAV:
        rect(x, y, w, h, "#263540", 3)
        cx, cy = x + w // 2, y + h // 2
        if label.startswith("Soft"):
            rect(cx - 6, cy - 1, 12, 2, "#f5f7f8")
        elif label == "Fire":
            rect(cx - 4, cy - 4, 8, 8, "#88b6c4", 2)
        elif label == "Up":
            polygon(((cx, cy - 3), (cx - 4, cy + 2), (cx + 4, cy + 2)), "#f5f7f8")
        elif label == "Down":
            polygon(((cx, cy + 3), (cx - 4, cy - 2), (cx + 4, cy - 2)), "#f5f7f8")
        elif label == "Left":
            polygon(((cx - 3, cy), (cx + 2, cy - 4), (cx + 2, cy + 4)), "#f5f7f8")
        elif label == "Right":
            polygon(((cx + 3, cy), (cx - 2, cy - 4), (cx - 2, cy + 4)), "#f5f7f8")
    rect(34, 342, 153, 2, "#b2bdc4", 1)
    for label, x, y, w, h in KEYS:
        rect(x, y, w, h, "#263540", 5)
        glyph(label, x + w // 2, y + h // 2)
    rect(96, 489, 35, 3, "#8c99a2", 1)
    svg.append("</svg>")
    png = io.BytesIO()
    image.save(png, format="PNG", compress_level=9)
    return image, {
        "assets/keymap/public-phone.svg": ("\n".join(svg) + "\n").encode("utf-8"),
        "assets/keymap/public-phone.png": png.getvalue(),
        "romfs/ui/nokia5300.rgba": image.tobytes("raw", "RGBA"),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify without writing")
    args = parser.parse_args()
    image, outputs = render()
    assert RAW_SIZE == 470344
    assert len(outputs["romfs/ui/nokia5300.rgba"]) == RAW_SIZE
    assert image.getpixel((0, 0)) == (0, 0, 0, 0)
    assert image.getpixel((226, 517)) == (0, 0, 0, 0)
    # Verify that all unchanged runtime hot areas contain opaque artwork.
    for label, x, y, w, h in NAV + KEYS:
        assert image.getpixel((x + w // 2, y + h // 2))[3] == 255, label
    for name, data in outputs.items():
        path = ROOT / name
        if args.check:
            if not path.exists() or path.read_bytes() != data:
                raise SystemExit(f"Asset differs from deterministic rebuild: {name}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    with Image.open(ROOT / "assets/keymap/public-phone.png") as saved:
        assert saved.size == (WIDTH, HEIGHT) and saved.mode == "RGBA"
        assert saved.tobytes() == outputs["romfs/ui/nokia5300.rgba"]
    print(json.dumps({
        "status": "verified" if args.check else "generated",
        "license": "GPL-3.0-or-later", "width": WIDTH, "height": HEIGHT,
        "raw_format": "RGBA, 8 bits per channel, row-major, no header",
        "raw_bytes": RAW_SIZE, "hotspots": NAV + KEYS,
        "assets": {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                   for name, data in outputs.items()},
    }, indent=2))


if __name__ == "__main__":
    main()
