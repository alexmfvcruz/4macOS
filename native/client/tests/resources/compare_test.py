#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile
import unittest
spec = importlib.util.spec_from_file_location('compare', Path(__file__).with_name('compare.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ComparisonTest(unittest.TestCase):
    def compare(self, native, windows):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory)/'native.tsv', Path(directory)/'windows.tsv'
            a.write_text(native)
            b.write_text(windows)
            return module.compare(a, b)

    @staticmethod
    def end():
        return 'result\t0\ttexture_failures\t0\nresult\t0\tstatus\tcomplete\n'

    @staticmethod
    def device(live='0'):
        return f'device\t0\tbackend\tcpu_empty_device\ndevice\t0\tlive_objects\t{live}\ndevice\t0\tlive_bytes\t0\ndevice\t0\tfailed_creations\t0\n'

    def test_completion_is_required(self):
        self.assertFalse(self.compare('', '')['pass'])
        prefix = 'tex\t1\twidth\t64\n'
        self.assertFalse(self.compare(prefix, prefix)['pass'])

    def test_exact_parity_and_device_validation(self):
        data = 'tex\t1\twidth\t64\n'
        self.assertTrue(self.compare(data+self.device()+self.end(), data+self.end())['pass'])
        self.assertFalse(self.compare(data+self.device('1')+self.end(), data+self.end())['pass'])
        self.assertFalse(self.compare(data+self.device().replace('cpu_empty_device','unknown')+self.end(), data+self.end())['pass'])
        self.assertFalse(self.compare(data+self.device()+'device\t0\tnot_executed.Present\t1\n'+self.end(), data+self.end())['pass'])

    def test_missing_and_different_fields(self):
        native = 'tex\t1\twidth\t64\ntex\t1\textra\t0\n'
        windows = 'tex\t1\twidth\t32\n'
        result = self.compare(native+self.device()+self.end(), windows+self.end())
        self.assertFalse(result['pass'])
        self.assertEqual(result['counts']['different'], 1)
        self.assertEqual(result['counts']['missing_field'], 1)

    def test_duplicate_field_is_rejected(self):
        data = 'tex\t1\twidth\t64\ntex\t1\twidth\t64\n'
        with self.assertRaises(ValueError):
            self.compare(data+self.device()+self.end(), data+self.end())


if __name__ == '__main__':
    unittest.main()
