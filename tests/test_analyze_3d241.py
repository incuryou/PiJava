#!/usr/bin/env python3
"""Regression checks for per-launch, process-cumulative 3D log accounting."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('analyze_3d241', Path(__file__).resolve().parents[1] / 'tools/analyze_3d241.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class LogAccountingTests(unittest.TestCase):
    def test_process_counters_do_not_leak_into_next_game(self):
        r = module.analyze('[0] ==== BOOT x ====\n[1] LAUNCH begin A\n[2] M3GROW calls=10\n[3] M3GROW calls=20\n[4] LAUNCH begin B\n[5] M3GROW calls=20')['sessions']
        self.assertEqual(r[0]['counters']['M3GROW.calls'], 20)
        self.assertEqual(r[1]['counters']['M3GROW.calls'], 0)
    def test_reset_is_explicit(self):
        r = module.analyze('[1] LAUNCH begin A\n[2] M3GROW calls=20\n[3] M3GROW calls=3')['sessions'][0]
        self.assertEqual(r['counters']['M3GROW.calls'], 23)
        self.assertEqual(r['warnings'][0]['counter_reset'], 'M3GROW.calls')
    def test_failed_snapshot_is_separate_from_fresh_launch(self):
        r = module.analyze('[1] CONTINUE begin A\n[2] SAVE invalid state: layout\n[3] SAVE apply failed\n[4] LAUNCH begin A')['sessions']
        self.assertEqual(len(r), 2)
        self.assertEqual(len(r[0]['warnings']), 2)
        self.assertEqual(r[1]['warnings'], [])
    def test_missing_or_malformed_data_is_not_zero_performance(self):
        r = module.analyze('garbled\n[1] LAUNCH begin A\n[2] STAT window_ms=bad canvas_paints=bad\n[3] M3GROW calls=oops')['sessions'][0]
        self.assertEqual(r['counters'], {})
        self.assertIsNone(r['summary']['all']['paint_mean_ms'])
    def test_draw_and_present_are_distinct_not_logic_fps(self):
        r = module.analyze('[1] LAUNCH begin A\n[3] STAT window_ms=2000 canvas_paints=4 presents=1 canvas_paint_avg_ms=400 canvas_paint_max_ms=500')['sessions'][0]
        s = r['summary']['all']
        self.assertEqual(s['draw_completions_per_s'], 2)
        self.assertEqual(s['presentations_per_s'], 0.5)
        self.assertNotIn('logic_fps', s)
    def test_configuration_and_boot_reset(self):
        r = module.analyze('[0] ==== BOOT x ====\n[1] M3G_STAGE241 float_batch=1\n[2] LAUNCH begin A\n[3] M3GROW calls=50\n[0] ==== BOOT y ====\n[1] M3G_STAGE241 float_batch=0\n[2] LAUNCH begin B\n[3] M3GROW calls=7')['sessions']
        self.assertEqual(r[1]['configuration']['M3G_STAGE241']['float_batch'], '0')
        self.assertEqual(r[1]['counters']['M3GROW.calls'], 7)

if __name__ == '__main__':
    unittest.main(verbosity=2)
