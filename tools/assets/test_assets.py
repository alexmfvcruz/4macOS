"""Focused real-asset and output regression checks; no live game/database writes."""
import io
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import numpy as np
from PIL import Image

from asset_data import AssetError, Reader, Store, inflate
from preview import geometry, terminal, ui_image


class AssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.store = Store()

    def test_original_mount_appearance_chain(self):
        expected = {31036: (0x110b7c1d, 21136, 0x110b7e9e),
                    31190: (0x110b7e09, 21138, 0x110b84e6)}
        for id, (object, selection, mesh) in expected.items():
            row = self.store.monsters[id]
            self.assertEqual(row['object'], object)
            parts = self.store.choose_parts(object, row['equipment'])
            self.assertEqual([(p['selection'], p['mesh']) for p in parts], [(selection, mesh)])
            self.assertGreater(np.std(np.asarray(geometry(self.store, parts, 240, 190))), 5)

    def test_other_behemoth_has_its_own_geometry(self):
        row = self.store.monsters[31038]
        parts = self.store.choose_parts(row['object'], row['equipment'])
        self.assertEqual(parts[0]['selection'], 21138)
        self.assertEqual(parts[0]['mesh'], 0x110b80b8)
        self.assertNotEqual(parts[0]['mesh'], 0x110b7e9e)

    def test_ui_tiles_and_texture_decoding(self):
        image = ui_image(self.store, 7)
        self.assertEqual(image.size, (324, 144))
        self.assertTrue(np.asarray(image)[:, :, 3].any())
        self.assertEqual(self.store.texture(0x110b821c).size, (512, 512))

    def test_unweighted_mesh(self):
        for id, (file, _, _) in self.store.indexes['mesh'].items():
            if 'Map' in file:
                try:
                    self.store.mesh(id)
                except AssetError:
                    continue
                # Plain geometry is a supported path even without object materials.
                image = geometry(self.store, [{'mesh': id, 'materials': []}], 160, 128, plain=True)
                self.assertGreater(np.std(np.asarray(image)), 1)
                return
        self.fail('No representative scenery mesh could be rendered')

    def test_submesh_texture_uses_list_index_not_draw_order(self):
        # In the engine, m_dwOrder sorts draws; texture index selects the submesh.
        class TinyStore:
            def mesh(self, id):
                return (np.array([[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]]),
                        np.tile([0., 0., 1.], (3, 1)), np.zeros((3, 2)),
                        [np.array([[0, 1, 2]])])
            def texture(self, id):
                return Image.new('RGBA', (1, 1), (255, 0, 0, 255))
        part = {'mesh': 1, 'materials': [{'texture': 1, 'order': 99, 'alpha': 0}]}
        data = np.asarray(geometry(TinyStore(), [part], 100, 100, yaw=0, elevation=0))
        self.assertTrue(((data[:, :, 0] > 0) & (data[:, :, 1] == 0)).any())

    def test_terminal_protocols_never_save_an_image_file(self):
        image = Image.new('RGBA', (40, 20), (100, 20, 200, 128))
        original_save = Image.Image.save
        def guarded_save(img, fp, *args, **kwargs):
            self.assertIsInstance(fp, io.BytesIO)
            return original_save(img, fp, *args, **kwargs)
        with patch.object(Image.Image, 'save', guarded_save):
            for format in ('ansi', 'iterm', 'kitty'):
                stream = io.StringIO(); terminal(image, format, 20, stream)
                out = stream.getvalue()
                self.assertTrue(out)
                if format == 'ansi':
                    self.assertEqual(len(out.splitlines()), 5)
                    self.assertTrue(out.endswith('\x1b[0m\n'))
                elif format == 'iterm':
                    self.assertIn('1337;File=inline=1', out)
                else:
                    self.assertIn('\x1b_Ga=T,f=100,t=d', out)

    def test_ui_buffer_changes_invalidate_catalogue(self):
        from render_asset import fingerprint
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root/'Index').mkdir(); (root/'Data/Img').mkdir(parents=True)
            name = b'Img\\IconSrc.TIS'
            (root/'Index/TClientList.LST').write_bytes(struct.pack('<iii', 1, 1, len(name))+name)
            buffer = root/'Data/Img/IconSrc.TIS'; buffer.write_bytes(b'old')
            store = SimpleNamespace(assets=root, indexes={}, path=lambda p: root/p.replace('\\', '/'))
            before = fingerprint(store); buffer.write_bytes(b'changed')
            self.assertNotEqual(before, fingerprint(store))

    def test_installation_retries_after_failure(self):
        from asset_data import ROOT
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root/'tools/assets').mkdir(parents=True)
            env = root/'deps/python/assets'; (env/'bin').mkdir(parents=True)
            shutil.copy(ROOT/'tools/render-asset.sh', root/'tools/render-asset.sh')
            (root/'tools/assets/requirements.txt').write_text('test dependency')
            python = env/'bin/python'
            python.write_text("""#!/bin/sh
if [ "$1" = -m ]; then
  echo attempt >> "$(dirname "$0")/../attempts"
  if [ ! -f "$(dirname "$0")/../failed-once" ]; then
    touch "$(dirname "$0")/../failed-once"
    exit 1
  fi
fi
exit 0
""")
            python.chmod(0o755)
            command = ['sh', str(root/'tools/render-asset.sh'), '--help']
            self.assertEqual(subprocess.run(command).returncode, 1)
            self.assertFalse((env/'.dependencies-installed').exists())
            self.assertEqual(subprocess.run(command).returncode, 0)
            self.assertEqual(subprocess.run(command).returncode, 0)
            self.assertEqual((env/'attempts').read_text().splitlines(), ['attempt', 'attempt'])

    def test_malformed_data_is_bounded(self):
        with self.assertRaises(AssetError): Reader(b'\x00').get('I')
        with self.assertRaises(AssetError): Reader(b'').skip(-1)
        with self.assertRaises(AssetError): inflate(b'bad', 0)
        with self.assertRaises(AssetError): inflate(b'bad', 1)
        with self.assertRaises(AssetError): self.store.texture(-1)


if __name__ == '__main__':
    unittest.main()
