import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('a',Path(__file__).resolve().parents[1]/'tools/analyze_3d242.py')
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)

class Logs(unittest.TestCase):
    def test_modes(self):
        for mode in (0,1):
            r=a.analyze(f'[1] ==== BOOT test ====\n[2] M3G_STAGE242 geometry_native={mode}\n[3] LAUNCH begin game.jar\n')
            self.assertEqual(r['sessions'][0]['configuration']['M3G_STAGE242']['geometry_native'],str(mode))
    def test_counts_not_constants(self):
        r=a.analyze('[0] ==== BOOT x ====\n[1] LAUNCH begin a\n[2] M3GMESH schema=1 enabled=1 skin_calls=7 max_bones=256\n[3] M3GMESH skin_calls=9 max_bones=256\n')['sessions'][0]
        self.assertEqual(r['counter_increments'],{'M3GMESH.skin_calls':9})
    def test_truncated_baseline(self):
        r=a.analyze('[1] LAUNCH begin a\n[2] M3GLIGHT calls=10\n[3] M3GLIGHT calls=15')['sessions'][0]
        self.assertEqual(r['counter_increments']['M3GLIGHT.calls'],5);self.assertTrue(r['warnings'])
    def test_reset(self):
        r=a.analyze('[0] ==== BOOT x ====\n[1] LAUNCH begin a\n[2] M3GLIGHT calls=10\n[3] M3GLIGHT calls=2')['sessions'][0]
        self.assertEqual(r['counter_increments']['M3GLIGHT.calls'],12);self.assertTrue(r['warnings'])
    def test_window_weighting(self):
        r=a.analyze('[0] ==== BOOT x ====\n[1] LAUNCH begin a\n[2] STAT window_ms=1000 canvas_paints=2 canvas_paint_avg_ms=550 presents=1\n[3] STAT window_ms=1000 canvas_paints=4 canvas_paint_avg_ms=600 presents=2')['sessions'][0]['summary']['heavy_paint_windows']
        self.assertAlmostEqual(r['weighted_paint_mean_ms'],583.333333);self.assertEqual(r['draw_completions_per_s'],3);self.assertEqual(r['presentations_per_s'],1.5)
    def test_boot_and_launch(self):
        r=a.analyze('[0] ==== BOOT x ====\n[1] LAUNCH begin a\n[2] M3GLIGHT calls=7\n[3] LIFECYCLE destroyed\n[4] LAUNCH begin b\n[5] M3GLIGHT calls=10\n[0] ==== BOOT y ====\n[1] LAUNCH begin c\n[2] M3GLIGHT calls=2')
        self.assertEqual([s['counter_increments']['M3GLIGHT.calls'] for s in r['sessions']],[7,3,2])
    def test_malformed(self):
        r=a.analyze('[1] LAUNCH begin a\n[2] STAT window_ms=nan canvas_paints=2\n[3] PAINTHOT method=a samples=bad')['sessions'][0]
        self.assertFalse(r['windows']);self.assertFalse(r['reported_checkpoint_counts']);self.assertTrue(r['warnings'])

if __name__=='__main__':unittest.main()
