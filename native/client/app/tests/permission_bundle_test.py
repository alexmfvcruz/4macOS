"""Regression checks: incomplete/stale bundles must never silently run."""
import json, sys, tempfile, unittest
import os
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import permission_bundle as bundle
from build_freshness import record_build

def fresh(out):
    source=out/'test-source.cpp'
    if not source.exists(): source.write_text('source')
    os.utime(out/'4story-client',None)
    record_build(out,[source])

class BundleTest(unittest.TestCase):
    def test_freshness_and_failed_build(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder); binary=out/'4story-client'
            binary.write_bytes(b'original')
            fresh(out)
            self.assertEqual(bundle.select_binary(binary),binary)
            for path in bundle.paths(out):
                path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'original')
            with patch.object(bundle,'verify_signature'):
                with self.assertRaises(RuntimeError):bundle.select_binary(binary)
                fresh(out);bundle.record_complete(out)
                self.assertEqual(bundle.select_binary(binary),bundle.paths(out)[2])
                binary.write_bytes(b'new build without updated bundle')
                with self.assertRaises(RuntimeError):bundle.select_binary(binary)
                binary.write_bytes(b'original');(out/bundle.STAMP).write_text('{}')
                with self.assertRaises(RuntimeError):bundle.select_binary(binary)
                fresh(out);bundle.record_complete(out);binary.unlink()
                with self.assertRaises(RuntimeError):bundle.select_binary(binary)
    def test_incomplete_bundle_or_signature(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)
            for path in bundle.paths(out):
                path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'original')
            with patch.object(bundle,'verify_signature'):fresh(out);bundle.record_complete(out)
            with patch.object(bundle,'verify_signature',side_effect=ValueError('invalid signature')):
                with self.assertRaises(RuntimeError):bundle.select_binary(out/'4story-client')
            bundle.paths(out)[4].write_bytes(b'changed plist')
            with self.assertRaises(RuntimeError):bundle.select_binary(out/'4story-client')
    def test_icon_missing_or_changed_rejects_bundle(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)
            for path in bundle.paths(out):
                path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'original')
            icon=out/'4Story.app/Contents/Resources/4story.icns'
            with patch.object(bundle,'verify_signature'):
                fresh(out);bundle.record_complete(out)
                icon.write_bytes(b'changed icon without a complete build')
                with self.assertRaises(RuntimeError):bundle.select_binary(out/'4story-client')
                icon.write_bytes(b'original');fresh(out);bundle.record_complete(out);icon.unlink()
                with self.assertRaises(RuntimeError):bundle.select_binary(out/'4story-client')
if __name__=='__main__':unittest.main()
