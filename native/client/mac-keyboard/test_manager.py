#!/usr/bin/env python3
"""Manager race/denial/state tests using compile-time fake HID and Cocoa shims."""
import os, subprocess, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'runtime/legacy/mac-keyboard-poc'
class ManagerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        OUT.mkdir(parents=True,exist_ok=True)
        flags=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-DFOURSTORY_KEYBOARD_PROBE_TEST']
        subprocess.run([*flags,str(Path(__file__).with_name('helper.cpp')),'-framework','CoreFoundation','-framework','IOKit','-o',str(OUT/'manager-test-helper')],check=True)
        subprocess.run([*flags,'-fobjc-arc','-DFOURSTORY_KEYBOARD_MANAGER_TEST',str(Path(__file__).with_name('manager.mm')),str(Path(__file__).with_name('manager_test.mm')),'-framework','AppKit','-framework','Carbon','-framework','ApplicationServices','-framework','IOKit','-o',str(OUT/'manager-test')],check=True)
    def run_commands(self,commands,original=0,trusted=True,helper=True):
        with tempfile.TemporaryDirectory(dir=OUT) as folder:
            path=Path(folder);state=path/'mode';state.write_text(str(original)+'\n')
            env=dict(os.environ,FOURSTORY_KEYBOARD_FAKE_STATE=str(state),FOURSTORY_KEYBOARD_TEST_LOCK=str(path/'lock'))
            if trusted:env['FOURSTORY_KEYBOARD_TEST_TRUSTED']='1'
            else:env.pop('FOURSTORY_KEYBOARD_TEST_TRUSTED',None)
            result=subprocess.run([str(OUT/'manager-test'),str(OUT/'manager-test-helper' if helper else path/'missing-helper')],input='\n'.join(commands+['quit'])+'\n',capture_output=True,text=True,env=env,timeout=25)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(state.read_text().strip(),str(original))
            return [int(x.split()[1]) for x in result.stdout.splitlines() if x.startswith('STATE ')]
    def test_verified_requires_all_twelve(self):
        self.assertEqual(self.run_commands(['toggle','partial','keys','toggle']),[2,2,3,0])
    def test_focus_restore_and_no_automatic_reenable(self):
        self.assertEqual(self.run_commands(['toggle','focus0','focus1']),[2,0,0])
    def test_pending_enable_cancelled_by_focus_loss(self):
        self.assertEqual(self.run_commands(['rapid','focus1']),[0,0])
    def test_original_enabled_is_preserved(self):
        self.assertEqual(self.run_commands(['toggle','keys','toggle'],original=1),[2,3,0])
    def test_permission_denied_changes_nothing(self):
        self.assertEqual(self.run_commands(['toggle'],trusted=False),[4])
    def test_missing_helper_changes_nothing(self):
        self.assertEqual(self.run_commands(['toggle'],helper=False),[4])
if __name__=='__main__':unittest.main(verbosity=2)
