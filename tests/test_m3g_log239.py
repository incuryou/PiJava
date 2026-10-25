"""Standard-library unit tests for cumulative log interpretation."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from analyze_m3g239 import COUNTERS, analyze_text


def snapshot(n):
    fields = dict.fromkeys(COUNTERS, n)
    fields.update(attempts=n*2, calls=n, pixels=n*8, schema=1, max_sample_us=n*3)
    return '[123] M3GSPAN ' + ' '.join(str(k)+'='+str(v) for k,v in fields.items())


class CounterTests(unittest.TestCase):
    def test_only_delta(self):
        r = analyze_text(snapshot(10)+'\n'+snapshot(30))
        self.assertEqual(r['delta']['calls'], 20)
        self.assertEqual(r['delta']['pixels'], 160)
        self.assertEqual(r['accepted_percent'], 50)
        self.assertEqual(r['mean_pixels_per_accepted_call'], 8)

    def test_counter_reset(self):
        r = analyze_text('\n'.join(map(snapshot, [10, 30, 5, 15])))
        self.assertEqual(r['delta']['calls'], 30)
        self.assertEqual(r['counter_resets'], 1)

    def test_new_boot_not_subtracted(self):
        r = analyze_text(snapshot(10)+'\nM3G_STAGE version=0.2.39-s1\n'+snapshot(50))
        self.assertEqual(r['usable_intervals'], 0)
        self.assertEqual(len(r['boots']), 1)

    def test_empty_or_malformed(self):
        self.assertTrue(analyze_text('M3GSTAGE calls=100')['warnings'])
        r = analyze_text(snapshot(10)+'\nM3GSPAN schema=1 calls=x\n'+snapshot(50))
        self.assertEqual(r['usable_intervals'], 0)
        self.assertTrue(r['warnings'])


if __name__ == '__main__':
    unittest.main()
