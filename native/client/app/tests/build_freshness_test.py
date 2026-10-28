"""Source, transitive header, flags and packaging regressions; no game launch."""
import os, sys, tempfile, unittest, io, contextlib
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from build_freshness import RECORD, dependencies, record_build, validate_build, capture_inputs
import app_bundle
import permission_bundle
import run as client_run

class FreshnessTest(unittest.TestCase):
    def test_compiled_inputs_and_inventory(self):
        with tempfile.TemporaryDirectory(prefix='4story space ') as folder:
            out=Path(folder);src=out/'source.cpp';header=out/'header.h';flags=out/'unit.flags'
            for p in (src,header,flags):p.write_text('original')
            dep=out/'unit.d';dep.write_text('unit.o: '+str(src).replace(' ','\\ ')+ ' \\\n '+str(header).replace(' ','\\ ')+'\n')
            binary=out/'4story-client';binary.write_bytes(b'executable')
            record_build(out,[*dependencies(dep),flags],[out])
            validate_build(out)
            for changed in (src,header,flags):
                stat=changed.stat();changed.write_text('modified')
                with self.assertRaises(RuntimeError): validate_build(out)
                for module in (app_bundle,permission_bundle):
                    with self.assertRaises(RuntimeError):module.select_binary(binary)
                changed.write_text('original');os.utime(changed,ns=(stat.st_atime_ns,stat.st_mtime_ns))
                validate_build(out)
            (out/'new-unit.cpp').write_text('new unit')
            with self.assertRaises(RuntimeError):validate_build(out)
    def test_edits_during_compilation_cannot_be_blessed_by_later_link(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder);src=out/'source.cpp';header=out/'header.h'
            src.write_text('source');header.write_text('header')
            inputs=[src,header]
            snapshot=capture_inputs(inputs,[out])
            header.write_text('edited after this unit compiled')
            binary=out/'4story-client';binary.write_bytes(b'linked later')
            # The final binary is newer, but does not contain the edited header.
            with self.assertRaises(RuntimeError):record_build(out,inputs,[out],snapshot)
            snapshot=capture_inputs(inputs,[out])
            (out/'new.cpp').write_text('not present in the original jobs list')
            os.utime(binary,None)
            with self.assertRaises(RuntimeError):record_build(out,inputs,[out],snapshot)
            snapshot=capture_inputs(inputs,[out])
            late=out/'new-header.h';late.write_text('discovered too late')
            os.utime(binary,None)
            with self.assertRaises(RuntimeError):record_build(out,[*inputs,late],[out],snapshot)

    def test_cannot_package_old_or_unrecorded_binary(self):
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder);src=out/'source.cpp';src.write_text('source')
            binary=out/'4story-client';binary.write_bytes(b'executable')
            icon=out/'icon.icns';icon.write_bytes(b'icon')
            for module in (app_bundle,permission_bundle):
                with self.assertRaises(RuntimeError):module.select_binary(binary)
            with self.assertRaises(RuntimeError):app_bundle.package_client(out,icon)
            record_build(out,[src]);src.write_text('changed after link')
            with self.assertRaises(RuntimeError):app_bundle.package_client(out,icon)
            os.utime(binary,None);record_build(out,[src]);validate_build(out)
            (out/RECORD).write_text('{}') # start of failed/compile-only build
            with self.assertRaises(RuntimeError):app_bundle.package_client(out,icon)
            (out/RECORD).unlink()
            with self.assertRaises(RuntimeError):validate_build(out)
    def test_run_refuses_source_touched_after_build_before_launch(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);out=root/'build/client/c4-plain';out.mkdir(parents=True)
            src=root/'Client.cpp';src.write_text('compiled client source')
            for p in permission_bundle.paths(out):
                p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'data')
            record_build(out,[src])
            with patch.object(permission_bundle,'verify_signature'):
                permission_bundle.record_complete(out)
                os.utime(src,ns=(src.stat().st_atime_ns,src.stat().st_mtime_ns+1000000000))
                errors=io.StringIO()
                with patch.object(client_run,'ROOT',root), patch.object(sys,'argv',['run.py','--plain']), patch.object(client_run.subprocess,'Popen') as launch, contextlib.redirect_stderr(errors):
                    with self.assertRaises(SystemExit) as failure:client_run.main()
                    self.assertEqual(failure.exception.code,2)
                    launch.assert_not_called()
                self.assertIn('Rebuild with python3 native/client/app/build.py',errors.getvalue())

    def test_run_can_select_validated_isolated_build_without_replacing_live_output(self):
        with tempfile.TemporaryDirectory() as folder:
            candidate=Path(folder)/'candidate'
            errors=io.StringIO()
            with patch.dict(os.environ,{'FOURSTORY_CLIENT_OUT':str(candidate)}), patch.object(sys,'argv',['run.py','--plain']), patch.object(client_run,'select_binary',side_effect=RuntimeError('validation reached')) as select, patch.object(client_run.subprocess,'Popen') as launch, contextlib.redirect_stderr(errors):
                with self.assertRaises(SystemExit):client_run.main()
                select.assert_called_once_with(candidate.resolve()/'4story-client')
                launch.assert_not_called()
                self.assertIn('validation reached',errors.getvalue())

    def test_both_bundle_selectors_reject_touched_source_after_packaging(self):
        for module in (app_bundle,permission_bundle):
            with self.subTest(module=module.__name__), tempfile.TemporaryDirectory() as folder:
                out=Path(folder);src=out/'source.cpp';src.write_text('source')
                for p in module.paths(out):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'data')
                record_build(out,[src])
                with patch.object(module,'verify_signature'):
                    module.record_complete(out);module.select_binary(out/'4story-client')
                    os.utime(src,None)
                    with self.assertRaises(RuntimeError):module.select_binary(out/'4story-client')
if __name__=='__main__':unittest.main()
