#!/usr/bin/env python3
"""Summarize per-launch 3D evidence without equating presents with logic FPS.

Hotspot counts cover reported longest paints and are checkpoint samples, not CPU
percentages. Heavy windows are a descriptive selection, not matched A/B trials.
"""
import argparse
from collections import Counter
import json
import re
from pathlib import Path

LINE = re.compile(r'^\[(\d+)\]\s+(.*)$')
PAIRS = re.compile(r'(\w+)=([^\s]+)')
CUMULATIVE = {'M3GROW', 'M3GSPAN', 'M3GSPRITE', 'M3GPREP', 'M3GCOVERAGE',
              'M3GSTAGE', 'M3GMESH', 'M3GLIGHT', 'M3GASSEMBLY', 'PAINTHANDOFF'}
IGNORE = {'schema', 'enabled', 'texture_native', 'sample_period', 'bounded_pixels',
          'texture_only', 'budget_us', 'not_paint_preemption'}


def numeric(value):
    try:
        result = float(value)
        return result if result == result and abs(result) != float('inf') else None
    except (ValueError, TypeError):
        return None


def window_summary(rows):
    duration = sum(r['window_ms'] for r in rows)
    draws = sum(r['canvas_paints'] for r in rows)
    presents = sum(r.get('presents', 0) for r in rows)
    return {'windows': len(rows), 'source_lines': [r['line'] for r in rows],
            'window_ms': round(duration, 3), 'completed_draws': draws,
            'presentations': presents,
            'weighted_paint_mean_ms': round(sum(r['canvas_paints'] *
                r.get('canvas_paint_avg_ms', 0) for r in rows) / draws, 6) if draws else None,
            'draw_completions_per_s': round(draws * 1000 / duration, 6) if duration else None,
            'presentations_per_s': round(presents * 1000 / duration, 6) if duration else None}


def analyze(text, heavy_ms=500):
    sessions, prior = [], {}
    current, boot = None, None
    config = {}
    for number, line in enumerate(text.splitlines(), 1):
        match = LINE.match(line)
        if not match:
            continue
        timestamp, body = int(match[1]), match[2]
        kind = body.split(' ', 1)[0]
        pairs = dict(PAIRS.findall(body))
        if body.startswith('==== BOOT'):
            boot, prior, current, config = body, {}, None, {}
        if kind in {'M3G_STAGE', 'M3G_STAGE241', 'M3G_STAGE242'}:
            config[kind] = pairs
        if body.startswith(('LAUNCH begin ', 'CONTINUE begin ')):
            current = {'launch': body, 'line': number, 'start_ms': timestamp,
                       'boot': boot, 'configuration': dict(config), 'windows': [],
                       'counter_increments': {}, 'warnings': [], 'reported_hotspots': [],
                       'audio_pump_gap_max_ms': 0}
            sessions.append(current)
            if boot is None:
                current['warnings'].append('Missing BOOT: first cumulative readings are baselines, not counts.')
        if kind in CUMULATIVE:
            for key, value_text in pairs.items():
                if (key in IGNORE or key.startswith(('max_', 'bounded_')) or
                        not value_text.isdigit()):
                    continue
                name, value = kind + '.' + key, int(value_text)
                old = prior.get(name)
                delta = value if old is None and boot is not None else (0 if old is None else value - old)
                if delta < 0:
                    delta = value
                    if current:
                        current['warnings'].append({'line': number, 'counter_reset': name})
                if current:
                    c = current['counter_increments']
                    c[name] = c.get(name, 0) + delta
                prior[name] = value
        if current is None:
            continue
        current['last_ms'] = timestamp
        if kind == 'STAT':
            row = {'line': number, 'timestamp_ms': timestamp}
            for key in ('window_ms', 'canvas_paints', 'canvas_paint_avg_ms',
                        'canvas_paint_max_ms', 'presents', 'present_avg_ms'):
                value = numeric(pairs.get(key))
                if value is not None:
                    row[key] = value
            if (row.get('window_ms', 0) > 0 and 'canvas_paints' in row and
                    'canvas_paint_avg_ms' in row):
                current['windows'].append(row)
            else:
                current['warnings'].append({'line': number, 'malformed_STAT': body})
        elif kind == 'PAINTHOT':
            current['reported_hotspots'].append({'line': number, **pairs})
        elif kind == 'AUDIOSTAT':
            gap = numeric(pairs.get('pump_gap_max_ms'))
            if gap is not None:
                current['audio_pump_gap_max_ms'] = max(current['audio_pump_gap_max_ms'], gap)
        elif body.startswith(('EX ', 'SAVE invalid', 'SAVE apply failed')):
            current['warnings'].append({'line': number, 'record': body})
        if body.startswith('LIFECYCLE destroyed') or 'back-to-menu' in body:
            current = None
    for session in sessions:
        windows = session['windows']
        session['summary'] = {'all': window_summary(windows),
            'heavy_paint_windows': window_summary([r for r in windows if r['canvas_paint_avg_ms'] >= heavy_ms])}
        counts = Counter()
        for row in session['reported_hotspots']:
            n = numeric(row.get('samples'))
            if n is not None:
                counts[row.get('method', 'unknown')] += int(n)
        session['reported_checkpoint_counts'] = dict(counts.most_common())
    return {'schema': 1, 'heavy_paint_threshold_ms': heavy_ms,
            'scope': 'Log observations, not matched gameplay trials. Draw completions and presentations are not logic FPS. Checkpoint samples are not method CPU time. First deltas across launches can include unreported work from the previous launch.',
            'sessions': sessions}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('logs', nargs='+', type=Path)
    ap.add_argument('--heavy-ms', type=float, default=500)
    ap.add_argument('--out', required=True, type=Path)
    args = ap.parse_args()
    if args.heavy_ms < 0:
        ap.error('--heavy-ms must not be negative')
    result = {str(p): analyze(p.read_text(errors='replace'), args.heavy_ms) for p in args.logs}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    for path, value in result.items():
        print(path)
        for session in value['sessions']:
            print(json.dumps(session['summary'], ensure_ascii=False))


if __name__ == '__main__':
    main()
