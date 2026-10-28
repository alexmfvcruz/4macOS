import subprocess
import tempfile
import unittest
from pathlib import Path

class TournamentHudTest(unittest.TestCase):
    def test_visibility_rule_under_sanitizers(self):
        source=Path(__file__).with_suffix('.cpp')
        with tempfile.TemporaryDirectory(prefix='4story tournament hud ') as folder:
            binary=Path(folder)/'test'
            subprocess.run(['clang++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(source),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__=='__main__':unittest.main()
