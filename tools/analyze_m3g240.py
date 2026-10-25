#!/usr/bin/env python3
"""Summarize M3GROW cumulative counters without confusing them with FPS.
Only differences between consecutive, monotonic snapshots are attributed.
Leading snapshots and reset boundaries are deliberately not extrapolated.
"""
import argparse
import json
from pathlib import Path
import re

COUNTERS = ('attempts', 'calls', 'pixels', 'subspans', 'texture_samples', 'mip_samples',
            'depth_rejects', 'alpha_rejects', 'color_writes', 'depth_writes',
            'decline_shape', 'decline_callback', 'decline_alias',
            'decline_coordinate', 'timed_calls', 'sample_prepare_us',
            'sample_raster_us', 'sample_commit_us')
PAIR = re.compile(r'([A-Za-z_][A-Za-z_0-9]*)=([^\s]+)')


def analyze_text(text: str, name: str = '<memory>') -> dict:
    total = dict.fromkeys(COUNTERS, 0)
    previous = None
    snapshots = windows = resets = 0
    boots, warnings = [], []
    max_sample = max_pixels = 0
    for line in text.splitlines():
        if 'M3G_STAGE ' in line:
            boots.append(dict(PAIR.findall(line)))
            previous = None
        if 'M3GROW ' not in line:
            continue
        fields = dict(PAIR.findall(line))
        try:
            if fields.get('schema') != '1':
                raise ValueError('unsupported M3GROW schema')
            current = {k: int(fields[k]) for k in COUNTERS}
            if any(v < 0 for v in current.values()):
                raise ValueError('negative cumulative counter')
            peak = int(fields['max_sample_us'])
            width = int(fields['max_pixels'])
            if peak < 0 or not 0 <= width <= 1024:
                raise ValueError('invalid maximum sample or row width')
        except (KeyError, ValueError) as error:
            warnings.append('Skipped malformed snapshot: ' + str(error))
            previous = None
            continue
        snapshots += 1
        max_sample = max(max_sample, peak)
        max_pixels = max(max_pixels, width)
        if previous is not None:
            if all(current[k] >= previous[k] for k in COUNTERS):
                for k in COUNTERS:
                    total[k] += current[k] - previous[k]
                windows += 1
            else:
                resets += 1
        previous = current
    if not snapshots:
        warnings.append('No stage240 M3GROW records; no native-row conclusion is possible.')
    elif not windows:
        warnings.append('Need two consecutive snapshots for an interval delta.')
    calls, attempts = total['calls'], total['attempts']
    sample_time = sum(total[k] for k in ('sample_prepare_us', 'sample_raster_us', 'sample_commit_us'))
    return {
        'file': name, 'boots': boots, 'snapshots': snapshots,
        'usable_intervals': windows, 'counter_resets': resets, 'delta': total,
        'accepted_percent': round(100 * calls / attempts, 3) if attempts else None,
        'mean_pixels_per_accepted_call': round(total['pixels'] / calls, 3) if calls else None,
        'sampled_prepare_share_percent': round(100 * total['sample_prepare_us'] / sample_time, 3) if sample_time else None,
        'max_logged_sample_us_process_cumulative': max_sample if snapshots else None,
        'max_accepted_row_pixels_process_cumulative': max_pixels if snapshots else None,
        'warnings': warnings,
        'scope': 'counter deltas only; excludes leading snapshots/reset gaps; sampled successful-call timing is not total render time; no logic FPS or physical-device speedup inferred',
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', type=Path, nargs='+')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    from analyze_m3g239 import analyze_text as analyze_spans
    results = []
    for path in args.logs:
        try:
            text = path.read_text(encoding='utf-8-sig', errors='replace')
            results.append({'file': str(path), 'row240': analyze_text(text, str(path)),
                            'span239': analyze_spans(text, str(path))})
        except OSError as error:
            parser.error(str(error))
    output = json.dumps({'schema': 1, 'results': results}, indent=2, ensure_ascii=False) + '\n'
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(output, encoding='utf-8')
    else:
        print(output, end='')


if __name__ == '__main__':
    main()
