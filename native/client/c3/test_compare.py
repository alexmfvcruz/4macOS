#!/usr/bin/env python3
"""Comparison acceptance cases: exact match, one-LSB difference, dimension mismatch."""
import csv
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PATH = Path(__file__).with_name('compare.py')
spec = importlib.util.spec_from_file_location('c3_compare',PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ComparisonTests(unittest.TestCase):
    def test_reports_every_difference_and_cli_status(self):
        local = PATH.parents[3]/'runtime/legacy'
        local.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='c3-comparison-',dir=local) as temp:
            root = Path(temp)
            native, reference = root/'native.ppm',root/'reference.ppm'
            # The first raster byte is LF: parser must preserve binary whitespace.
            rgb = bytes([10,32,255,0,1,2,128,64,32,255,255,255])
            native.write_bytes(b'P6\n2 2\n255\n'+rgb)
            reference.write_bytes(native.read_bytes())
            result = module.compare(native,reference,root/'match')
            self.assertTrue(result['match'])
            self.assertEqual(result['different_pixels'],0)
            changed = bytearray(rgb);changed[3]=1;changed[11]=0
            reference.write_bytes(b'P6\n2 2\n255\n'+changed)
            result = module.compare(native,reference,root/'changed')
            self.assertEqual((result['different_pixels'],result['maximum_channel_delta']),(2,255))
            with (root/'changed-pixels.tsv').open() as source:
                rows = list(csv.DictReader(source,delimiter='\t'))
            self.assertEqual([(r['x'],r['y']) for r in rows],[('1','0'),('1','1')])
            self.assertEqual(rows[0]['delta_r'],'1')
            self.assertEqual(rows[1]['delta_b'],'255')
            command = [sys.executable,str(PATH),str(native),str(reference),str(root/'cli')]
            self.assertEqual(subprocess.run(command,capture_output=True).returncode,1)
            reference.write_bytes(native.read_bytes())
            self.assertEqual(subprocess.run(command,capture_output=True).returncode,0)
            reference.write_bytes(b'P6\n1 4\n255\n'+rgb)
            self.assertEqual(subprocess.run(command,capture_output=True).returncode,2)
            reference.write_bytes(b'P6\n2 2\n255\n'+rgb[:-1])
            with self.assertRaises(ValueError): module.read_ppm(reference)


if __name__ == '__main__': unittest.main()
