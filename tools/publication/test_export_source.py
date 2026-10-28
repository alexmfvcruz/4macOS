"""Publication boundaries must hold even when local files look like source."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('export_source', Path(__file__).with_name('export_source.py'))
publication = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publication)


class PublicationTests(unittest.TestCase):
    def test_exported_build_all_omits_happy_and_remains_valid_python(self):
        workspace = Path(__file__).parents[1]/'workspace.py'
        data, changes = publication.scrub(workspace.read_bytes(), 'tools/workspace.py')
        compile(data, 'workspace.py', 'exec')
        self.assertNotIn(b"'happy'", data)
        self.assertNotIn(b'native/happy', data)
        self.assertIn(b"['accounts', 'server', 'client']", data)
        again, _ = publication.scrub(data, 'tools/workspace.py')
        self.assertEqual(data, again)

    def test_markdown_omits_removed_tool_section_and_keeps_other_sections(self):
        data = b'# Setup\n\nInstall does four things:\n4. Optional Happy Tool login.\n\n## Happy Tool\n\nOld instructions.\n\n## Play\n\nKeep this.\n'
        clean, _ = publication.scrub(data, 'example.md')
        self.assertNotIn(b'Happy', clean)
        self.assertNotIn(b'Old instructions', clean)
        self.assertIn(b'Install does three things:', clean)
        self.assertIn(b'## Play', clean)
        self.assertIn(b'Keep this.', clean)

    def test_source_and_required_shader_paths(self):
        for name in ('native/client/build/include/a.h', 'native/accounts/Cargo.lock',
                     'native/client/reference/shaders/manifest.json',
                     'native/client/reference/shaders/VS_MESH.vsh.bin',
                     publication.ORIGINAL+'/Client/TClient/TClient.vcxproj'):
            self.assertIsNone(publication.reason(Path(name)), name)

    def test_private_and_generated_inputs_never_selected(self):
        for name in ('runtime/server/config.json', 'deps/sdk/header.h', 'game/TString.tcd',
                     'server/database/TGLOBAL_GSP.bak', 'source/4Story 5.0 Database/db.bak',
                     'source/4Story 5.0 Registry and Services/settings.reg',
                     publication.ORIGINAL+'/Client/TClient/backup.bak',
                     publication.ORIGINAL+'/Lib/lib.lib',
                     'native/happy/Sources/HappyMac/main.swift',
                     'native/client/c3/baseline/receipt.json',
                     'native/client/reference/c6/compare/field.json',
                     'tools/server/equip_omunk.py', 'tools/database/restore_local.py',
                     'native/client/__pycache__/file.pyc'):
            self.assertIsNotNone(publication.reason(Path(name)), name)

    def test_known_credentials_are_checked_in_all_encodings(self):
        value = 'fixture-secret-value'
        for encoding in ('utf-8', 'utf-16-le', 'utf-16-be'):
            self.assertIn('known local credential', publication.scan(value.encode(encoding), {value}))
        self.assertIn('private key', publication.scan(b'-----BEGIN RSA ' + b'PRIVATE KEY-----', set()))

    def test_symlinked_selected_root_is_excluded_and_documents_are_scanned(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base/'workspace'; root.mkdir()
            helper = root/'tools/publication'; helper.mkdir(parents=True)
            marker = '-----BEGIN RSA ' + 'PRIVATE KEY-----'
            (helper/'SOURCE-README.md').write_text(marker)
            (helper/'source.gitignore').write_text('/build/\n')
            outside = base/'external'; outside.mkdir()
            (outside/'private.cpp').write_text('must stay outside')
            (root/'native').symlink_to(outside, target_is_directory=True)
            output = root/'.publication/source'
            result = publication.export(root, output, root/'.publication/report.json')
            self.assertFalse((output/'native/private.cpp').exists())
            self.assertEqual(result['status'], 'scan-blocked')
            self.assertTrue(any(f['path'] == 'README.md' for f in result['findings']))

    def test_export_retains_original_and_excludes_escaping_links(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)/'workspace'; root.mkdir()
            helper = root/'tools/publication'; helper.mkdir(parents=True)
            for name in ('SOURCE-README.md', 'source.gitignore'):
                (helper/name).write_text('fixture\n')
            src = root/'native/client/example.cpp'; src.parent.mkdir(parents=True)
            data = b'// 192.' + b'168.178.76\n'; src.write_bytes(data)
            (src.parent/'external.h').symlink_to('/etc/hosts')
            excluded = root/'source/4Story 5.0 Registry and Services/test.reg'
            excluded.parent.mkdir(parents=True); excluded.write_text('"Password"="fixture-secret"')
            output = root/'.publication/source'; report = root/'.publication/report.json'
            result = publication.export(root, output, report)
            self.assertEqual(result['findings'], [])
            self.assertEqual(src.read_bytes(), data)
            self.assertEqual((output/'native/client/example.cpp').read_bytes(), b'// 192.0.2.1\n')
            self.assertFalse((output/'native/client/external.h').exists())
            self.assertFalse((output/'source/4Story 5.0 Registry and Services/test.reg').exists())
            self.assertEqual(result['known_credential_values_checked'], 1)
            with self.assertRaises(ValueError):
                publication.export(root, output, report)


if __name__ == '__main__':
    unittest.main()
