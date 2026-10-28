"""Build label metadata: prompts, refusal without a terminal, stamp read-back."""
import io, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_metadata as m


class Terminal(io.StringIO):
    def __init__(self, tty): super().__init__(); self.tty = tty
    def isatty(self): return self.tty


class MetadataTest(unittest.TestCase):
    def test_flags_skip_the_prompt(self):
        def never(_): raise AssertionError('prompted')
        self.assertEqual(m.ask('beta', '0.1', Terminal(False), never), ('beta', '0.1'))

    def test_missing_values_are_refused_without_terminal(self):
        with self.assertRaises(ValueError): m.ask(None, '0.1', Terminal(False))
        with self.assertRaises(ValueError): m.ask('beta', None, Terminal(False))

    def test_prompt_repeats_until_valid(self):
        answers = iter(['release', 'Stable', 'v1', '1.2.3'])
        self.assertEqual(m.ask(None, None, Terminal(True), lambda _: next(answers)), ('stable', '1.2.3'))

    def test_bad_flag_values(self):
        for channel, version in [('main', '0.1'), ('beta', '0.1-rc'), ('beta', '.1'), ('beta', '')]:
            with self.assertRaises(ValueError): m.ask(channel, version, Terminal(False))

    def test_utc_format(self):
        self.assertRegex(m.utc_now(), r'^[0-9]{8}T[0-9]{6}Z$')

    def test_defines_and_stamp_round_trip(self):
        self.assertEqual(m.defines('beta', '0.1', '20261022T053524Z'),
                         ['-DFOURSTORY_BUILD_CHANNEL="beta"', '-DFOURSTORY_BUILD_VERSION="0.1"',
                          '-DFOURSTORY_BUILD_UTC="20261022T053524Z"'])
        with self.assertRaises(ValueError): m.defines('beta', '0.1', '2026-10-22')
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'exe'
            exe.write_bytes(b'\0junk FOURSTORY_BUILD_STAMP=dev|0.10.2|20261022T053524Z\0more')
            self.assertEqual(m.read_stamp(exe), ('dev', '0.10.2', '20261022T053524Z'))
            exe.write_bytes(b'no stamp')
            self.assertIsNone(m.read_stamp(exe))
            exe.write_bytes(b'FOURSTORY_BUILD_STAMP=dev|1|20261022T053524Z\0FOURSTORY_BUILD_STAMP=beta|1|20261022T053524Z\0')
            self.assertIsNone(m.read_stamp(exe))


if __name__ == '__main__':
    unittest.main()
