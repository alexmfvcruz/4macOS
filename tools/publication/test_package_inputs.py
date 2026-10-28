import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('package_inputs', Path(__file__).with_name('package_inputs.py'))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackageTests(unittest.TestCase):
    def test_input_subtrees_cannot_contain_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch.object(package, 'ROOT', root):
                for subtree in ('deps', 'game', 'server/database'):
                    with self.assertRaises(ValueError):
                        package.validate_destination(root/subtree/'packages')
                self.assertEqual(package.validate_destination((root/'.publication/packages').resolve()), (root/'.publication/packages').resolve())

    def test_sidecar_and_partial_outputs_are_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root/'packages'; output.mkdir()
            with patch.object(package, 'ROOT', root):
                for name in ('SHA256SUMS.txt', 'packages.json', '4macOS-dependencies.zip.partial'):
                    path = output/name; path.write_text('preserve')
                    with self.assertRaises(ValueError):
                        package.validate_destination(output)
                    self.assertEqual(path.read_text(), 'preserve')
                    path.unlink()


if __name__ == '__main__':
    unittest.main()
