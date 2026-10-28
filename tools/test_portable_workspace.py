"""Checks for source archive boundaries and the client build selection."""
import argparse
import contextlib
import io
import os
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch
import workspace


class PortableWorkspaceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = (Path(self.temp.name)/'workspace with spaces').resolve()
        self.root.mkdir()
        self.root_patch = patch.object(workspace, 'ROOT', self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)

    def put(self, name):
        path = self.root/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(name)
        return path

    def entries(self):
        return {str(p.relative_to(self.root)) for p in workspace.archive_files()}

    def test_materializes_framework_directory_and_file_links(self):
        self.put('deps/framework/Versions/A/Resources/data')
        self.put('deps/framework/Versions/A/library')
        (self.root/'deps/framework/Versions/Current').symlink_to('A', target_is_directory=True)
        (self.root/'deps/framework/Resources').symlink_to('Versions/Current/Resources', target_is_directory=True)
        (self.root/'deps/framework/library').symlink_to('Versions/Current/library')
        self.assertIn('deps/framework/Resources/data', self.entries())
        self.assertIn('deps/framework/library', self.entries())

    def test_excludes_generated_and_private_state(self):
        self.put('.publication/local-audit.json')
        self.put('build/client/program')
        self.put('runtime/server/password')
        self.put('server/bin/TLoginSvr')
        self.put('game/TClient.log')
        self.put('game/LOL.txt')
        self.put('game/TString.tcd')
        self.assertEqual({'game/TString.tcd'}, self.entries())

    def test_rejects_external_link(self):
        (self.root/'outside').symlink_to(self.root.parent, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'external'):
            self.entries()

    def test_rejects_link_to_private_state(self):
        secret = self.put('runtime/secret')
        (self.root/'leak').symlink_to(secret)
        with self.assertRaisesRegex(RuntimeError, 'private'):
            self.entries()

    def test_rejects_directory_cycle(self):
        (self.root/'loop').symlink_to('.', target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'cycle'):
            self.entries()

    def test_checked_client_does_not_select_plain(self):
        args = argparse.Namespace(checks=True, seconds=0, server=None, port=4815)
        with patch.object(workspace, 'run') as run:
            workspace.client(args)
        self.assertNotIn('--plain', run.call_args.args[0])
        args.checks = False
        with patch.object(workspace, 'run') as run:
            workspace.client(args)
        self.assertIn('--plain', run.call_args.args[0])

    def test_archive_accepts_old_vendor_timestamps(self):
        path = self.put('deps/header.h')
        os.utime(path, (0, 0))
        archive = self.root.parent/'source.zip'
        with contextlib.redirect_stdout(io.StringIO()):
            workspace.archive(argparse.Namespace(output=archive))
        with zipfile.ZipFile(archive) as source:
            self.assertIsNone(source.testzip())
            self.assertEqual(source.read(self.root.name+'/deps/header.h'), b'deps/header.h')


if __name__ == '__main__':
    unittest.main()
