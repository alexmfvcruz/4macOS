"""Main-client bundle freshness and icon packaging checks; no game launch."""
import plistlib, subprocess, sys, tempfile, unittest
import os
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import app_bundle as bundle

from build_freshness import record_build

def fresh(out):
    source=out/'test-source.cpp'
    if not source.exists(): source.write_text('source')
    os.utime(out/'4story-client',None)
    record_build(out,[source])

class BundleTest(unittest.TestCase):
    def test_package_and_detect_stale_inputs(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder);binary=out/'4story-client';binary.write_bytes(b'client')
            fresh(out)
            self.assertEqual(bundle.select_binary(binary),binary)
            icon=out/'source.icns';icon.write_bytes(b'icon')
            with patch.object(bundle.subprocess,'run') as run:
                app=bundle.package_client(out,icon)
                self.assertEqual((app/'Contents/Resources/4story.icns').read_bytes(),b'icon')
                info=plistlib.loads((app/'Contents/Info.plist').read_bytes())
                # Accessibility access survives rebuilds only with this requirement.
                self.assertTrue(any(f'-r=designated => identifier "{info["CFBundleIdentifier"]}"' in c.args[0] for c in run.call_args_list))
                self.assertEqual(info['CFBundleIconFile'],'4story.icns')
                self.assertEqual(bundle.select_binary(binary),app/'Contents/MacOS/4story-client')
                for path in bundle.paths(out):
                    saved=path.read_bytes();stat=path.stat();path.write_bytes(b'changed')
                    with self.assertRaises(RuntimeError):bundle.select_binary(binary)
                    path.write_bytes(saved);os.utime(path,ns=(stat.st_atime_ns,stat.st_mtime_ns))
                (app/'Contents/Resources/4story.icns').unlink()
                with self.assertRaises(RuntimeError):bundle.select_binary(binary)
    def test_failed_packaging_invalidates_old_bundle(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder);(out/'4story-client').write_bytes(b'client')
            icon=out/'source.icns';icon.write_bytes(b'icon')
            fresh(out)
            with patch.object(bundle.subprocess,'run'):bundle.package_client(out,icon)
            with patch.object(bundle.subprocess,'run',side_effect=subprocess.CalledProcessError(1,'codesign')):
                with self.assertRaises(subprocess.CalledProcessError):bundle.package_client(out,icon)
            with self.assertRaises(RuntimeError):bundle.select_binary(out/'4story-client')
if __name__=='__main__':unittest.main()
