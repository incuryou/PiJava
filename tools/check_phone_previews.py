#!/usr/bin/env python3
"""Check native PPM previews against the example and packed asset manifest.

Run preview_phone_skins first, with and without the game-preview.ppm argument.
This verifies real outputs from the C++ decoder, not a Pillow JPEG approximation.
"""
from pathlib import Path
import hashlib
import json
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    manifest = json.loads((ROOT / 'assets/phone-skins/packed-assets.json').read_text())
    example_path = ROOT / 'verification/target-replay/robot-60-music-on.png'
    with Image.open(example_path) as source:
        require(source.size == (240, 320), 'unexpected example size')
        expected = source.convert('RGB').resize((480, 640), Image.Resampling.NEAREST).tobytes()
    results = []
    output = ROOT / 'verification/phone89'
    output.mkdir(exist_ok=True)
    for index, entry in enumerate(manifest['results'], 1):
        for path, expected_hash in [
            (ROOT / 'romfs/skins' / (entry['name'] + '.skin'), entry['sha256']),
            (ROOT / 'assets/phone-skins' / (entry['name'] + '.png'), entry['source_sha256']),
        ]:
            require(hashlib.sha256(path.read_bytes()).hexdigest() == expected_hash,
                    'manifest digest mismatch: ' + str(path))
        blank_path = ROOT / 'build' / f'phone89-native-{index}.ppm'
        game_path = ROOT / 'build' / f'phone89-native-{index}-game.ppm'
        with Image.open(blank_path) as blank_input, Image.open(game_path) as game_input:
            blank, game = blank_input.convert('RGB'), game_input.convert('RGB')
        require(blank.size == game.size == (1280, 720), 'incorrect frame size')
        require(blank.crop((400, 40, 880, 680)).tobytes() == bytes(480 * 640 * 3),
                'skin pixels inside game viewport')
        require(game.crop((400, 40, 880, 680)).tobytes() == expected,
                'game pixels differ from integer 2x example')
        for region in [(0, 0, 1280, 40), (0, 680, 1280, 720),
                       (0, 40, 400, 680), (880, 40, 1280, 680)]:
            require(blank.crop(region).tobytes() == game.crop(region).tobytes(),
                    'game write changed housing pixels')
        corners = [blank.getpixel(p) for p in [(0, 0), (1279, 0), (0, 719), (1279, 719)]]
        edges = [blank.getpixel(p) for p in [(640, 0), (640, 719), (0, 360), (1279, 360)]]
        # Artwork is gray outside the rounded housing. Extrema must have a
        # materially different color from that background, on all four edges.
        background = tuple(sum(c[channel] for c in corners) / 4 for channel in range(3))
        require(all(max(abs(e[c] - background[c]) for c in range(3)) > 8 for e in edges),
                'housing does not reach a frame edge')
        require(all(max(c) - min(c) < 12 for c in corners), 'corner background changed')
        blank.save(output / (blank_path.stem + '.png'))
        game.save(output / (game_path.stem + '.png'))
        results.append(dict(name=entry['name'], viewport=[400, 40, 480, 640],
                            blank_viewport_zero=True, game_matches_integer_2x=True,
                            outside_viewport_unchanged=True, game_pixels_checked=480 * 640,
                            edge_order=['top', 'bottom', 'left', 'right'],
                            edge_rgb=edges, corner_rgb=corners, asset_sha256=entry['sha256']))
    report = dict(version='0.1.89', example=str(example_path.relative_to(ROOT)),
                  example_is_historical=True, example_sha256=hashlib.sha256(example_path.read_bytes()).hexdigest(),
                  native_loader='host_skin::load_file', native_renderer='host_menu::render_bezel',
                  results=results)
    (output / 'alignment-results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS three native assets: exact zero apertures, 307200 game pixels each, unchanged housing, four edge extrema and corner background')


if __name__ == '__main__':
    main()
