import subprocess
import tempfile
import unittest
from pathlib import Path

class QuestScrollTest(unittest.TestCase):
    def test_real_extent_conversions_under_sanitizers(self):
        source=Path(__file__).with_suffix('.cpp')
        with tempfile.TemporaryDirectory(prefix='4story quest scroll ') as folder:
            binary=Path(folder)/'test'
            subprocess.run(['clang++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(source),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__=='__main__':unittest.main()
