"""Standard-library unit tests for cumulative log interpretation."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from analyze_m3g240 import COUNTERS, analyze_text


def snapshot(n):
    fields = dict.fromkeys(COUNTERS, n)
    fields.update(attempts=n*2, calls=n, pixels=n*8, schema=1, max_sample_us=n*3, max_pixels=8)
    return '[123] M3GROW ' + ' '.join(str(k)+'='+str(v) for k,v in fields.items())


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
        r = analyze_text(snapshot(10)+'\nM3G_STAGE version=0.2.40-s1\n'+snapshot(50))
        self.assertEqual(r['usable_intervals'], 0)
        self.assertEqual(len(r['boots']), 1)

    def test_empty_or_malformed(self):
        self.assertTrue(analyze_text('M3GSTAGE calls=100')['warnings'])
        r = analyze_text(snapshot(10)+'\nM3GROW schema=1 calls=x\n'+snapshot(50))
        self.assertEqual(r['usable_intervals'], 0)
        self.assertTrue(r['warnings'])

    def test_row_max_is_not_sum(self):
        r = analyze_text(snapshot(10)+'\n'+snapshot(30).replace('max_pixels=8', 'max_pixels=1024'))
        self.assertEqual(r['max_accepted_row_pixels_process_cumulative'], 1024)

    def test_old_span_not_counted_as_row(self):
        r = analyze_text(snapshot(10).replace('M3GROW', 'M3GSPAN'))
        self.assertEqual(r['snapshots'], 0)

    def test_bad_row_bound(self):
        r = analyze_text(snapshot(10).replace('max_pixels=8', 'max_pixels=1025'))
        self.assertEqual(r['snapshots'], 0)
        self.assertTrue(r['warnings'])

    def test_negative_time(self):
        r = analyze_text(snapshot(10).replace('max_sample_us=30', 'max_sample_us=-1'))
        self.assertEqual(r['snapshots'], 0)


if __name__ == '__main__':
    unittest.main()
